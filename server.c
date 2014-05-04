#include <stdio.h>
#include "udp.h"

#define BUFFER_SIZE (4096)

//global variables
int imageFD;
/*
iNode contents: size, type(file or dir), 14 "pointers"(int offsets)
iMap contents: ID 0-255, 16 pointers to iNodes
4096 iNodes = 256 iMap pieces
*/
struct checkRegion
{
	int EOL; //end of log pointer
	int maps[256]; //offset locations of iMaps

};
struct iMap
{
	int nodes[16];//offset locations of iNodes
};
struct iNode
{
	int size;
	int type; // f1le = 1, direct0ry = 0;
	int blocks[14];

};

int server_init(char *image)
{
	//try to open image, create if it does not exist
	imageFD = open(&image, O_RDWR);
	if(imageFD < 0)
	{
		imageFD = open(image, O_RDWR| O_CREAT| O_TRUNC, S_IRWXU);

		//setup checkpoint region

		//Setup empty root directory
	}

	return 0;
}

int
main(int argc, char *argv[])
{

	//check for errors  prompt> server [portnum] [file-system-image]
	if(argc != 3)
	{
		printf("Usage: server [port-number] [file-system-image]");
	}

	//open port
	int port = atoi(argv[1]);
    int sd = UDP_Open(port);
    assert(sd > -1);

    char *imageIn = (char*)argv[2];

    server_init(imageIn);

    printf("                                SERVER:: waiting in loop\n");

    while (1) {
	struct sockaddr_in s;
	char buffer[BUFFER_SIZE];
	int rc = UDP_Read(sd, &s, buffer, BUFFER_SIZE);
	if (rc > 0) {
	    printf("                                SERVER:: read %d bytes (message: '%s')\n", rc, buffer);
	    char reply[BUFFER_SIZE];
	    sprintf(reply, "reply");
	    rc = UDP_Write(sd, &s, reply, BUFFER_SIZE);
	}
    }

    return 0;
}


