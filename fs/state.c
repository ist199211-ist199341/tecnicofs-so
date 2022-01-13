#include "state.h"
#include "utils.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

/* Persistent FS state  (in reality, it should be maintained in secondary
 * memory; for simplicity, this project maintains it in primary memory) */

/* I-node table */
static inode_t inode_table[INODE_TABLE_SIZE];
static pthread_rwlock_t inode_locks[INODE_TABLE_SIZE];
static char freeinode_ts[INODE_TABLE_SIZE];
static pthread_rwlock_t freeinode_ts_rwl;

/* Data blocks */
static char fs_data[BLOCK_SIZE * DATA_BLOCKS];
static char free_blocks[DATA_BLOCKS];
static pthread_rwlock_t free_blocks_rwl;

/* Volatile FS state */

static open_file_entry_t open_file_table[MAX_OPEN_FILES];
static char free_open_file_entries[MAX_OPEN_FILES];
static pthread_mutex_t free_open_file_entries_mutex;

static inline bool valid_inumber(int inumber) {
    return inumber >= 0 && inumber < INODE_TABLE_SIZE;
}

static inline bool valid_block_number(int block_number) {
    return block_number >= 0 && block_number < DATA_BLOCKS;
}

static inline bool valid_file_handle(int file_handle) {
    return file_handle >= 0 && file_handle < MAX_OPEN_FILES;
}

/**
 * We need to defeat the optimizer for the insert_delay() function.
 * Under optimization, the empty loop would be completely optimized away.
 * This function tells the compiler that the assembly code being run (which is
 * none) might potentially change *all memory in the process*.
 *
 * This prevents the optimizer from optimizing this code away, because it does
 * not know what it does and it may have side effects.
 *
 * Reference with more information: https://youtu.be/nXaxk27zwlk?t=2775
 *
 * Exercise: try removing this function and look at the assembly generated to
 * compare.
 */
static void touch_all_memory() { __asm volatile("" : : : "memory"); }

/*
 * Auxiliary function to insert a delay.
 * Used in accesses to persistent FS state as a way of emulating access
 * latencies as if such data structures were really stored in secondary memory.
 */
static void insert_delay() {
    for (int i = 0; i < DELAY; i++) {
        touch_all_memory();
    }
}

/*
 * Initializes FS state
 */
void state_init() {
    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        freeinode_ts[i] = FREE;
        if (pthread_rwlock_init(&inode_locks[i], NULL) != 0) {
            perror("Failed to init RWlock");
            exit(EXIT_FAILURE);
        }
    }
    if (pthread_rwlock_init(&freeinode_ts_rwl, NULL) != 0) {
        perror("Failed to init RWlock");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        free_blocks[i] = FREE;
    }
    if (pthread_rwlock_init(&free_blocks_rwl, NULL) != 0) {
        perror("Failed to init RWlock");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        if (pthread_mutex_init(&open_file_table[i].lock, NULL) != 0) {
            perror("Failed to init Mutex");
            exit(EXIT_FAILURE);
        }
        free_open_file_entries[i] = FREE;
    }
    if (pthread_mutex_init(&free_open_file_entries_mutex, NULL) != 0) {
        perror("Failed to init Mutex");
        exit(EXIT_FAILURE);
    }
}

/*
 * Destroys FS state
 */
void state_destroy() {
    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        if (pthread_rwlock_destroy(&inode_locks[i]) != 0) {
            perror("Failed to destroy RWlock");
            exit(EXIT_FAILURE);
        }
    }

    if (pthread_rwlock_destroy(&freeinode_ts_rwl) != 0) {
        perror("Failed to destroy RWlock");
        exit(EXIT_FAILURE);
    }

    if (pthread_rwlock_destroy(&free_blocks_rwl) != 0) {
        perror("Failed to destroy RWlock");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        if (pthread_mutex_destroy(&open_file_table[i].lock) != 0) {
            perror("Failed to destroy Mutex");
            exit(EXIT_FAILURE);
        }
    }

    if (pthread_mutex_destroy(&free_open_file_entries_mutex) != 0) {
        perror("Failed to destroy Mutex");
        exit(EXIT_FAILURE);
    }
}

/*
 * Creates a new i-node in the i-node table.
 * Input:
 *  - n_type: the type of the node (file or directory)
 * Returns:
 *  new i-node's number if successfully created, -1 otherwise
 */
int inode_create(inode_type n_type) {
    rwl_rdlock(&freeinode_ts_rwl);
    for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++) {
        if ((inumber * (int)sizeof(allocation_state_t) % BLOCK_SIZE) == 0) {
            insert_delay(); // simulate storage access delay (to freeinode_ts)
        }

        /* Finds first free entry in i-node table */
        if (freeinode_ts[inumber] == FREE) {
            rwl_unlock(&freeinode_ts_rwl);
            rwl_wrlock(&freeinode_ts_rwl);
            // Recheck inode status because it might have been changed by
            // another thread
            if (freeinode_ts[inumber] != FREE) {
                rwl_unlock(&freeinode_ts_rwl);
                rwl_rdlock(&freeinode_ts_rwl);
                continue;
            }
            /* Found a free entry, so takes it for the new i-node*/
            freeinode_ts[inumber] = TAKEN;
            insert_delay(); // simulate storage access delay (to i-node)
            inode_table[inumber].i_node_type = n_type;

            if (n_type == T_DIRECTORY) {
                /* Initializes directory (filling its block with empty
                 * entries, labeled with inumber==-1) */
                int indirect_block_number = data_block_alloc();
                if (indirect_block_number == -1) {
                    freeinode_ts[inumber] = FREE;
                    rwl_unlock(&freeinode_ts_rwl);
                    return -1;
                }

                inode_table[inumber].i_size = BLOCK_SIZE;
                /* For simplificaion, a directory will only use the first entry
                 * of the array of data_blocks */
                inode_table[inumber].i_data_blocks[0] = indirect_block_number;
                for (int i = 1; i < INODE_DIRECT_BLOCK_SIZE; i++) {
                    inode_table[inumber].i_data_blocks[i] = -1;
                }
                inode_table[inumber].i_indirect_block = -1;

                dir_entry_t *dir_entry =
                    (dir_entry_t *)data_block_get(indirect_block_number);
                if (dir_entry == NULL) {
                    freeinode_ts[inumber] = FREE;
                    rwl_unlock(&freeinode_ts_rwl);
                    return -1;
                }

                for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
                    dir_entry[i].d_inumber = -1;
                }
            } else {
                /* In case of a new file, simply sets its size to 0 */
                inode_table[inumber].i_size = 0;

                for (int i = 0; i < INODE_DIRECT_BLOCK_SIZE; i++) {
                    inode_table[inumber].i_data_blocks[i] = -1;
                }
                inode_table[inumber].i_indirect_block = -1;
            }

            rwl_unlock(&freeinode_ts_rwl);
            return inumber;
        }
    }
    rwl_unlock(&freeinode_ts_rwl);
    return -1;
}

/*
 * Deletes the i-node.
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_delete(int inumber) {
    // simulate storage access delay (to i-node and freeinode_ts)
    insert_delay();
    insert_delay();

    if (!valid_inumber(inumber)) {
        return -1;
    }

    rwl_wrlock(&freeinode_ts_rwl);
    if (freeinode_ts[inumber] == FREE) {
        rwl_unlock(&freeinode_ts_rwl);
        return -1;
    }
    rwl_wrlock(&inode_locks[inumber]);

    freeinode_ts[inumber] = FREE;
    inode_t *inode = &inode_table[inumber];
    if (inode_delete_data_blocks(inode) < 0) {
        rwl_unlock(&inode_locks[inumber]);
        rwl_unlock(&freeinode_ts_rwl);
        return -1;
    }

    rwl_unlock(&inode_locks[inumber]);
    rwl_unlock(&freeinode_ts_rwl);

    return 0;
}

/*
 * Deletes all allocated blocks in i-node.
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_truncate(int inumber) {
    // simulate storage access delay (to i-node and freeinode_ts)
    insert_delay();
    insert_delay();

    if (!valid_inumber(inumber)) {
        return -1;
    }

    rwl_rdlock(&freeinode_ts_rwl);
    if (freeinode_ts[inumber] == FREE) {
        rwl_unlock(&freeinode_ts_rwl);
        return -1;
    }

    rwl_wrlock(&inode_locks[inumber]);

    inode_t *inode = &inode_table[inumber];
    if (inode_delete_data_blocks(inode) < 0) {
        rwl_unlock(&inode_locks[inumber]);
        rwl_unlock(&freeinode_ts_rwl);
        return -1;
    }

    rwl_unlock(&inode_locks[inumber]);
    rwl_unlock(&freeinode_ts_rwl);

    return 0;
}

/*
 * Deletes all allocated blocks in i-node, but it's NOT thread safe.
 * Meant to be used as an auxilary function.
 * Input:
 *  - inode: a pointer to the inode
 * Returns: 0 if successful, -1 if failed
 */
int inode_delete_data_blocks(inode_t *inode) {
    size_t remaining_size = inode->i_size;
    int current_block_i = (int)(remaining_size / BLOCK_SIZE);
    while (current_block_i >= 0) {
        int i_data_block =
            inode_get_block_number_at_index(inode, current_block_i);
        if (i_data_block == -1) {
            return -1;
        }
        if (data_block_free(i_data_block) == -1) {
            return -1;
        }

        --current_block_i;
        remaining_size -= BLOCK_SIZE;
    }
    inode->i_size = 0;

    return 0;
}

/*
 * Returns a pointer to an existing i-node.
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: pointer if successful, NULL if failed
 */
inode_t *inode_get(int inumber) {
    if (!valid_inumber(inumber)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to i-node
    return &inode_table[inumber];
}

/*
 * Writes to the data blocks of the i-node
 * Input:
 *  - fhandle: file handle (obtained from a previous call to tfs_open)
 *  - buffer: buffer containing the contents to write
 *  - length of the contents (in bytes)
 * Returns:  the number of bytes that were written (can be lower than
 *  'len' if the maximum file size is exceeded), or -1 in case of error
 */
ssize_t inode_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    mutex_lock(&file->lock);

    /* From the open file table entry, we get the inode */
    int inumber = file->of_inumber;

    inode_t *inode = inode_get(inumber);
    if (inode == NULL) {
        mutex_unlock(&file->lock);
        return -1;
    }
    rwl_wrlock(&inode_locks[inumber]);

    /* Make sure offset is not out of bounds */
    if (file->of_offset > inode->i_size) {
        file->of_offset = inode->i_size;
    }

    /* Determine how many bytes to write */
    if (to_write + file->of_offset > BLOCK_SIZE * INODE_BLOCK_COUNT) {
        to_write = BLOCK_SIZE * INODE_BLOCK_COUNT - file->of_offset;
    }

    int current_block_i = (int)(file->of_offset / BLOCK_SIZE);

    size_t written = to_write;

    while (to_write > 0) {

        size_t to_write_block = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);
        /* if remaining to_write does not fill the whole block */
        if (to_write_block > to_write) {
            to_write_block = to_write;
        }

        /* If empty file, allocate new block */
        if (inode->i_size <= current_block_i * BLOCK_SIZE) {

            int new_block = data_block_alloc();
            if (new_block < 0) {
                /* If it gets an error to alloc block */
                rwl_unlock(&inode_locks[inumber]);
                mutex_unlock(&file->lock);
                return -1;
            }
            if (inode_set_block_number_at_index(inode, current_block_i,
                                                new_block) < 0) {
                rwl_unlock(&inode_locks[inumber]);
                mutex_unlock(&file->lock);
                return -1;
            }
        }
        /* Get block to write to */
        void *block = data_block_get(
            inode_get_block_number_at_index(inode, current_block_i));
        if (block == NULL) {
            rwl_unlock(&inode_locks[inumber]);
            mutex_unlock(&file->lock);
            return -1;
        }

        /* Perform the actual write */
        memcpy(block + (file->of_offset % BLOCK_SIZE),
               buffer + sizeof(char) * (written - to_write), to_write_block);

        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_write_block;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }

        ++current_block_i;
        to_write -= to_write_block;
    }
    rwl_unlock(&inode_locks[inumber]);
    mutex_unlock(&file->lock);
    return (ssize_t)written;
}

/* Reads the data of the i-node to the buffer
 * Input:
 *  - file handle (obtained from a previous call to tfs_open)
 *  - destination buffer
 *  - length of the buffer
 *  Returns the number of bytes that were copied from the file to the buffer
 *  (can be lower than 'len' if the file size was reached), or -1 in case of
 * error
 */
ssize_t inode_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    mutex_lock(&file->lock);

    /* From the open file table entry, we get the inode */
    int inumber = file->of_inumber;
    inode_t *inode = inode_get(inumber);
    if (inode == NULL) {
        mutex_unlock(&file->lock);
        return -1;
    }
    rwl_rdlock(&inode_locks[inumber]);

    /* Make sure offset is not out of bounds */
    /* For consistency with inode_write, even though is won't affect reads,
     * the file offset is still changed */
    if (file->of_offset > inode->i_size) {
        file->of_offset = inode->i_size;
    }

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    int current_block_i = (int)(file->of_offset / BLOCK_SIZE);

    size_t read = to_read;

    while (to_read > 0) {
        size_t to_read_block = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);
        /* if remaining to_read does not need the whole block */
        if (to_read_block > to_read) {
            to_read_block = to_read;
        }

        void *block = data_block_get(
            inode_get_block_number_at_index(inode, current_block_i));
        if (block == NULL) {
            rwl_unlock(&inode_locks[inumber]);
            mutex_unlock(&file->lock);
            return -1;
        }

        /* Perform the actual read */
        memcpy(buffer + sizeof(char) * (read - to_read),
               block + (file->of_offset % BLOCK_SIZE), to_read_block);

        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_read_block;

        ++current_block_i;
        to_read -= to_read_block;
    }
    rwl_unlock(&inode_locks[inumber]);
    mutex_unlock(&file->lock);
    return (ssize_t)read;
}

/*
 * Adds an entry to the i-node directory data.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 *  - sub_name: name of the sub i-node entry
 * Returns: SUCCESS or FAIL
 */
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name) {
    if (!valid_inumber(inumber) || !valid_inumber(sub_inumber)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to i-node with inumber
    if (inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }

    if (strlen(sub_name) == 0) {
        return -1;
    }

    inode_t *inode = &inode_table[inumber];
    rwl_wrlock(&inode_locks[inumber]);

    /* Locates the block containing the directory's entries */
    // Directories only occupy one block at the moment, so get the first block
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode->i_data_blocks[0]);
    if (dir_entry == NULL) {
        rwl_unlock(&inode_locks[inumber]);
        return -1;
    }

    /* Finds and fills the first empty entry */
    for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (dir_entry[i].d_inumber == -1) {
            dir_entry[i].d_inumber = sub_inumber;
            strncpy(dir_entry[i].d_name, sub_name, MAX_FILE_NAME - 1);
            dir_entry[i].d_name[MAX_FILE_NAME - 1] = 0;
            rwl_unlock(&inode_locks[inumber]);
            return 0;
        }
    }
    rwl_unlock(&inode_locks[inumber]);
    return -1;
}

/* Looks for a given name inside a directory
 * Input:
 *  - parent directory's i-node number
 *  - name to search
 *  Returns i-number linked to the target name, -1 if not found
 */
int find_in_dir(int inumber, char const *sub_name) {
    insert_delay(); // simulate storage access delay to i-node with inumber
    if (!valid_inumber(inumber) || freeinode_ts[inumber] == FREE ||
        inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }

    inode_t *inode = &inode_table[inumber];
    rwl_rdlock(&inode_locks[inumber]);

    /* Locates the block containing the directory's entries */
    // Directories only occupy one block at the moment, so get the first block
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode->i_data_blocks[0]);
    if (dir_entry == NULL) {
        rwl_unlock(&inode_locks[inumber]);
        return -1;
    }

    /* Iterates over the directory entries looking for one that has the target
     * name */
    for (int i = 0; i < MAX_DIR_ENTRIES; i++)
        if ((dir_entry[i].d_inumber != -1) &&
            (strncmp(dir_entry[i].d_name, sub_name, MAX_FILE_NAME) == 0)) {
            rwl_unlock(&inode_locks[inumber]);
            return dir_entry[i].d_inumber;
        }

    rwl_unlock(&inode_locks[inumber]);
    return -1;
}

/*
 * Allocated a new data block
 * Returns: block index if successful, -1 otherwise
 */
int data_block_alloc() {
    rwl_rdlock(&free_blocks_rwl);

    for (int i = 0; i < DATA_BLOCKS; i++) {

        if (i * (int)sizeof(allocation_state_t) % BLOCK_SIZE == 0) {
            insert_delay(); // simulate storage access delay to free_blocks
        }

        if (free_blocks[i] == FREE) {
            rwl_unlock(&free_blocks_rwl);
            rwl_wrlock(&free_blocks_rwl);
            // recheck since we only had read lock
            if (free_blocks[i] == FREE) {
                free_blocks[i] = TAKEN;
                rwl_unlock(&free_blocks_rwl);
                return i;
            } else {
                // another thread got the block first, let go of the write lock
                // and change to the read lock again
                rwl_unlock(&free_blocks_rwl);
                rwl_rdlock(&free_blocks_rwl);
            }
        }
    }
    rwl_unlock(&free_blocks_rwl);
    return -1;
}

/* Frees a data block
 * Input
 *  - the block index
 * Returns: 0 if success, -1 otherwise
 */
int data_block_free(int block_number) {
    if (!valid_block_number(block_number)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to free_blocks
    free_blocks[block_number] = FREE;
    return 0;
}

/* Returns a pointer to the contents of a given block
 * Input:
 *  - Block's index
 * Returns: pointer to the first byte of the block, NULL otherwise
 */
void *data_block_get(int block_number) {
    if (!valid_block_number(block_number)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to block
    return &fs_data[block_number * BLOCK_SIZE];
}

/* Add new entry to the open file table
 * Inputs:
 *  - I-node number of the file to open
 *  - Initial offset
 * Returns: file handle if successful, -1 otherwise
 */
int add_to_open_file_table(int inumber, size_t offset) {
    mutex_lock(&free_open_file_entries_mutex);
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (free_open_file_entries[i] == FREE) {
            free_open_file_entries[i] = TAKEN;
            mutex_lock(&open_file_table[i].lock);
            open_file_table[i].of_inumber = inumber;
            open_file_table[i].of_offset = offset;
            mutex_unlock(&open_file_table[i].lock);
            mutex_unlock(&free_open_file_entries_mutex);
            return i;
        }
    }
    mutex_unlock(&free_open_file_entries_mutex);
    return -1;
}

/* Frees an entry from the open file table
 * Inputs:
 *  - file handle to free/close
 * Returns 0 is success, -1 otherwise
 */
int remove_from_open_file_table(int fhandle) {
    mutex_lock(&free_open_file_entries_mutex);
    if (!valid_file_handle(fhandle) ||
        free_open_file_entries[fhandle] != TAKEN) {
        mutex_unlock(&free_open_file_entries_mutex);
        return -1;
    }
    free_open_file_entries[fhandle] = FREE;
    mutex_unlock(&free_open_file_entries_mutex);
    return 0;
}

/* Returns pointer to a given entry in the open file table
 * Inputs:
 *   - file handle
 * Returns: pointer to the entry if successful, NULL otherwise
 */
open_file_entry_t *get_open_file_entry(int fhandle) {
    if (!valid_file_handle(fhandle)) {
        return NULL;
    }
    return &open_file_table[fhandle];
}
/* Gets the block number given the index of the i-node
 * Inputs:
 *   - inode: pointer to the inode
 *   - index: the index of the i_data_block to get
 * Returns: index of the data block if successful, or -1 otherwise
 */
int inode_get_block_number_at_index(inode_t *inode, int index) {
    if (index < 0 || index >= INODE_BLOCK_COUNT) {
        return -1;
    }

    // if index is in the indirect block
    if (index >= INODE_DIRECT_BLOCK_SIZE) {
        int *block = data_block_get(inode->i_indirect_block);
        if (block == NULL) {
            return -1;
        }
        return block[index - INODE_DIRECT_BLOCK_SIZE];
    } else {
        return inode->i_data_blocks[index];
    }
}

/* Sets the number of the data block at the given index
 * Inputs:
 *   - inode: pointer to the inode
 *   - index: the index of the i_data_block to set
 *   - i_block_number: the block number to set the i_data_block to
 * Returns: 0 on success, -1 on failure
 */
int inode_set_block_number_at_index(inode_t *inode, int index,
                                    int i_block_number) {
    if (index < 0 || index >= INODE_BLOCK_COUNT) {
        return -1;
    }
    // if index is in the indirect block
    if (index >= INODE_DIRECT_BLOCK_SIZE) {
        int *block = data_block_get(inode->i_indirect_block);
        if (block == NULL) {
            int indirect_block_number = data_block_alloc();
            if (indirect_block_number == -1) {
                return -1;
            }
            inode->i_indirect_block = indirect_block_number;
            block = data_block_get(indirect_block_number);
            if (block == NULL) {
                return -1;
            }
        }
        block[index - INODE_DIRECT_BLOCK_SIZE] = i_block_number;
    } else {
        inode->i_data_blocks[index] = i_block_number;
    }
    return 0;
}
