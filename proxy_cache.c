#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define MAX_OBJECT_COUNT 10
#define LRU_MAGIC_NUM 20
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/75.0\r\n";
static const char *conn_hdr = "Connection:close\r\n";
static const char *proxy_hdr = "Proxy-Connection:close\r\n";
static const char *host_hdr_format = "Host:%s\r\n";
static const char *requestline_hdr_format = "GET %s HTTP/1.1\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key = "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void* thread(void* vargp);

void doit(int fd);
void parse_uri(char *uri, char* hostname, char* path, char* port);
void build_http_headers(char* http_headers, char* hostname, char* path, char* port, rio_t* client_rio);
int conn_endserver(char* hostname, char* port);

/* Cache structure and operations */
typedef struct {
    char uri[MAXLINE];
    char object[MAX_OBJECT_SIZE];
    
    int lru;
    int isEmpty;

    int writecnt;
    sem_t cache_mutex, writecnt_mutex;
}Cache_object;

typedef struct {
    Cache_object buf[MAX_OBJECT_COUNT];
}Cache;

void cache_init();
void read_prev(int idx);
void read_after(int idx);
void write_prev(int idx);
void write_after(int idx);

int cache_find(char* uri);

void cache_lru(int idx);//update every objs lru num
void cache_update(char* uri, char* buf);
int cache_eviction();//find lru obj and move it out

Cache cache;

int main(int argc, char ** argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    /* check command-line args */
    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    cache_init();
    listenfd = Open_listenfd(argv[1]);
    while(1){
	clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
	Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
	printf("Accepted connection from (%s, %s)\n", hostname, port);
	pthread_create(&tid, NULL, thread, (void *)connfd);
    }
    exit(0);
}

void* thread(void* vargp)//thread routine
{
    int connfd = (int ) vargp;
    pthread_detach(pthread_self());
    doit(connfd);
    Close(connfd);
}

void doit(int fd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
    char http_hdrs[MAXLINE];
    rio_t conn_rio, serv_rio;
    int serverfd;
    size_t n;

    Rio_readinitb(&conn_rio, fd);
    Rio_readlineb(&conn_rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    /* Parse URI from request */
    parse_uri(uri, hostname, path, port);
    printf("hostname:%s  path:%s\n", hostname, path);
    if(strcasecmp(method, "GET")){
        printf("Proxy does not implement this method");
	return;
    }

    int cache_idx = cache_find(uri);
    /* If cache hit */
    if(cache_idx != -1){
	printf("Cache hit\n");
        write_prev(cache_idx);
	Rio_writen(fd, cache.buf[cache_idx].object, strlen(cache.buf[cache_idx].object));
	write_after(cache_idx);
	return;
    }

    printf("Cache miss\n");
    /* If cache miss */
    /* Build http request and headers */
    build_http_headers(http_hdrs, hostname, path, port, &conn_rio);

    /* Connect end server */
    serverfd = Open_clientfd(hostname, port);
    printf("Connect to endserver success\n");

    /* Send request and headers to end server */
    Rio_readinitb(&serv_rio, serverfd);
    Rio_writen(serverfd, http_hdrs, strlen(http_hdrs));
    printf("HTTP headers:\n");
    printf("%s", http_hdrs);
    printf("Send request to endserver success\n");

    /* Receive message from end server and send to client */
    size_t buf_size = 0;
    char cachebuf[MAX_OBJECT_SIZE];
    while((n = Rio_readlineb(&serv_rio, buf, MAXLINE)) != 0){
	//printf("Proxy received %d bytes\n", (int) n);
	buf_size += n;
	strcat(cachebuf, buf);
	printf("%s",buf);
	Rio_writen(serverfd, buf, strlen(buf));
    }

    printf("Receive result from endserver finished\n");

    if(buf_size <= MAX_OBJECT_SIZE)
        cache_update(uri, cachebuf);

    Close(serverfd);
}

void parse_uri(char *uri, char* hostname, char* path, char* port)
{
    int port_i; //port in integer
    char *pos1 = strstr(uri, "//");

    pos1 = (pos1!=NULL) ? pos1+2 : uri;

    char *pos2 = strstr(pos1, ":");//have port number or not.

    if(pos2!=NULL){
        *pos2 = '\0';
	sscanf(pos1, "%s", hostname);
	sscanf(pos2, "%d%s", &port_i, path);
	sprintf(port, "%d", port_i);
    }
    else{
	sprintf(port, "80");
        pos2 = strstr(pos1, "/");
	if(pos2!=NULL){
	    *pos2 = '\0';
	    sscanf(pos1, "%s", hostname);
	    *pos2 = '/';
	    sscanf(pos2, "%s", path);
	}
	else{
	    sscanf(pos1, "%s", hostname);
	}
    }
    return;
}

void build_http_headers(char* http_headers, char* hostname, char* path, char* port, rio_t* client_rio)
{
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
    /* request line*/
    sprintf(request_hdr, requestline_hdr_format, path);
    /* get other headers from client_rio and change it */
    while(Rio_readlineb(client_rio, buf, MAXLINE)){
        if(!strcmp(buf, endof_hdr)) break;//EOF

	if(!strncasecmp(buf, host_key, strlen(host_key))){//Host
	    strcpy(host_hdr, buf);
	    continue;
	}

        if(strncasecmp(buf, connection_key, strlen(connection_key))
	    && strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))
	    && strncasecmp(buf, user_agent_key, strlen(user_agent_key))){
	    strcat(other_hdr, buf);
	    //continue;
	}
    }

    if(strlen(host_hdr) == 0){
        sprintf(host_hdr, host_hdr_format, hostname);
    }

    sprintf(http_headers, "%s%s%s%s%s%s%s",
		    request_hdr,
		    host_hdr,
		    conn_hdr,
		    proxy_hdr,
		    user_agent_hdr,
		    other_hdr,
		    endof_hdr);
    printf("Other headers:\n%s", other_hdr);
}

/* Cche operatons */
void cache_init()
{
    for(int i = 0; i < MAX_OBJECT_COUNT; i++){
        cache.buf[i].isEmpty = 1;
        cache.buf[i].lru = 0;
        cache.buf[i].writecnt = 0;
	Sem_init(&cache.buf[i].cache_mutex, 0, 1);
	Sem_init(&cache.buf[i].writecnt_mutex, 0, 1);
    }
}

void read_prev(int idx)
{
    P(&cache.buf[idx].cache_mutex);
}

void read_after(int idx)
{
    V(&cache.buf[idx].cache_mutex);
}
void write_prev(int idx)
{
    printf("In write_prev\n");
    P(&cache.buf[idx].writecnt_mutex);
    cache.buf[idx].writecnt++;
    if(cache.buf[idx].writecnt == 1) P(&cache.buf[idx].cache_mutex);
    V(&cache.buf[idx].writecnt_mutex);
}
void write_after(int idx)
{
    printf("In write_after\n");
    P(&cache.buf[idx].writecnt_mutex);
    cache.buf[idx].writecnt--;
    if(cache.buf[idx].writecnt == 0) V(&cache.buf[idx].cache_mutex);
    V(&cache.buf[idx].writecnt_mutex);
}

int cache_find(char* uri)
{
    int i;
    for(i = 0; i < MAX_OBJECT_COUNT; i++){
        read_prev(i);
	if(!cache.buf[i].isEmpty && !strcmp(uri, cache.buf[i].uri)){
	    read_after(i);
	    break;
	}
	read_after(i);
    }
    return (i == MAX_OBJECT_COUNT) ? -1 : i;
}

void cache_lru(int idx)//update every objs lru num except the new one
{
    int i;
    for(i = 0; i < idx; i++){
        write_prev(i);
	if(!cache.buf[i].isEmpty)
	    cache.buf[i].lru--;
	write_after(i);
    }
    i++;
    for(; i < MAX_OBJECT_COUNT; i++){ 
        write_prev(i);
	if(!cache.buf[i].isEmpty)
	    cache.buf[i].lru--;
	write_after(i);
    }
}

void cache_update(char* uri, char* buf)
{
    int idx = cache_eviction();

    write_prev(idx);
    strcpy(cache.buf[idx].uri, uri);
    strcpy(cache.buf[idx].object, buf);
    cache.buf[idx].isEmpty = 0;
    cache.buf[idx].lru = LRU_MAGIC_NUM;
    write_after(idx);
}

int cache_eviction()//find empty or lru obj and return its index
{
    int minidx;
    int min_lru = LRU_MAGIC_NUM;

    for(int i = 0; i < MAX_OBJECT_COUNT; i++){
        read_prev(i);
	if(cache.buf[i].isEmpty){//find empty object
	    minidx = i;
            read_after(i);
	    break;
	}
	if(cache.buf[i].lru < min_lru){//find lru object
	    minidx = i;
	    read_after(i);
	    continue;
	}
    }

    return minidx;
}
