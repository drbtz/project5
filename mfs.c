#include "mfs.h"
#include "udp.h"
#define BUFFER_SIZE (4096)

struct sockaddr_in raddr;
struct sockaddr_in saddr;
int sd;
char buffer[BUFFER_SIZE];

/*
 * MFS_Init() takes a host name and port number and uses those to find the
 * server exporting the file system.
 */
int MFS_Init(char *hostname, int port)
{
	int sd = UDP_Open(0);
	assert(sd > -1);
	int rc = UDP_FillSockAddr(&saddr, hostname, port);
	if(rc < 0)
	{
		perror("Socket fill");
		return -1;
	}


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
	//Check the name
	int nameLength = strlen(name);
	if(nameLength > 60)
	{
		printf("Lookup: name too long");
		return -1;
	}
	//Create package
	Package_t lookupPackage;
	//Fill package
	lookupPackage.pinum = pinum;
	strcpy(lookupPackage.name, name);
	lookupPackage.requestType = LOOKUP_REQUEST;
	//Send the package to the server

	//Wait for a response from the server(5 second timeout)

	//Unpack the package

	//return the result
	return 0;
}

/*
 * MFS_Stat() returns some information about the file specified by inum. Upon
 * success, return 0, otherwise -1. The exact info returned is defined by
 * MFS_Stat_t. Failure modes: inum does not exist.
 */
int MFS_Stat(int inum, MFS_Stat_t *m)
{
	return 0;
}

/*
 * MFS_Write() writes a block of size 4096 bytes at the block offset specified
 * by block . Returns 0 on success, -1 on failure. Failure modes: invalid inum,
 * invalid block, not a regular file (because you can't write to directories).
 */
int MFS_Write(int inum, char *buffer, int block)
{
	int rc = UDP_Write(sd, &saddr, buffer, BUFFER_SIZE);
	if (rc <= 0) return -1;
	// printf("CLIENT:: sent message (%d)\n", rc);
	if (rc > 0) {
		int rc = UDP_Read(sd, &raddr, buffer, BUFFER_SIZE);
		printf("CLIENT:: read %d bytes (message: '%s')\n", rc, buffer);
	}

	return 0;
}

/*
 * MFS_Read() reads a block specified by block into the buffer from file
 * specified by inum . The routine should work for either a file or directory;
 * directories should return data in the format specified by MFS_DirEnt_t.
 * Success: 0, failure: -1. Failure modes: invalid inum, invalid block.
 */
int MFS_Read(int inum, char *buffer, int block)
{
	return 0;
}

/*
 * MFS_Creat() makes a file ( type == MFS_REGULAR_FILE) or directory
 * ( type == MFS_DIRECTORY) in the parent directory specified by pinum of name
 * name . Returns 0 on success, -1 on failure. Failure modes: pinum does not
 * exist, or name is too long. If name already exists, return success
 * (think about why).
 */
int MFS_Creat(int pinum, int type, char *name)
{
	return 0;
}

/*
 * MFS_Unlink() removes the file or directory name from the directory specified
 * by pinum . 0 on success, -1 on failure. Failure modes: pinum does not exist,
 * directory is NOT empty. Note that the name not existing is NOT a failure by
 * our definition (think about why this might be).
 */
int MFS_Unlink(int pinum, char *name)
{
	return 0;
}

/*
 * MFS_Shutdown() just tells the server to force all of its data structures to
 * disk and shutdown by calling exit(0). This interface will mostly be used for
 * testing purposes.
 */
int MFS_Shutdown()
{
	return 0;
}
