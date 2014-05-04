#include <stdio.h>
#include "mfs.h"

#define BUFFER_SIZE (4096)
char buffer[BUFFER_SIZE];

int
main(int argc, char *argv[])
{
    int rc = MFS_Init("mumble-20.cs.wisc.edu", 11614);

    printf("CLIENT:: about to send message (%d)\n", rc);
    char message[BUFFER_SIZE];
    sprintf(message, "hello world");
    //rc = UDP_Write(sd, &saddr, message, BUFFER_SIZE);
    // printf("CLIENT:: sent message (%d)\n", rc);
    rc = MFS_Write(1, message, 1);
    //if (rc == 0) {
	//struct sockaddr_in raddr;
	//int rc = UDP_Read(sd, &raddr, buffer, BUFFER_SIZE);
	//printf("CLIENT:: read %d bytes (message: '%s')\n", rc, buffer);
   // }

    return 0;
}


