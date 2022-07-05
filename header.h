#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>


#define CLIENT_FIFO_TEMPLATE "/tmp/seqnum_cl.%ld"
/* Template for building client FIFO name */
#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE) + 30)
/* Space required for client FIFO pathname */


struct request { /* Request (client --> server) */
	pid_t pid; /* PID of client */
	char reqMatrix[10000];
	int n;
};


struct response { /* Response (server --> client) */
	int invertable; /* Start of sequence */
};