#ifndef CONFIG_H
#define CONFIG_H

/* FS root inode number */
#define ROOT_DIR_INUM (0)

#define BLOCK_SIZE (1024)
#define DATA_BLOCKS (1024)
#define INODE_TABLE_SIZE (50)
#define MAX_OPEN_FILES (20)
#define MAX_FILE_NAME (40)
#define INODE_DIRECT_BLOCK_SIZE (10)
// Maximum number of pointers that a inode can handle
#define INODE_BLOCK_COUNT (INODE_DIRECT_BLOCK_SIZE + BLOCK_SIZE / sizeof(int))

#define DELAY (5000)

// Number of simultaneous connections that the server can handle at a given time
#define SIMULTANEOUS_CONNECTIONS (50)

#endif // CONFIG_H
