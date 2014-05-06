#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)

//global variables
int imageFD;

int DEBUG = 0;
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
	//int offset; //offset of this iMap (may help in testing)
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
struct dirBlock//block of directory entries
{
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

		//CR initialized by default, but create map and node and initialize them all to "null"
		struct iMap map1;
		struct iNode node1;
		struct dirBlock dirBlock1;

		//"Unused entries in the inode map and unused direct pointers
		//in the inodes should have the value 0."
		//unused directory entries have value -1
		int i;
		for(i=0; i<256; i++)//initalize CR
		{
			CR.maps[i] = sizeof(struct checkRegion) + i*sizeof(struct iMap);
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
			dirBlock1.entries[i].inum = -1;
		}




		// | CR | M_all | D0 | I0 | M0 | EOL


		//write initial CR
		CR.EOL = 0;
		pwrite(imageFD, &CR, sizeof(struct checkRegion), CR.EOL);
		CR.EOL = sizeof(struct checkRegion);

		//write initial map (as pieces)
		for(i=0; i<256; i++)
		{
			pwrite(imageFD, &CR.maps[i], sizeof(struct iMap), CR.EOL);
			CR.EOL += sizeof(struct iMap);
		}
		//populate and write directory block
		dirBlock1.entries[0].inum = 0;//set . and .. , parent is same as current for root
		dirBlock1.entries[1].inum = 0;

		char nameIn[60];
		strcpy(nameIn, ".");
		strcpy(dirBlock1.entries[0].name, nameIn);
		strcpy(nameIn, "..");
		strcpy(dirBlock1.entries[1].name, nameIn);

		pwrite(imageFD, &dirBlock1, MFS_BLOCK_SIZE, CR.EOL);

		node1.blocks[0] = CR.EOL;//set iNode pointer to this block

		CR.EOL += MFS_BLOCK_SIZE;

		//populate iNode
		node1.stats.type = MFS_DIRECTORY;//set type to directory entry
		node1.stats.size = MFS_BLOCK_SIZE ; // set size to default block size

		pwrite(imageFD, &node1, sizeof(struct iNode), CR.EOL); //write iNode

		map1.nodes[0] = CR.EOL;//set iMap pointer
		CR.EOL += sizeof(struct iNode);//advance past iNode

		//write new map piece
		pwrite(imageFD, &map1, sizeof(struct iMap), CR.EOL);
		CR.maps[0] = CR.EOL; //update CR pointer to new iMap

		//update EOL to point at very end of file
		CR.EOL += sizeof(struct iMap);

		pwrite(imageFD, &CR, sizeof(struct checkRegion), 0); //write CR with correct EOL

		fsync(imageFD);
	}
	else //image exists, read in CR to memory
	{

		pread(imageFD, &CR.EOL, sizeof(int), 0);//read in value of EOL
		int i;
		for(i=0; i<256; i++)
		{
			pread(imageFD, &CR.maps[i], sizeof(int), (i+1) * sizeof(int));
			//printf("bytes read %d at offset %d \n ", check, i+1 * sizeof(int));
		}
		//printf("pants");
	}
	fsync(imageFD);
	close(imageFD);
	return 0;
}

int server_lookup(int pinum, char *name)
{
	return 0;
}

int server_stat(int inum, MFS_Stat_t m)
{
	return 0;
}

int server_write(int inum, char *buffer, int block)
{
	return 0;
}


int server_read(int inum, char *buffer, int block)
{

	close(imageFD);

	return 0;
}

int server_creat(int pinum, int type, char *name)
{
	return 0;
}

int server_unlink(int pinum, char *name)
{
	return 0;
}

int server_shutdown()
{
	fsync(imageFD);
	close(imageFD);
	exit(0);
	return 0;
}

//unpack the package_t file sent by the client library, call method, return success/fail to main
int unpack(Package_t buff)
{
	int returnCode;

	switch(buff.requestType)
	{
	case LOOKUP_REQUEST:
		returnCode = server_lookup(buff.pinum, buff.name);
		break;

	case STAT_REQUEST:
		returnCode = server_stat(buff.inum, buff.m);
		break;

	case WRITE_REQUEST:
		returnCode = server_write(buff.inum, buff.buffer, buff.block);
		break;

	case READ_REQUEST:
		returnCode = server_read(buff.inum, buff.buffer, buff.block);
		break;

	case CREAT_REQUEST:
		returnCode = server_creat(buff.inum, buff.type, buff.name);
		break;

	case UNLINK_REQUEST:
		returnCode = server_unlink(buff.pinum, buff.name);
		break;

	case SHUTDOWN_REQUEST:
		returnCode = server_shutdown();
		break;
	}
	return returnCode;
}




int
main(int argc, char *argv[])
{


	//check for errors  prompt> server [portnum] [file-system-image]
	if(argc != 3)
	{
		printf("Usage: server [port-number] [file-system-image]");
		exit(0);
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
		Package_t buffer;
		int rc = UDP_Read(sd, &s, &buffer, BUFFER_SIZE);




		if (rc > 0) {
			int unpacked = unpack(buffer);//helper method to decode package_t and call proper server method
			if (unpacked < 0)
			{
				//i don't know what would happen here
			}

			printf("                                SERVER:: read %d bytes (message: '%s')\n", rc, buffer.name);
			//Package_t reply;
			strcpy(buffer.name, "Reply");
			rc = UDP_Write(sd, &s, &buffer, BUFFER_SIZE);
		}
	}

	return 0;
}


