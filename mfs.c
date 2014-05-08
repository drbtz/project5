#include "mfs.h"
#include "udp.h"
#define BUFFER_SIZE (4096)

struct sockaddr_in raddr;
struct sockaddr_in saddr;
int sd;

fd_set sockets;
struct timeval timeout;

/*
 * MFS_Init() takes a host name and port number and uses those to find the
 * server exporting the file system.
 */
int MFS_Init(char *hostname, int port)
{
	sd = UDP_Open(0);
	assert(sd > -1);
	int rc = UDP_FillSockAddr(&saddr, hostname, port);
	if(rc < 0)
	{
		perror("Socket fill");
		return -1;
	}

	//set up the fd_set for select
	FD_ZERO(&sockets);
	FD_SET(sd, &sockets);

	//set up timeval for select
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;


	return 0;
}

/*
 * MFS_Lookup() takes the parent inode number (which should be the inode number
 * of a directory) and looks up the entry name in it. The inode number of name
 * is returned. Success: return inode number of name; failure: return -1.
 * Failure modes: invalid pinum, name does not exist in pinum.
 */
int MFS_Lookup(int pinum, char *name)
{
	//Check the name?
	int nameLength = strlen(name);
	if(nameLength > 60)
	{//too long, can't be in the server
		//printf("Lookup: name too long");
		return -1;
	}

	//Create package
	Package_t lookupPackage;
	//Fill package
	lookupPackage.pinum = pinum;
	strncpy(lookupPackage.name, name, 60);
	lookupPackage.requestType = LOOKUP_REQUEST;
	lookupPackage.result = 31337;

	//Send the package to the server
	int rc = UDP_Write(sd, &saddr,(char*)&lookupPackage, sizeof(Package_t));
	if (rc <= 0) return -1;

	printf("before select.\n");
	//Wait for a response from the server(5 second timeout)
	if (select(FD_SETSIZE, &sockets, NULL, NULL, &timeout)) {
		UDP_Read(sd, &raddr, (char*)&lookupPackage, sizeof(Package_t));
		//printf("CLIENT:: read %d bytes (message: '%s')\n", rc, lookupPackage.name);
	}

	//return the result
	return lookupPackage.result;
}

/*
 * MFS_Stat() returns some information about the file specified by inum. Upon
 * success, return 0, otherwise -1. The exact info returned is defined by
 * MFS_Stat_t. Failure modes: inum does not exist.
 */
int MFS_Stat(int inum, MFS_Stat_t *m)
{
	//Create a package and fill it
	Package_t statPackage;
	statPackage.inum = inum;
	statPackage.requestType = STAT_REQUEST;
	statPackage.result = 31337;

	//send package to the server
	int rc = UDP_Write(sd, &saddr, (char*)&statPackage, sizeof(Package_t));
	if (rc <= 0) return -1;


	//Wait for a response from the server(5 second timeout)
	if (select(FD_SETSIZE, &sockets, NULL, NULL, &timeout)) {
		UDP_Read(sd, &raddr, (char*)&statPackage, sizeof(Package_t));
		//printf("CLIENT:: read %d bytes (message: '%s')\n", rc, statPackage.name);
	}

	//check result of the package
	if(statPackage.result == -1)
	{//if fail, return fail
		return -1;
	}
	MFS_Stat_t m2;

	m2.type = statPackage.m.type;
	m2.size = statPackage.m.size;

	printf("size: %d\n",m2.size);
	printf("type: %d\n",m2.type);

	//Unload stat data from package into m
	*m = m2;
	return 0;
}

/*
 * MFS_Write() writes a block of size 4096 bytes at the block offset specified
 * by block . Returns 0 on success, -1 on failure. Failure modes: invalid inum,
 * invalid block, not a regular file (because you can't write to directories).
 */
int MFS_Write(int inum, char *buffer, int block)
{
	//create package and fill it with data(buffer, inum, block, request type)
	Package_t writePackage;
	memcpy(writePackage.buffer, buffer, MFS_BLOCK_SIZE);
	//printf("write buffer length: %zu\n",sizeof(writePackage.buffer));
	writePackage.inum = 1;
	writePackage.block = block;
	writePackage.requestType = WRITE_REQUEST;
	writePackage.result = 31337;

	if(block > 13)
	{
		return -1;
	}

	//Send package to the server
	int rc = UDP_Write(sd, &saddr,(char*) &writePackage, sizeof(Package_t));
	if (rc <= 0) return -1;

	//Wait for a package from the server(5 second timeout)
	if (select(FD_SETSIZE, &sockets, NULL, NULL, &timeout)) {
		UDP_Read(sd, &raddr,(char*) &writePackage, sizeof(Package_t));
		//printf("CLIENT:: read %d bytes (message: '%s')\n", rc, writePackage.buffer);
	}

	return writePackage.result;
}

/*
 * MFS_Read() reads a block specified by block into the buffer from file
 * specified by inum . The routine should work for either a file or directory;
 * directories should return data in the format specified by MFS_DirEnt_t.
 * Success: 0, failure: -1. Failure modes: invalid inum, invalid block.
 */
int MFS_Read(int inum, char *buffer, int block)
{
	//create a package and fill it
	Package_t readPackage;
	readPackage.inum = inum;
	readPackage.block = block;
	readPackage.requestType = READ_REQUEST;
	readPackage.result = 31337;

	if(block > 13)
	{
		return -1;
	}


	//send package to the server
	int rc = UDP_Write(sd, &saddr,(char*) &readPackage, sizeof(Package_t));
	if (rc <= 0) return -1;

	//Wait for a package from the server(5 second timeout)
	if (select(FD_SETSIZE, &sockets, NULL, NULL, &timeout)) {
		UDP_Read(sd, &raddr,(char*) &readPackage, sizeof(Package_t));
		//printf("CLIENT:: read %d bytes (message: '%s')\n", rc, readPackage.buffer);
	}

	//check the result of the package
	if(readPackage.result == -1)
	{//if fail, return fail
		return -1;
	}
	//printf("read buffer length: %zu\n",sizeof(readPackage.buffer));
	//Unload read data from server into buffer
	memcpy(buffer, (void *)readPackage.buffer, MFS_BLOCK_SIZE);
	//*buffer = readPackage.buffer;
	return readPackage.result;
}

/*
 * MFS_Creat() makes a file ( type == MFS_REGULAR_FILE) or directory
 * ( type == MFS_DIRECTORY) in the parenpackaget directory specified by pinum of name
 * name . Returns 0 on success, -1 on failure. Failure modes: pinum does not
 * exist, or name is too long. If name already exists, return success
 * (think about why).
 */
int MFS_Creat(int pinum, int type, char *name)
{
	//check the name
	int nameLength = strlen(name);
	if(nameLength > 60)
	{//too long
		return -1;
	}

	//creat a package and fill it
	Package_t creatPackage;
	creatPackage.pinum = pinum;
	creatPackage.type = type;
	strncpy(creatPackage.name, name, 60);
	creatPackage.requestType = CREAT_REQUEST;
	creatPackage.result = 31337;

	//send package to the server
	int rc = UDP_Write(sd, &saddr, (char*)&creatPackage, sizeof(Package_t));
	if (rc <= 0) return -1;

	//Wait for a package from the server(5 second timeout)
	if (select(FD_SETSIZE, &sockets, NULL, NULL, &timeout)) {
		UDP_Read(sd, &raddr, (char*)&creatPackage, sizeof(Package_t));
		//printf("CLIENT:: read %d bytes (message: '%s')\n", rc, creatPackage.buffer);
	}


	return creatPackage.result;
}

/*
 * MFS_Unlink() removes the file or directory name from the directory specified
 * by pinum . 0 on success, -1 on failure. Failure modes: pinum does not exist,
 * directory is NOT empty. Note that the name not existing is NOT a failure by
 * our definition (think about why this might be).
 */
int MFS_Unlink(int pinum, char *name)
{
	//check the name
	int nameLength = strlen(name);
	if(nameLength > 60)
	{//too long, return success
		return 0;
	}

	//Create package and fill it
	Package_t unlinkPackage;
	unlinkPackage.pinum = pinum;
	strncpy(unlinkPackage.name, name, 60);
	unlinkPackage.requestType = UNLINK_REQUEST;
	unlinkPackage.result = 31337;

	//send package to the server
	int rc = UDP_Write(sd, &saddr, (char*)&unlinkPackage, sizeof(Package_t));
	if (rc <= 0) return -1;

	//Wait for a package from the server(5 second timeout)
	if (select(FD_SETSIZE, &sockets, NULL, NULL, &timeout)) {
		UDP_Read(sd, &raddr, (char*)&unlinkPackage, sizeof(Package_t));
		//printf("CLIENT:: read %d bytes (message: '%s')\n", rc, unlinkPackage.buffer);
	}

	return unlinkPackage.result;
}

/*
 * MFS_Shutdown() just tells the server to force all of its data structures to
 * disk and shutdown by calling exit(0). This interface will mostly be used for
 * testing purposes.
 */
int MFS_Shutdown()
{
	//create package
	Package_t shutdownPackage;
	shutdownPackage.requestType = SHUTDOWN_REQUEST;
	shutdownPackage.result = 31337;

	//send package to the server
	int rc = UDP_Write(sd, &saddr, (char*)&shutdownPackage, sizeof(Package_t));
	if (rc <= 0) return -1;

	//Wait for a package from the server(5 second timeout)
	if (select(FD_SETSIZE, &sockets, NULL, NULL, &timeout)) {
		UDP_Read(sd, &raddr, (char*)&shutdownPackage, sizeof(Package_t));
		//printf("CLIENT:: read %d bytes (message: '%s')\n", rc, shutdownPackage.buffer);
	}
	printf("%d\n", shutdownPackage.result);
	return shutdownPackage.result;
}
