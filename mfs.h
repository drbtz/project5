#ifndef __MFS_h__
#define __MFS_h__

#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

#define MFS_BLOCK_SIZE   (4096)

typedef struct __MFS_Stat_t {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
    char name[60];  // up to 60 bytes of name in directory (including \0)
    int  inum;      // inode number of entry (-1 means entry not used)
} MFS_DirEnt_t;

//Package structure that is passed between the shared library and the server
typedef struct __Package_t {
	int 		requestType;			//Request being sent to server(read, write, etc)
	int 		pinum;					//The parent inode number
	int 		inum;					//The inode number
	MFS_Stat_t 	m;						//Used by MFS_Stat for formatting
	char 		name[60];				//The file name
	int 		block;					//The block of the inode
	char 		buffer[MFS_BLOCK_SIZE];	//buffer for read request from client
	int 		type;					//The type of the inode(file or directory)
	int			result;					//The result of the request sent to server
} Package_t;

//Package constants to be used by the shared library and the server
//(For consistency)
#define LOOKUP_REQUEST		(0)
#define STAT_REQUEST		(1)
#define WRITE_REQUEST		(2)
#define READ_REQUEST		(3)
#define CREAT_REQUEST		(4)
#define UNLINK_REQUEST		(5)
#define SHUTDOWN_REQUEST	(6)

//communication stuff
//struct 	sockaddr_in raddr;
//struct 	sockaddr_in saddr;
//int 	sd;
//fd_set sockets;
//struct timeval timeout;

int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

#endif // __MFS_h__

