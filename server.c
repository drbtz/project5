#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)

//global variables
int imageFD;
/*
iNode contents: size, type(file or dir), 14 "pointers"(int offsets)
iMap contents: ID 0-255, 16 pointers to iNodes
4096 iNodes = 256 iMap pieces
*/
struct checkRegion //contains up to 256 pointers to iMap pieces
{
	int EOL; //end of log pointer
	int maps[256]; //offset locations of iMaps

} CR;
struct iMap //contains up to 16 pointers to iNodes
{
	int nodes[16];//offset locations of iNodes
	int offset; //offset of this iMap (may help in testing)
};
struct iNode
{
	//size of file, miltiple of 4096 for directories. for files, offset of last non-zero byte
	//stats.type = f11e(1) or direct0ry(0)
	MFS_Stat_t stats;
	//int size;
	int blocks[14];
	//int offset; //offset of this iNode (may help in testing)

};
struct dBlock//block of directory entries
{
	MFS_Stat_t stats;
	MFS_DirEnt_t entries[64];

};

//routine to open server disk image, or create one if it doesnt exist
//build CR or read it in off existing image.
int server_init(char *image)
{
	//try to open image
	imageFD = open(image, O_RDWR);

	if(imageFD < 0)//does not exist
	{	//create new image
		imageFD = open(image, O_RDWR| O_CREAT| O_TRUNC, S_IRWXU);

		//setup checkpoint region**************************************
		//first update log end pointer
		CR.EOL = sizeof(CR) + sizeof(struct iMap) + sizeof(struct iNode);

		//CR initialized by default, but create map and node and initialize them all to "null"
		struct iMap map1;
		struct iNode node1;
		struct dBlock dBlock1;

		node1.stats.type = 0;//set type to directory entry
		node1.stats.size = 4096; // set size to default block size

		//"Unused entries in the inode map and unused direct pointers
		//in the inodes should have the value 0."
		//unused directory entries have value -1
		int i;
		for(i=0; i<256; i++)//initalize CR
		{
			CR.maps[i] = 0;
		}
		for(i=0; i<16; i++)//initialize first map piece
		{
			map1.nodes[i] = 0;
		}
		for(i=0; i<14; i++)//initialize inode
		{
			node1.blocks[i] = 0;
		}
		for(i=0; i<64; i++)//initialize directory block
		{
			dBlock1.entries[i].inum = -1;
		}

		//Setup empty root directory
		dBlock1.entries[0].inum = 0;

		dBlock1.entries[0].name[0] = ".";

		//set all pointers properly

		//write file image

	}
	else //image exists, read in CR to memory
	{

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


