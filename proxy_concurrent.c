#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

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

    printf("%s", user_agent_hdr);

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
    while((n = Rio_readlineb(&serv_rio, buf, MAXLINE)) != 0){
	//printf("Proxy received %d bytes\n", (int) n);
	printf("%s",buf);
	Rio_writen(serverfd, buf, n);
    }

    printf("Receive result from endserver finished\n");

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

int conn_endserver(char* hostname, char* port)
{
    
}
