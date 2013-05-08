/* server.h */
#ifndef __SERVER_H__
#define __SERVER_H__

#include "rio.h"
#include "list.h"

#define THREADS 20
#define MAXLINE 8192
#define MAXBUF  8192

extern char **environ; /* defined by libc */
struct list memory_list;

struct memory {
    void *block;
    struct list_elem elem;
};

static void process_http(int fd);
static void run_loop(void);
ssize_t read_requesthdrs(rio_t *rp);
void response_ok(int fd, char *msg, char *content_type, char *version);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg, char *version);
static size_t parse_callback(char *uri, char *callback);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);


#endif /* server.h */
