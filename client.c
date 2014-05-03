#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)

//struct sockaddr_in saddr;
//struct sockaddr_in raddr;
//int sd;


int
main(int argc, char *argv[])
{
    int rc = MFS_Init("mumble-19.cs.wisc.edu", 11614);
    assert(rc == 0);
    printf("CLIENT:: about to send message (%d)\n", rc);
    char message[BUFFER_SIZE];
    sprintf(message, "hello world");
    rc = MFS_Write(1, message, 1);
    if(rc == -1) printf("FAIL");
    return 0;
}


