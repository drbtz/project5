#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)

//global variables
int imageFD;
struct sockaddr_in s;
int sd;
Package_t *buffer;
int newInodeCount;

int DEBUG = 1;
/*
iNode contents: size, type(file or dir), 14 "pointers"(int offsets)
iMap contents: ID 0-255, 16 pointers to iNodes
4096 iNodes = 256 iMap pieces

//size of CR = 279556
//size of iMap = 64
//size of iNode = 1088
 */
struct dirBlock//block of directory entries
{
	MFS_DirEnt_t entries[64];
};
struct iNode
{
	//size of file, miltiple of 4096 for directories. for files, offset of last non-zero byte
	//stats.type = f11e(1) or direct0ry(0)
	MFS_Stat_t stats;
	//int size;
	int blocks[14];
	//struct dirBlock memBlocks[14];
	//int offset; //offset of this iNode (may help in testing)
};
struct iMap //contains up to 16 pointers to iNodes
{
	int nodes[16];//offset locations of iNodes
	struct iNode memNodes[16];
	//int offset; //offset of this iMap (may help in testing)
};
struct checkRegion //contains up to 256 pointers to iMap pieces
{
	int EOL; //end of log pointer
	int maps[256]; //offset locations of iMaps
	struct iMap memMaps[256];

} CR;
struct holderNode//used to hold 1 directory iNode and all its possible contents
{
	struct dirBlock memBlocks[14];
	int offsets[14];
}holder;

//helper routines----------------------------------------------------
void dirInit(struct dirBlock *newDir)
{
	int i;
	for(i=0; i<64; i++)//initialize directory block
	{
		newDir->entries[i].inum = -1;
	}
}
void nodeInit(struct iNode *new)
{
	int i;
	for(i=0; i<14; i++)//initialize directory block
	{
		new->blocks[i] = 0;
	}
}
void mapInit(struct iMap *new)
{
	int i;
	for(i=0; i<16; i++)//initialize directory block
	{
		new->nodes[i] = 0;
	}
}

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

		//size of CR = 279556
		//size of iMap = 64
		//size of iNode = 1088

		//"Unused entries in the inode map and unused direct pointers
		//in the inodes should have the value 0."
		//unused directory entries have value -1
		int i, j;
		for(i=0; i<256; i++)//initalize CR
		{
			CR.maps[i] = sizeof(struct checkRegion) + i*sizeof(struct iMap);
			for(j=0; j<16; j++)
			{
				CR.memMaps[i].nodes[j] = 0;
			}
		}
		mapInit(&map1);
		nodeInit(&node1);
		dirInit(&dirBlock1);

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
		map1.memNodes[0] = node1;//store node in mem structure
		CR.EOL += sizeof(struct iNode);//advance past iNode

		//write new map piece
		pwrite(imageFD, &map1, sizeof(struct iMap), CR.EOL);
		CR.memMaps[0] = map1;//store map in mem struct
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
		for(i=0; i<256; i++)//read map pieces into
		{
			pread(imageFD, &CR.maps[i], sizeof(int)+sizeof(struct iMap), (i+1) * (sizeof(int)+sizeof(struct iMap)));
			//printf("bytes read %d at offset %d \n ", check, i+1 * sizeof(int));
		}
	}
	fsync(imageFD);
	//close(imageFD);
	return 0;
}
//MFS_Lookup() takes the parent inode number (which should be the inode number of a directory)
//and looks up the entry name in it. The inode number of name is returned.
//Success: return inode number of name; failure: return -1.
//Failure modes: invalid pinum, name does not exist in pinum.
//int pinum, char *name
int server_lookup(Package_t *packIn)
{
	if(packIn->pinum<0 || packIn->pinum > 4095)//check if its in range
	{
		packIn->result = -1;
		return -1;
	}
	//lookup
	struct iNode look = CR.memMaps[packIn->pinum/16].memNodes[packIn->pinum%16];
	int type = look.stats.type;
	if(type == MFS_REGULAR_FILE)//if its a file
	{
		packIn->result = -1;
		return -2;
	}
	else if(type == MFS_DIRECTORY)//if its a directory
	{
		int i, j;

		//lookup 1: look.blocks[0] = 558084
		//lookup 2: look.blocks[0] =

		/*
		 * issue:  holder contains the correct data after 1st look up.  Creat, makes the file correctly
		 * but either fails to save it properly(offset pointer) or, the 2nd lookup is reading the wrong place.
		 * correct data exists in holder, but is overwritten on 2nd lookup.
		 *
		 * try clearing contents of holder after write to disk and then tracking where 2nd lookup reads from
		 * it should be the same location as creat saved
		 */


		for(i=0; i<14; i++)//TODO after creat, the new name "test", should be found in here.
		{
			//read a whole directory iNode into memory (possibly 14 blocks of 64 directories)
			pread(imageFD, &holder.memBlocks[i], sizeof(struct dirBlock), look.blocks[i]);
			//also save offset to corresponding block  TODO???


		}
		//scan over all possible valid entries in this iNode(now in memory)
		for(i=0; i<14; i++)
		{
			for(j=0; j<64; j++)
			{
				if(strcmp(holder.memBlocks[i].entries[j].name, packIn->name) == 0)
				{
					//compare to names and save and return the corresponding iNode# if found
					packIn->result = holder.memBlocks[i].entries[j].inum;
					return holder.memBlocks[i].entries[j].inum;
				}
			}
		}
		packIn->result = -1;//the name was not found
		return -1;
	}
	else//type not file or directory
	{
		packIn->result = -1;
		return -1;
	}

	//check if node is a directory
	return 0;
}

//int inum, MFS_Stat_t m
int server_stat(Package_t *packIn)
{
	return 0;
}
//int inum, char *buffer, int block
int server_write(Package_t *packIn)
{
	return 0;
}

//int inum, char *buffer, int block
int server_read(Package_t *packIn)
{

	close(imageFD);

	return 0;
}
//MFS_Creat() makes a file ( type == MFS_REGULAR_FILE) or directory ( type == MFS_DIRECTORY)
//in the parent directory specified by pinum of name name .
//Returns 0 on success, -1 on failure.
//Failure modes: pinum does not exist, or name is too long.
//If name already exists, return success (think about why).
//int pinum, int type, char *name
int server_creat(Package_t *packIn)
{
	//name length check already done in MFS_lib
	//printf("type %d \n", type);

	if(packIn->pinum<0 || packIn->pinum>4095)//check validity
	{
		return -1;
	}
	int parent = server_lookup(packIn);
	if(parent > -1)//if files exists, we don't need to create it
	{
		packIn->result = 0;
		return 0;
	}
	else if(parent == -2)//pinum is a regular file, can not create IN a file(only in DIR)
	{
		packIn->result = -1;
		return -1;
	}
	else if(parent == -1)//parent DIR exists, name does not
	{
		struct iNode *newNode;
		newNode = malloc(sizeof(struct iNode));
		nodeInit(newNode);

		//scan CR to find empty iNode
		int i, j;
		int eye, jay;
		for(i=0; i<256; i++)
		{
			for(j=0; j<16; j++)//there should be a better way to do this
			{
				if(CR.memMaps[i].nodes[j] == 0)
				{
					newNode = &CR.memMaps[i].memNodes[j];
					eye = i;
					jay = j;
					i = 256;
					j = 16;
				}
			}
		}
		i = eye;
		j = jay;
		nodeInit(newNode);
		//create new based on type
		if(packIn->type == 1) //its a file
		{
			newNode->stats.type = MFS_REGULAR_FILE;
			newNode->stats.size = 0;




			//access to name, type, pinum
			int a, b;
			for(a=0; a<14; a++)
			{
				for(b=0; b<64; b++)
				{
					if(holder.memBlocks[a].entries[b].inum == -1)
					{
						//setnam and inum, write block, inc EOL
						strcpy(holder.memBlocks[a].entries[b].name, packIn->name);
						holder.memBlocks[a].entries[b].inum = i*16 + j;
						pwrite(imageFD, &holder.memBlocks[a], sizeof(MFS_BLOCK_SIZE), CR.EOL);
						newNode->blocks[0] = CR.EOL;
						CR.EOL += MFS_BLOCK_SIZE;
						a = 14;
						b = 64;
						break;
					}
				}
			}

			pwrite(imageFD, &newNode, sizeof(struct iNode), CR.EOL); //write iNode

			CR.memMaps[i].nodes[j] = CR.EOL;//set iMap pointer
			//map1.memNodes[0] = node1;//store node in mem structure
			CR.EOL += sizeof(struct iNode);//advance past iNode

			//write new map piece
			pwrite(imageFD, &CR.memMaps[i], sizeof(struct iMap), CR.EOL);
			//CR.memMaps[0] = map1;//store map in mem struct
			CR.maps[i] = CR.EOL; //update CR pointer to new iMap

			//update EOL to point at very end of file
			CR.EOL += sizeof(struct iMap);

			pwrite(imageFD, &CR, sizeof(struct checkRegion), 0); //sync Mem CR with disk CR

			fsync(imageFD);

		}
		else if(packIn->type == 0)//its a directory
		{
			newNode->stats.type = MFS_DIRECTORY;
			newNode->stats.size = MFS_BLOCK_SIZE;
			//setup directory
			struct dirBlock newDir;
			dirInit(&newDir);

			//inum is based off the first empty index into our 2D array
			newDir.entries[0].inum = i*16 + j;//set . and ..
			newDir.entries[1].inum = packIn->pinum;

			char nameIn[60];//assign self DIR and parent DIR
			strcpy(nameIn, ".");
			strcpy(newDir.entries[0].name, nameIn);
			strcpy(nameIn, "..");
			strcpy(newDir.entries[1].name, nameIn);

			pwrite(imageFD, &newDir, MFS_BLOCK_SIZE, CR.EOL);

			newNode->blocks[0] = CR.EOL;//set iNode pointer to this block
			CR.EOL += MFS_BLOCK_SIZE;

			//populate iNode
			newNode->stats.type = MFS_DIRECTORY;//set type to directory entry
			newNode->stats.size = MFS_BLOCK_SIZE ; // set size to default block size

			pwrite(imageFD, &newNode, sizeof(struct iNode), CR.EOL); //write iNode

			CR.memMaps[i].nodes[j] = CR.EOL;//set iMap pointer
			//map1.memNodes[0] = node1;//store node in mem structure
			CR.EOL += sizeof(struct iNode);//advance past iNode

			//write new map piece
			pwrite(imageFD, &CR.memMaps[i], sizeof(struct iMap), CR.EOL);
			//CR.memMaps[0] = map1;//store map in mem struct
			CR.maps[i] = CR.EOL; //update CR pointer to new iMap

			//update EOL to point at very end of file
			CR.EOL += sizeof(struct iMap);

			pwrite(imageFD, &CR, sizeof(struct checkRegion), 0); //sync Mem CR with disk CR

			fsync(imageFD);
		}
		else//something is fucked!!!
		{
			printf("something is fucked in server_creat, DIR exists\n");
			packIn->result = -1;
			return -1;
		}
	}
	else
	{
		printf("something is fucked in server_creat\n");
		packIn->result = -1;
		return -1;
	}



	//packIn->result = 0;
	return 0;
}
//int pinum, char *name
int server_unlink(Package_t *packIn)
{
	return 0;
}

int server_shutdown()
{
	fsync(imageFD);
	close(imageFD);
	buffer->result = 0;
	UDP_Write(sd, &s, buffer, sizeof(Package_t));
	exit(0);
	return 0;
}

//unpack the package_t file sent by the client library, call method, return success/fail to main
int unpack(Package_t *buff)
{
	int returnCode;

	switch(buff->requestType)
	{
	case LOOKUP_REQUEST:
		returnCode = server_lookup(buff);
		break;

	case STAT_REQUEST:
		returnCode = server_stat(buff);
		break;

	case WRITE_REQUEST:
		returnCode = server_write(buff);
		break;

	case READ_REQUEST:
		returnCode = server_read(buff);
		break;

	case CREAT_REQUEST:
		returnCode = server_creat(buff);
		break;

	case UNLINK_REQUEST:
		returnCode = server_unlink(buff);
		break;

	case SHUTDOWN_REQUEST:
		printf("before server_shutdown\n");
		returnCode = server_shutdown();
		printf("after server_shutdown\n");
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
		printf("Usage: server [port-number] [file-system-image]\n");
		exit(-1);
	}

	//open port
	int port = atoi(argv[1]);
	sd = UDP_Open(port);
	assert(sd > -1);



	char *imageIn = (char*)argv[2];
	buffer = malloc(sizeof(Package_t));

	server_init(imageIn);

	if(DEBUG)
	{
		buffer->block = 0;              //int The block of the inode
		strcpy(buffer->buffer, "pants");//char[] buffer for read request from client
		buffer->inum = 0;               //int The inode number
		strcpy(buffer->name, "test");  // char[] The file name
		buffer->pinum = 0;              //int  The parent inode number
		buffer->requestType = 0;        //int Request being sent to server(read, write, etc)
		buffer->result = 0;             //int The result of the inode(file or directory)
		buffer->type = 1;               //int The type of the request sent to server

		MFS_Stat_t test;                //MFS_Stat_t Used by MFS_Stat for formatting
		test.size = 0;                  //size of file, %block size for directories, otherwise offset to last non-zero byte in data
		test.type = 0;                  // 0 = directory, 1 = file
		buffer->m = test;

		server_creat(buffer);

		buffer->block = 0; 			    //int The block of the inode
		strcpy(buffer->buffer, "pants");//char[] buffer for read request from client
		buffer->inum = 0;			    //int The inode number
		strcpy(buffer->name, "test");   // char[] The file name
		buffer->pinum = 0; 			    //int  The parent inode number
		buffer->requestType = 0;		//int Request being sent to server(read, write, etc)
		buffer->result = 0; 			//int The result of the request sent to server
		buffer->type = 0; 				//int The type of the inode(file or directory)

		test.size = 0;					//size of file, %block size for directories, otherwise offset to last non-zero byte in data
		test.type = 0;					// 0 = directory, 1 = file
		buffer->m = test;

		server_lookup(buffer);

	}

	if(!DEBUG)
	{
		printf("                                SERVER:: waiting in loop\n");

		while (1) {

			int rc = UDP_Read(sd, &s, buffer, sizeof(Package_t));

			if (rc > 0)
			{
				unpack(buffer);//helper method to decode package_t and call proper server method

				printf("                                SERVER:: read %d bytes (message: '%s')\n", rc, buffer->name);
				//Package_t reply;
				strcpy(buffer->name, "Reply");
				rc = UDP_Write(sd, &s, buffer, sizeof(Package_t));
			}
		}
	}
	return 0;
}


