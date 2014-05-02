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

typedef struct __Package_t {
	int 		requestType;	//Request being sent to server(read, write, etc)
	int 		pinum;			//The parent inode number
	int 		inum;			//The inode number
	MFS_Stat_t 	m;				//Used by MFS_Stat for formatting
	char 		name[60];		//The file name
	int 		block;			//The block of the inode
	char 		buffer[4096];	//buffer for read request from client
	int 		type;			//The type of the inode(file or directory)
	int			result;			//The result of the request sent to server
} Package_t;


int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

#endif // __MFS_h__

