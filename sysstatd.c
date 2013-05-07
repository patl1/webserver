#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "threadpool.h"

#define MAX_THREADS 100
#define MSG_SIZE 500000

#include "http.h"

int done[MAX_THREADS];			//1000 == max number of threads
struct future * f[MAX_THREADS];	
char path[200];

/* Helper function to print error messages. */
void error(const char* msg){
	perror(msg);
	exit(1);
}

/* Data to be passed to callable. */
struct callable_data {
    int number;
    int newsockfd;
};

/* 
 * A callable. 
 *
 * Returns a string that reports its number, which thread
 * executes the task, and the start and end time of the 
 * task's execution.
 */
static void callable_task(struct callable_data * callable){
  // printf("connection accepted, callable_task() run. (i=%i)\n",callable->number);
   // Service HTTP Request Here: 
   char buf[MSG_SIZE], response[MSG_SIZE];
   bzero(buf, MSG_SIZE); 		//clear buffer
   int once = 1;
   int offset = 0;
   while(once && read(callable->newsockfd, &buf[offset], MSG_SIZE-1-offset) > 0){
	// error == 1 --> Invalid URL
   	// error == 2 --> Invalid character in callback
   	// error == 3 --> METHOD NOT ALLOWED
	int error_code = 0;
  	
	bzero(response, MSG_SIZE);		//clear buffer

	// Output message:
   	//printf("Message Received: %s\n", buf);
	offset = strlen(buf);
	char* str;

	int i = 0, nNewlines = 0;
   	for(i=0; i<MSG_SIZE && buf[i]; ++i){
		if (buf[i] == '\n') nNewlines += 1;
	}
	if (nNewlines < 3) continue;
   	once = 0;

	// Parse message:
   	// buf == "GET _____  HTTP/1.1 .... 
   	if (strncmp(&buf[0], "GET ", 4)){
		error_code = 3;
		str = malloc(sizeof(char)); 
	}
   	else if (! (strncmp(&buf[4], "/loadavg ", 9) && strncmp(&buf[4], "/loadavg?", 9))){
		//print loadavg info:
		str = loadavg();
	   }
	   else if (! (strncmp(&buf[4], "/meminfo ", 9) && strncmp(&buf[4], "/meminfo?", 9))){
		//print meminfo info:
		str = meminfo();
	   }
	   else if (!strncmp(&buf[4], "/files", 6) ){
		char filename[250]; 		//file to serve
		bzero(filename, 250);
		strncpy(filename, path, strlen(path));
		strcpy(&filename[strlen(filename)], &buf[10]);	

		int i;
		for(i=0; i<250; ++i){
			//check for ".."--> throw error
			if (!strncmp(&filename[i], "..", 2)) {error_code = 4; break;}
			//' ' --> end of filename
			if (filename[i] == ' ') {filename[i] = '\0'; break;}
		}

		// PRINTS OUT FILE NAME
		//printf("\nFilename: %s\n", filename);

		//serve file
		//str = getfile("/etc/adjtime");
		str = getfile(filename);
		//printf("\nstr: %s\n", str);
		//filetype = ____;
	}
	else{
    
		error_code = 1; 
		str = malloc(sizeof(char)); 
	}

	   int callback_begin = 0;
	   int callback_end = 0; 
	   for(callback_begin=12; buf[callback_begin] && !callback_end && !error_code; ++callback_begin){
		if (buf[callback_begin] == '?' || buf[callback_begin] == '&'){
		    callback_begin += 1;
		    if (!strncmp(&buf[callback_begin], "callback=", 9)){
			callback_begin = callback_begin + 9;
			for(callback_end = callback_begin; buf[callback_end] != ' ' && 
							buf[callback_end] != '?' && 
							buf[callback_end] != '&' && 
							!error_code; ++callback_end){
			    // check for invalid characters:
			    error_code = 2;
			    if (buf[callback_end] < 97 || buf[callback_end] > 122) error_code = 0;
			    else if (buf[callback_end] < 65 || buf[callback_end] > 90) error_code = 0;
			    else if (buf[callback_end] < 30 || buf[callback_end] > 39) error_code = 0;
			    else if (buf[callback_end] == 46 || buf[callback_end] == 95) error_code = 0;
			}
			callback_end += 2;
		    }
		}
   	    }	
   
  	 // Determine response:
  	 if (error_code){
		bzero(response, MSG_SIZE);
		if (error_code == 3) strcpy(&response[0], "HTTP/1.1 405 Method Not Allowed\r\n");
		else strcpy(&response[0], "HTTP/1.1 404 Not Found\r\n");
		strcpy(&response[strlen(&response[0])], "\r\n");
	   }
	   else if (callback_end){

		strcpy(&response[strlen(&response[0])], "HTTP/1.1 200 OK\r\n");
		strcpy(&response[strlen(&response[0])], "Content-Type: text/html\r\n");
		strcpy(&response[strlen(&response[0])], "\r\n");
		strncpy(&response[strlen(&response[0])], &buf[callback_begin-1], callback_end-callback_begin-1);
		strcpy(&response[strlen(&response[0])], "(");
		strcpy(&response[strlen(&response[0])], str);
		strcpy(&response[strlen(&response[0])], ")");
	   }
	else {
		//no callback
		strcpy(&response[0], "HTTP/1.1 200 OK\r\n");
		strcpy(&response[strlen(&response[0])], "Content-Type: text/html\r\n");
		strcpy(&response[strlen(&response[0])], "\r\n");
		strcpy(&response[strlen(&response[0])], str);
	}

	//free str
	free(str);

	   // Send response:
	   //printf("Sending Response:\n%s\n", &response[0]);
	   int n = write(callable->newsockfd, &response[0], strlen(&response[0]));
	   if (n < 0) error("ERROR writing to socket");
   
	bzero(buf, MSG_SIZE); 		//clear buffer
   }	
   close(callable->newsockfd);		//close the fd
   future_free(f[callable->number]);	//free the future
   done[callable->number] = 1; 		//mark as finished
   
   return;
}

int main(int argc, char *argv[]){
    // parse command line arguments:
    int ind;
    int portno = -1; 
    bzero(path, 200);
    for(ind = 1; ind + 1 < argc; ++ind){   
	if (strcmp(argv[ind], "-p") == 0){
		ind += 1;
		portno = atoi(argv[ind]);
	}
	else if(strcmp(argv[ind], "-r") == 0){
		;//to be implemented...
	}
	else if(strcmp(argv[ind], "-R") == 0){
		ind += 1;
		strcpy(path, argv[ind]);
	}
	else{
		error("invalid argument found!\n-p _portno_\n\n");
	}
    }
    if (portno == -1) error("port must be assigned with '-p'\n");

    // Basic data declarations:
    int nthreads = MAX_THREADS;
    int sockfd, newsockfd, i;
    socklen_t clilen;
    ///int done[nthreads];
    struct sockaddr_in serv_addr, cli_addr;
    struct thread_pool * ex = thread_pool_new(nthreads);
    
    // Sleep .5 seconds to give threads time to start up:
    struct timespec sleep_for = { .tv_sec = 0, .tv_nsec = 5*1e8 };
    nanosleep(&sleep_for, NULL);

    // Set up sockets & address structures:
    sockfd = socket(AF_INET, SOCK_STREAM, 0);      //open socket
    if (sockfd < 0) error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr)); //zero-out buffer
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    clilen = sizeof(cli_addr);

    // Bind socket to listening address & port:
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    // Zero out done[]:
    for(i=0; i<nthreads; ++i) done[i] = 1;

    //printf("starting to listen for incoming connections...\n");
    // Begin listening & endlessly look for incoming connections
    listen(sockfd,5);
    for(i=0; 1; i=(i+1)%nthreads){	
	//keep trying a new i until we reach a thread # that isn't busy:
	if (done[i] == 0) continue;

	//blocks until connection: 
	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	if (newsockfd < 0) error("ERROR on accept");
        
	//submit thread to threadpool:
	struct callable_data * callable_data = malloc(sizeof *callable_data);
        callable_data->number = i;
	callable_data->newsockfd = newsockfd;
        done[i] = 0;
	f[i] = thread_pool_submit(ex, 
                               (thread_pool_callable_func_t) callable_task, 
                               callable_data);
    }
    close(sockfd);
    thread_pool_shutdown(ex);
    return EXIT_FAILURE; //we should never reach this point
}

