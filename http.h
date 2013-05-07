#define MAXLINE 500000


// LL structure for the memory blocks alloc/free by {alloc/free}anon
struct memory_block {
	struct memory_block *next;
	void *address;
};

// Serves loadavg in json format
char* loadavg();

// Serves meminfo in json format
char* meminfo ();

// Gets the current uptime of the PROCESS
long int uptime();

// Runs an empty loop
// Just done to increase load on server
void loop();

// Allocates a 64MB block of anonymous data
char *allocanon();

// Frees any blocks that has been allocated with allocanon
char *freeanon();

// Gets the File contents and stores to bufferspace
// returns the bufferspace and file contents
char *getfile(char*);
