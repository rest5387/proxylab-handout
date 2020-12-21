# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy proxy_concurrent proxy_cache

concurrent: proxy_concurrent

cache: proxy_cache

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o
	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

proxy_concurrent.o: proxy_concurrent.c csapp.h
	$(CC) $(CFLAGS) -c proxy_concurrent.c

proxy_concurrent: proxy_concurrent.o csapp.o
	$(CC) $(CFLAGS) proxy_concurrent.o csapp.o -o proxy_concurrent $(LDFLAGS)

proxy_cache.o: proxy_cache.c csapp.h
	$(CC) $(CFLAGS) -c proxy_cache.c

proxy_cache: proxy_cache.o csapp.o
	$(CC) $(CFLAGS) proxy_cache.o csapp.o -o proxy_cache $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o proxy  proxy_concurrent proxy_cache core *.tar *.zip *.gzip *.bzip *.gz

