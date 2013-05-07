#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>

#include "http.h"

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define BLOCK_SIZE 64*1024*1024

struct memory_block *blocks;

// returns contents of file
char* getfile(char* filename){
	char buffer[MAXLINE] = "\0";
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	/*
	char lf[2] = "\n\0";
	*/
	bzero(buffer, MAXLINE);

	//get file with absolute path of filename
	FILE *fp = NULL;
	if ((fp = fopen(filename, "r"))) {
		while ((read = getline(&line, &len, fp)) != -1) {
			//strcat(line, lf);
			strcat(buffer, line);
		}

		fclose(fp);
	}
	else {
		sprintf(buffer, "File Not Found\n");
	}
	//printf("%s", buffer);
	return strdup(buffer);
}

// Serves it via json format
char* loadavg()
{
	char buffer[MAXLINE];
	char json[MAXLINE];
	char str2[200];
	bzero(buffer, MAXLINE);
	bzero(json, MAXLINE);
	bzero(str2, 200);
	char *load1min = NULL, *load5min = NULL, *load15min = NULL,
	     *running = NULL, *total = NULL, *last=NULL;

	FILE *file;
	file = fopen("/proc/loadavg", "r");

	// Stores file in buffer space.
	fgets (buffer, MAXLINE, file);

	// File is loaded to buffer space, so no need to keep it open
	fclose (file);

	// Reads /proc/loadavg (well, the buffer)

	load1min = buffer;
	strtok_r(load1min, " ", &load5min);
	strtok_r(load5min, " ", &load15min);
	strtok_r(load15min, " ", &running);
	strtok_r(running, "/", &total);
	strtok_r(total, " ", &last);

	// Now create json
	sprintf(str2, "{\"total_threads\": ");
	strcat(json, str2);

	sprintf(str2, "\"%s\", \"loadavg\": [", total);
	strcat(json, str2);

	sprintf(str2, "\"%s\", ", load1min);
	strcat(json, str2);

	sprintf(str2, "\"%s\", ", load5min);
	strcat(json, str2);

	sprintf(str2, "\"%s\"], ", load15min);
	strcat(json, str2);

	sprintf(str2, "\"running_threads\": ");
	strcat(json, str2);

	sprintf(str2, "\"%s\"}", running);
	strcat(json, str2);

	//Add $json to the output
	return strdup(json);
}

// Returns JSON output of /proc/meminfo
char* meminfo ()
{
	char json[MAXLINE] = "{\0", str2[100], line[100];
	int x = 0;

	FILE *file;
	file = fopen("/proc/meminfo", "r");

	// Converts /proc/meminfo to json
	while (fscanf(file, "%s", line) != EOF)
	{
		if (strcmp(line, "kB") == 0) //==0 if strings are equal
		{
			if (fscanf(file, "%s", line) == EOF)
				break;
		}

		if (x++ != 0)
		{
			sprintf(str2, ", ");
			strcat(json, str2);
		}

		char *progressptr;
		char *next = strtok_r(line, ":", & progressptr);

		sprintf(str2, "\"%s\": ", next);
		strcat(json, str2);

		fscanf(file, "%s", line);

		sprintf(str2, "\"%s\"", line);
		strcat(json, str2);
	}

	fclose(file);

	sprintf(str2, "}");
	strcat(json, str2);

	// Add $json to output
	return strdup(json);
}

// This /only/ measures how long this process has been up for, and not the host
// machine. Reasoning is since this is the "server" we don't want the host.
long int uptime() 
{
	return clock() / (CLOCKS_PER_SEC) / 1000;
}

// Busy spins for 15s
void loop()
{
	// Server needs to respond to this request as soon as it is received
	long end = uptime() + 15000;
	while (uptime() < end){};
}

char *allocanon() 
{
	struct memory_block *block = malloc(sizeof(struct memory_block));

	int i;
	char *str = "\0";

	// Use mmap to allocate the anon file
	block->address = mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	for (i = 1; i < BLOCK_SIZE-1; i += 100)
		((char *) block->address)[i] = 'x';
	block->next = blocks;
	blocks = block;

	sprintf(str, "\"completed\": \"1\", \"address\": \"%p\"}", block->address);
	return strdup(str);
}

char *freeanon()
{

	struct memory_block *block = blocks;
	char *str = NULL;

	if (blocks) 
	{
		blocks=blocks->next;
		munmap(block->address, BLOCK_SIZE);
		sprintf(str, "\"completed\": \"1\"}");
	}
	else
	{
		sprintf(str, "\"completed\": \"0\"}");
	}

	return strdup(str);

}
