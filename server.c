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
struct iNode *parentInode; //used in creat, when we make a new file we have to update parent iNode and map

int DEBUG = 0;
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
	MFS_DirEnt_t dirEntry[64];
};
struct iNode
{
	//size of file, multiple of 4096 for directories. for files, offset of last non-zero byte
	//stats.type = f11e(1) or direct0ry(0)
	MFS_Stat_t nodeStats;
	//int size;
	int blockOffset[14];
	//struct dirBlock memBlocks[14];
	//int offset; //offset of this iNode (may help in testing)
};
struct iMap //contains up to 16 pointers to iNodes
{
	int nodeOffset[16];//offset locations of iNodes
	struct iNode memNodes[16];
	//int offset; //offset of this iMap (may help in testing)
};
struct checkRegion //contains up to 256 pointers to iMap pieces
{
	int EOL; //end of log pointer
	int mapOffset[256]; //offset locations of iMaps
	struct iMap memMaps[256];

} CR;
struct holderNode//used to hold 1 directory iNode and all its possible contents
{
	struct dirBlock memBlocks[14];
	//int offsets[14];
}inMemoryDataBlock;

//helper routines----------------------------------------------------
void dirInit(struct dirBlock *newDir)
{
	int i;
	for(i=0; i<64; i++)//initialize directory block
	{
		newDir->dirEntry[i].inum = -1;
		memset(newDir->dirEntry[i].name, 0, 60);

	}
}
void nodeInit(struct iNode *new)
{
	int i;
	for(i=0; i<14; i++)//initialize directory block
	{
		new->blockOffset[i] = 0;
	}
}
void mapInit(struct iMap *new)
{
	int i;
	for(i=0; i<16; i++)//initialize directory block
	{
		new->nodeOffset[i] = 0;
	}
}
int inumValidate(int inum)
{
	if(CR.memMaps[inum/16].nodeOffset[inum%16] == 0)
	{
		return -1;
	}
	return 0;
}
//scans the parent directory before creat to ensure there is space for the file
int creatValidate()
{
	if(parentInode->nodeStats.size > (895 * (int)sizeof(MFS_DirEnt_t)))
	{
		return -1;
	}

	//	int i, j;
	//	for(i=0; i<14; i++)
	//	{
	//		for(j=0; j<64; j++)
	//		{
	//			if(inMemoryDataBlock.memBlocks[i].dirEntry[j].inum == -1)
	//			{
	//				return 0; //if it finds a space, we can return success
	//			}
	//		}
	//	}
	//no spaces, return failure before creating anything
	return 0;
}
//-------------------------------------------------------------------

//routine to open server disk image, or create one if it doesnt exist
//build CR or read it in off existing image.
int server_init(char *image)
{
	/*	printf("size of dirBlock: %d\n", sizeof(struct dirBlock));
	printf("size of iNode: %d\n", sizeof(struct iNode));
	printf("size of iMap: %d\n", sizeof(struct iMap));
	printf("size of CR: %d\n", sizeof( CR));
	printf("size of holderNode: %d\n", sizeof(struct holderNode));
	 */
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
			CR.mapOffset[i] = sizeof(struct checkRegion) + i*sizeof(struct iMap);
			for(j=0; j<16; j++)
			{
				CR.memMaps[i].nodeOffset[j] = 0;
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
			pwrite(imageFD, &CR.mapOffset[i], sizeof(struct iMap), CR.EOL);
			CR.EOL += sizeof(struct iMap);
		}
		//populate and write directory block

		char nameIn[60];
		strcpy(nameIn, ".");
		strcpy(dirBlock1.dirEntry[0].name, nameIn);
		dirBlock1.dirEntry[0].inum = 0;


		strcpy(nameIn, "..");
		strcpy(dirBlock1.dirEntry[1].name, nameIn);
		dirBlock1.dirEntry[1].inum = 0;

		pwrite(imageFD, &dirBlock1, MFS_BLOCK_SIZE, CR.EOL);

		node1.blockOffset[0] = CR.EOL;//set iNode pointer to this block
		CR.EOL += MFS_BLOCK_SIZE;

		//populate iNode
		node1.nodeStats.type = MFS_DIRECTORY;//set type to directory entry
		node1.nodeStats.size = 2 * sizeof(MFS_DirEnt_t); // size is 2 entries

		pwrite(imageFD, &node1, sizeof(struct iNode), CR.EOL); //write iNode

		map1.nodeOffset[0] = CR.EOL;//set iMap pointer
		map1.memNodes[0] = node1;//store node in mem structure
		CR.EOL += sizeof(struct iNode);//advance past iNode

		//write new map piece
		pwrite(imageFD, &map1, sizeof(struct iMap), CR.EOL);
		CR.memMaps[0] = map1;//store map in mem struct
		CR.mapOffset[0] = CR.EOL; //update CR pointer to new iMap

		//update EOL to point at very end of file
		CR.EOL += sizeof(struct iMap);

		pwrite(imageFD, &CR, sizeof(struct checkRegion), 0); //write CR with correct EOL

		fsync(imageFD);
	}
	else //image exists, read in CR to memory
	{
		//fill CR with data
		pread(imageFD, &CR, sizeof(struct checkRegion), 0);

		//for each map piece

		//		int i;
		//		for(i = 0; i < 256; i++)
		//		{
		//			//read in each map from its offset into file
		//			pread(imageFD, &CR.memMaps[i], sizeof(struct iMap), CR.mapOffset[i]);
		//
		//			//for each iNode in the map
		//			int j;
		//			for(j = 0; j < 16; j++)
		//			{
		//				//If the offset is not 0, then there is a node to load into mem
		//				if(CR.memMaps[i].nodeOffset[j] != 0)
		//				{
		//					pread(imageFD, &CR.memMaps[i].memNodes[j], sizeof(struct iNode), CR.memMaps[i].nodeOffset[j]);
		//				}
		//			}
		//		}

		//read in EOL from file
		//		pread(imageFD, &CR.EOL, sizeof(int), 0);
		//
		//		//assemble the in memory file system
		//		//maybe not this hard
		//		int i;
		//		for(i=0; i<256; i++)//
		//		{
		//			//read in map piece offset into memory
		//			pread(imageFD, &CR.mapOffset[i], sizeof(int), (i+1) * (sizeof(int)));
		//
		//			//if the offset is not zero, then there is a map at the offset to read in
		//			//this SHOULD be moot, because the whole map is created during init
		//			if(CR.mapOffset[i])
		//			{
		//				pread(imageFD, &CR.memMaps[i], sizeof(iMap), CR.mapOffset[i]);
		//				int j;
		//
		//				//for each iNode in the map
		//				for(j = 0; j < 16; j++)
		//				{
		//					//read each node offset into iMap
		//					pread(imageFD, &CR.memMaps[i].nodeOffset[j], sizeof(int), CR.mapOffset[i]);
		//
		//				}
		//			}
		//			//printf("bytes read %d at offset %d \n ", check, i+1 * sizeof(int));
		//		}

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
	//error check
	if(packIn->pinum<0 || packIn->pinum > 4095)//check if its in range
	{
		printf("failed in lookup1 check");
		packIn->result = -1;
		return -1;
	}
	if(inumValidate(packIn->pinum) == -1)
	{
		printf("failed in lookup2 check\n");
		packIn->result = -1;
		return -1;
	}
	//lookup
	parentInode = &CR.memMaps[packIn->pinum/16].memNodes[packIn->pinum%16];
	int type = parentInode->nodeStats.type;
	if(type == MFS_REGULAR_FILE)//if its a file, can not lookup in a file
	{
		printf("failed in lookup3 check\n");
		packIn->result = -1;
		return -2;
	}
	else if(type == MFS_DIRECTORY)//if its a directory
	{
		int i, j;
		for(i=0; i<14; i++)
		{
			//read whole parent directory iNode into memory (possibly 14 blocks of 64 directories)
			pread(imageFD, &inMemoryDataBlock.memBlocks[i], sizeof(struct dirBlock), parentInode->blockOffset[i]);
		}
		//scan over all possible valid entries in this iNode(now in memory)
		for(i=0; i<14; i++)
		{
			for(j=0; j<64; j++)
			{
				if(strcmp(inMemoryDataBlock.memBlocks[i].dirEntry[j].name, packIn->name) == 0)
				{
					//compare to names and save and return the corresponding iNode# if found
					packIn->result = inMemoryDataBlock.memBlocks[i].dirEntry[j].inum;
					return inMemoryDataBlock.memBlocks[i].dirEntry[j].inum;
				}
			}
		}
		//printf("failed in lookup4 check\n");
		packIn->result = -1;//the name was not found
		return -1;
	}
	else//type not file or directory
	{
		printf("failed in lookup5 check\n");
		packIn->result = -1;
		return -1;
	}

	//check if node is a directory
	return 0;
}

//returns some information about the file specified by inum. Upon success, return 0, otherwise -1.
//The exact info returned is defined by MFS_Stat_t. Failure modes: inum does not exist.
//int inum, MFS_Stat_t m
int server_stat(Package_t *packIn)
{
	if(inumValidate(packIn->inum) == -1)
	{
		packIn->result = -1;
		return -1;
	}

	struct iNode thisNode = CR.memMaps[packIn->inum/16].memNodes[packIn->inum%16];

	packIn->m = thisNode.nodeStats;
	//packIn->m.size = MFS_BLOCK_SIZE;
	//packIn->m.type = MFS_REGULAR_FILE;

	packIn->result = 0;
	return 0;
}

//writes a block of size 4096 bytes at the block offset specified by block .
//Returns 0 on success, -1 on failure. Failure modes: invalid inum, invalid block,
//not a regular file (because you can't write to directories).
//int inum, char *buffer, int block
int server_write(Package_t *packIn)
{
	//error checks
	//invalid iNum
	if(packIn->inum <0 || packIn->inum >4095)
	{
		packIn->result = -1;
		return -1;
	}
	if(inumValidate(packIn->inum) == -1)
	{
		packIn->result = -1;
		return -1;
	}
	if(packIn->block <0 || packIn->block >13)
	{
		packIn->result = -1;
		return -1;
	}

	struct iNode *thisNode = &CR.memMaps[packIn->inum/16].memNodes[packIn->inum%16];
	if(thisNode->nodeStats.type == MFS_DIRECTORY)//not a file
	{
		packIn->result = -1;
		return -1;
	}



	pwrite(imageFD, packIn->buffer, MFS_BLOCK_SIZE, CR.EOL);
	CR.memMaps[packIn->inum/16].memNodes[packIn->inum%16].blockOffset[packIn->block] = CR.EOL;//point inMem parent iNode at new block
	CR.EOL += MFS_BLOCK_SIZE;//point past written block

	//Before iNode get written, we must update the size
	int sizeCount = 13;
	while(thisNode->blockOffset[sizeCount]== 0)
	{
		sizeCount--;
	}
	thisNode->nodeStats.size = MFS_BLOCK_SIZE * (sizeCount + 1);


	pwrite(imageFD, &CR.memMaps[packIn->inum/16].memNodes[packIn->inum%16], sizeof(struct iNode), CR.EOL); //write iNode
	CR.memMaps[packIn->inum/16].nodeOffset[packIn->inum%16] = CR.EOL;//set inMem iMap pointer to new node
	CR.EOL += sizeof(struct iNode);//advance past iNode

	pwrite(imageFD, &CR.memMaps[packIn->inum/16], sizeof(struct iMap), CR.EOL);
	CR.mapOffset[packIn->inum/16] = CR.EOL; //update CR pointer to new iMap

	//update EOL to point at very end of file
	CR.EOL += sizeof(struct iMap);

	pwrite(imageFD, &CR, sizeof(struct checkRegion), 0); //sync Mem CR with disk CR
	fsync(imageFD);//push all to disk

	packIn->result = 0;
	return 0;
}
//reads a block specified by block into the buffer from file specified by inum .
//The routine should work for either a file or directory; directories should return data
//in the format specified by MFS_DirEnt_t.
//Success: 0, failure: -1. Failure modes: invalid inum, invalid block.
//int inum, char *buffer, int block
int server_read(Package_t *packIn)
{
	if(inumValidate(packIn->inum) == -1)
	{
		packIn->result = -1;
		return -1;
	}

	struct iNode thisNode = CR.memMaps[packIn->inum/16].memNodes[packIn->inum%16];

	if(thisNode.blockOffset[packIn->block] == 0)
	{
		packIn->result = -1;
		return -1;
	}

	pread(imageFD, packIn->buffer, MFS_BLOCK_SIZE, thisNode.blockOffset[packIn->block]);


	packIn->result = 0;
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
	//error checks, name length is already handled in MFS client
	if(packIn->pinum<0 || packIn->pinum>4095)//check validity
	{
		printf("failed in valid check\n");
		packIn->result = -1;
		return -1;
	}
	if(inumValidate(packIn->pinum) == -1)
	{
		printf("failed in iNum check\n");
		packIn->result = -1;
		return -1;
	}

	int parent = server_lookup(packIn);
	if(parent > -1)//if files exists, we don't need to create it
	{
		printf("failed in lookup check\n");
		packIn->result = 0;
		return 0;
	}
	else if(parent == -2)//pinum is a regular file, can not create IN a file(only in DIR)
	{
		printf("failed in file check\n");
		packIn->result = -1;
		return -1;
	}
	else if(parent == -1)//parent DIR exists, name(file or DIR) does not
	{
		//validate that the new file can fit in DIR
		int validDir = creatValidate();
		if(validDir == -1)
		{
			printf("failed in fit check\n");
			packIn->result = -1;
			return -1;
		}

		struct iNode *newNode;//new node for new file
		newNode = malloc(sizeof(struct iNode));
		nodeInit(newNode);

		//scan CR to find empty iNode in a iMap piece
		int i, j, eye, jay;
		for(i=0; i<256; i++)
		{
			for(j=0; j<16; j++)//there should be a better way to do this
			{

				if(CR.memMaps[i].nodeOffset[j] == 0)
				{
					//found first empty space, point to it, so it updates in memory
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
		if(packIn->type == 1) //we are creating a new FILE
		{
			//files have no initial size, just an iNode
			newNode->nodeStats.type = MFS_REGULAR_FILE;
			newNode->nodeStats.size = 0;

			//here we find a space in the current directory
			//TODO if DIR block is full, we must bring a new one online.
			int a, b, a2, b2;
			a=0;

			if(parentInode->nodeStats.size % 4096 == 0)
			{
				//find location of new block
				a = parentInode->nodeStats.size/4096;
				dirInit(&inMemoryDataBlock.memBlocks[a]);//init the next DIR block
			}
			for(a=0; a<14; a++)
			{
				for(b=0; b<64; b++)
				{
					if(inMemoryDataBlock.memBlocks[a].dirEntry[b].inum == -1)
					{
						//update info for new directory entry
						strcpy(inMemoryDataBlock.memBlocks[a].dirEntry[b].name, packIn->name);//set name in holder
						inMemoryDataBlock.memBlocks[a].dirEntry[b].inum = i*16 + j;           //set iNode in holder
						parentInode->nodeStats.size += sizeof(MFS_DirEnt_t);

						//write new file to EOL (no data since it is empty file)
						pwrite(imageFD, &newNode, sizeof(struct iNode), CR.EOL); //write iNode
						CR.memMaps[i].nodeOffset[j] = CR.EOL;//set iMap pointer to new node

						CR.EOL += sizeof(struct iNode);//advance past iNode

						//write new map piece
						pwrite(imageFD, &CR.memMaps[i], sizeof(struct iMap), CR.EOL);
						CR.mapOffset[i] = CR.EOL; //update CR pointer to new iMap
						//update EOL to point at very end of file
						CR.EOL += sizeof(struct iMap);

						a2=a;
						b2=b;
						b=64;
						a=16;
						break;
					}
				}
			}
			a=a2;
			b=b2;//this is the DIR entry


			pwrite(imageFD, &inMemoryDataBlock.memBlocks[a], sizeof(struct dirBlock), CR.EOL);//write updated DIR block
			parentInode->blockOffset[a] = CR.EOL;//point inMem parent iNode at new block
			CR.EOL += MFS_BLOCK_SIZE;//point past written block

			pwrite(imageFD, &parentInode, sizeof(struct iNode), CR.EOL); //write iNode
			CR.memMaps[packIn->pinum/16].nodeOffset[packIn->pinum%16] = CR.EOL;//set inMem iMap pointer to new node
			CR.EOL += sizeof(struct iNode);//advance past iNode

			pwrite(imageFD, &CR.memMaps[packIn->pinum/16], sizeof(struct iMap), CR.EOL);
			CR.mapOffset[packIn->pinum/16] = CR.EOL; //update CR pointer to new iMap

			//update EOL to point at very end of file
			CR.EOL += sizeof(struct iMap);

			pwrite(imageFD, &CR, sizeof(struct checkRegion), 0); //sync Mem CR with disk CR
			fsync(imageFD);//push all to disk

		}
		else if(packIn->type == 0)//we are creating a new directory
		{
			//validate that the new file can fit in DIR
			int validDir = creatValidate();
			if(validDir == -1)
			{
				printf("failed in valid dir check\n");
				packIn->result = -1;
				return -1;
			}
			newNode->nodeStats.type = MFS_DIRECTORY;
			newNode->nodeStats.size = sizeof(MFS_DirEnt_t) * 2;
			//setup directory
			struct dirBlock newDir;
			dirInit(&newDir);

			//inum is based off the first empty index into our 2D array
			newDir.dirEntry[0].inum = i*16 + j;//set . and ..
			newDir.dirEntry[1].inum = packIn->pinum;

			char nameIn[60];//assign self DIR and parent DIR
			strcpy(nameIn, ".");
			strcpy(newDir.dirEntry[0].name, nameIn);
			strcpy(nameIn, "..");
			strcpy(newDir.dirEntry[1].name, nameIn);

			pwrite(imageFD, &newDir, sizeof(struct dirBlock), CR.EOL);
			newNode->blockOffset[0] = CR.EOL;//set iNode pointer to this block
			CR.EOL += sizeof(struct dirBlock);
			//populate iNode
			newNode->nodeStats.type = MFS_DIRECTORY;//set type to directory entry
			newNode->nodeStats.size = sizeof(MFS_DirEnt_t) * 2; // set size to default block size
			pwrite(imageFD, &newNode, sizeof(struct iNode), CR.EOL); //write iNode
			//populate map
			CR.memMaps[i].nodeOffset[j] = CR.EOL;//set iMap pointer
			CR.EOL += sizeof(struct iNode);//advance past iNode
			//write new map piece
			pwrite(imageFD, &CR.memMaps[i], sizeof(struct iMap), CR.EOL);
			CR.mapOffset[i] = CR.EOL; //update CR pointer to new iMap
			//update EOL to point at very end of file
			CR.EOL += sizeof(struct iMap);


			///////////////////////////////////////////////////////////////////////////


			int a, b, a2, b2;
			a=0;

			//TODO If DIR block is full, we need to bring a new one online
			if(parentInode->nodeStats.size % 4096 == 0)
			{
				//find location of new block
				a = parentInode->nodeStats.size/4096;
				dirInit(&inMemoryDataBlock.memBlocks[a]);//init the next DIR block
			}

			for(a=0; a<14; a++)
			{

				for(b=0; b<64; b++)
				{
					if(inMemoryDataBlock.memBlocks[a].dirEntry[b].inum == -1)
					{
						//update info for new directory entry
						strcpy(inMemoryDataBlock.memBlocks[a].dirEntry[b].name, packIn->name);//set name in holder
						inMemoryDataBlock.memBlocks[a].dirEntry[b].inum = i*16 + j;           //set iNode in holder
						parentInode->nodeStats.size += sizeof(MFS_DirEnt_t);

						a2=a;
						b2=b;
						a=14;
						b=64;
						break;
					}
				}
			}
			a=a2;
			b=b2;//this is the DIR entry

			//update parent D I M
			pwrite(imageFD, &inMemoryDataBlock.memBlocks[a], sizeof(struct dirBlock), CR.EOL);//write updated DIR block
			parentInode->blockOffset[a] = CR.EOL;//point inMem parent iNode at new block
			CR.EOL += MFS_BLOCK_SIZE;//point past written block

			pwrite(imageFD, &parentInode, sizeof(struct iNode), CR.EOL); //write iNode
			CR.memMaps[packIn->pinum/16].nodeOffset[packIn->pinum%16] = CR.EOL;//set inMem iMap pointer to new node
			CR.EOL += sizeof(struct iNode);//advance past iNode

			pwrite(imageFD, &CR.memMaps[packIn->pinum/16], sizeof(struct iMap), CR.EOL);
			CR.mapOffset[packIn->pinum] = CR.EOL; //update CR pointer to new iMap

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



	packIn->result = 0;
	return 0;
}

//removes the file or directory name from the directory specified by pinum .
//0 on success, -1 on failure. Failure modes: pinum does not exist,
//directory is NOT empty. Note that the name not existing is NOT a failure by our definition
//int pinum, char *name
int server_unlink(Package_t *packIn)
{
	if(inumValidate(packIn->pinum) == -1)
	{
		packIn->result = -1;
		return -1;
	}
	int unlinkInum = server_lookup(packIn);

	//find name in DIR and zeros it out(marks it for reuse)
	int i, j, eye, jay;
	for(i=0; i<14; i++)
	{
		for(j=0; j<64; j++)
		{
			//clear name and iNum
			if(strcmp(inMemoryDataBlock.memBlocks[i].dirEntry[j].name, packIn->name) == 0)
			{
				bzero(inMemoryDataBlock.memBlocks[i].dirEntry[j].name, sizeof(inMemoryDataBlock.memBlocks[i].dirEntry[j].name));
				inMemoryDataBlock.memBlocks[i].dirEntry[j].inum = -1;
				eye = i;
				jay = j;
				i=14;
				j=64;
			}
		}
	}
	i = eye;
	j = jay;

	CR.memMaps[unlinkInum/16].nodeOffset[unlinkInum%16] = 0;

	//update parent D I M
	pwrite(imageFD, &inMemoryDataBlock.memBlocks[i], sizeof(struct dirBlock), CR.EOL);//write updated DIR block
	parentInode->blockOffset[i] = CR.EOL;//point inMem parent iNode at new block
	CR.EOL += MFS_BLOCK_SIZE;//point past written block

	pwrite(imageFD, &parentInode, sizeof(struct iNode), CR.EOL); //write iNode
	CR.memMaps[packIn->pinum/16].nodeOffset[packIn->pinum%16] = CR.EOL;//set inMem iMap pointer to new node
	CR.EOL += sizeof(struct iNode);//advance past iNode

	pwrite(imageFD, &CR.memMaps[packIn->pinum/16], sizeof(struct iMap), CR.EOL);
	CR.mapOffset[packIn->pinum] = CR.EOL; //update CR pointer to new iMap

	//update EOL to point at very end of file
	CR.EOL += sizeof(struct iMap);

	pwrite(imageFD, &CR, sizeof(struct checkRegion), 0); //sync Mem CR with disk CR
	fsync(imageFD);

	packIn->result = 0;
	return 0;
}

int server_shutdown()
{
	fsync(imageFD);
	close(imageFD);
	buffer->result = 0;
	UDP_Write(sd, &s, (char*)buffer, sizeof(Package_t));
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


		MFS_Stat_t test;                //MFS_Stat_t Used by MFS_Stat for formatting
		buffer->block = 0;              //int The block of the inode
		strcpy(buffer->buffer, "pants");//char[] buffer for read request from client
		buffer->inum = 0;               //int The inode number
		strcpy(buffer->name, "test");  // char[] The file name
		buffer->pinum = 0;              //int  The parent inode number
		buffer->requestType = CREAT_REQUEST;        //int Request being sent to server(read, write, etc)
		buffer->result = 3;             //int The result of the inode(file or directory)
		buffer->type = 0;               //int creating a fi1e(1) or DIR (0)


		server_creat(buffer);


		//		server_creat(buffer);
		//		strcpy(buffer->name, "test1");
		//		server_creat(buffer);
		//		strcpy(buffer->name, "test2");
		//		server_creat(buffer);
		//		strcpy(buffer->name, "test3");
		//		server_creat(buffer);
		//		strcpy(buffer->name, "test4");
		//		server_creat(buffer);
		//		strcpy(buffer->name, "test5");
		//		server_creat(buffer);

		buffer->pinum = 1;
		buffer->type = 1;

		int z;
		for(z=0; z<896; z++)
		{
			printf("z = %d", z);
			sprintf(buffer->name, "test%d" ,z + 2);
			server_creat(buffer);
			if(z == 64 || z == 128)
			{
				z= 65;
				printf("64\n");
			}
		}


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



		buffer->requestType = WRITE_REQUEST;
		buffer->inum = 1;
		buffer->block = 0;
		server_write(buffer);



		buffer->requestType = READ_REQUEST;
		buffer->inum = 1;
		buffer->block = 0;
		server_read(buffer);

		printf("buff %s", buffer->buffer);

		buffer->inum = 1;
		server_stat(buffer);


	}


	if(!DEBUG)
	{
		//printf("                                SERVER:: waiting in loop\n");

		while (1) {

			int rc = UDP_Read(sd, &s, (char*)buffer, sizeof(Package_t));


			if (rc > 0)
			{
				unpack(buffer);//helper method to decode package_t and call proper server method

				//printf("                                SERVER:: read %d bytes (message: '%s')\n", rc, buffer->name);
				//Package_t reply;
				//strcpy(buffer->name, "Reply");


				rc = UDP_Write(sd, &s, (char*)buffer, sizeof(Package_t));



			}
		}
	}
	return 0;
}
