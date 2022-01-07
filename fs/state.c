#include "state.h"

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
pthread_rwlock_t inode_locks[INODE_TABLE_SIZE]; /* TODO add static */
static char freeinode_ts[INODE_TABLE_SIZE];
static pthread_rwlock_t freeinode_ts_rwl;

/* Data blocks */
static char fs_data[BLOCK_SIZE * DATA_BLOCKS];
static char free_blocks[DATA_BLOCKS];
static pthread_rwlock_t free_blocks_rwl;

/* Volatile FS state */

static open_file_entry_t open_file_table[MAX_OPEN_FILES];
static char free_open_file_entries[MAX_OPEN_FILES];
static pthread_rwlock_t open_file_table_rwl;

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
    };

    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        free_blocks[i] = FREE;
    }
    if (pthread_rwlock_init(&free_blocks_rwl, NULL) != 0) {
        perror("Failed to init RWlock");
        exit(EXIT_FAILURE);
    };

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        free_open_file_entries[i] = FREE;
    }
    if (pthread_rwlock_init(&open_file_table_rwl, NULL) != 0) {
        perror("Failed to init RWlock");
        exit(EXIT_FAILURE);
    };
}

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

    if (pthread_rwlock_destroy(&open_file_table_rwl) != 0) {
        perror("Failed to destroy RWlock");
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
    // TODO i don't think this will cause a deadlock, but maybe it'd be worth to
    // add a trylock here instead (?)
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

/* TODO does this make sense? we don't want to repeat code, but we can't lock
 * twice on inode_delete*/
/*
 * Deletes all allocated blocks in i-node, but it's NOT thread safe.
 * Meant to be used as an auxilary function.
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_delete_data_blocks(inode_t *inode) {
    // TODO what happens if we an error occurs while deleting the inode?
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
    // TODO directories only occupy one block at the moment
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
 * 	- parent directory's i-node number
 * 	- name to search
 * 	Returns i-number linked to the target name, -1 if not found
 */
int find_in_dir(int inumber, char const *sub_name) {
    insert_delay(); // simulate storage access delay to i-node with inumber
    // TODO should we check if inode is not free (like on inode_delete for
    // example)?
    if (!valid_inumber(inumber) || freeinode_ts[inumber] == FREE ||
        inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }

    inode_t *inode = &inode_table[inumber];
    rwl_rdlock(&inode_locks[inumber]);

    /* Locates the block containing the directory's entries */
    // TODO directories only occupy one block at the moment
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
 * 	- the block index
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
 * 	- Block's index
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
 * 	- I-node number of the file to open
 * 	- Initial offset
 * Returns: file handle if successful, -1 otherwise
 */
int add_to_open_file_table(int inumber, size_t offset) {
    rwl_rdlock(&open_file_table_rwl);
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (free_open_file_entries[i] == FREE) {
            rwl_unlock(&open_file_table_rwl);
            rwl_wrlock(&open_file_table_rwl);
            if (free_open_file_entries[i] != FREE) {
                // another thread might have gotten here while we changed the
                // locks, therefore we need to let go and check next entry
                rwl_unlock(&open_file_table_rwl);
                rwl_rdlock(&open_file_table_rwl);
                continue;
            }
            free_open_file_entries[i] = TAKEN;
            open_file_table[i].of_inumber = inumber;
            open_file_table[i].of_offset = offset;
            rwl_unlock(&open_file_table_rwl);
            return i;
        }
    }
    rwl_unlock(&open_file_table_rwl);
    return -1;
}

/* Frees an entry from the open file table
 * Inputs:
 * 	- file handle to free/close
 * Returns 0 is success, -1 otherwise
 */
int remove_from_open_file_table(int fhandle) {
    rwl_wrlock(&open_file_table_rwl);
    if (!valid_file_handle(fhandle) ||
        free_open_file_entries[fhandle] != TAKEN) {
        rwl_unlock(&open_file_table_rwl);
        return -1;
    }
    free_open_file_entries[fhandle] = FREE;
    rwl_unlock(&open_file_table_rwl);
    return 0;
}

/* Returns pointer to a given entry in the open file table
 * Inputs:
 * 	 - file handle
 * Returns: pointer to the entry if sucessful, NULL otherwise
 */
open_file_entry_t *get_open_file_entry(int fhandle) {
    if (!valid_file_handle(fhandle)) {
        return NULL;
    }
    return &open_file_table[fhandle];
}
/*
 * Inputs:
 *   - inode: pointer to the inode
 *   - index: the index of the i_data_block to get
 * Returns: Returns the index of the data block if sucessful, or -1 otherwise
 */
int inode_get_block_number_at_index(inode_t *inode, int index) {
    if (index < 0 || index >= INODE_BLOCK_COUNT) {
        return -1;
    }

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

/*
 * Inputs:
 *   - inode: pointer to the inode
 *   - index: the index of the i_data_block to set
 *   - i_block_number: the block number to set the i_data_block to
 * Returns: Returns 0 on success, -1 on failure
 */
int inode_set_block_number_at_index(inode_t *inode, int index,
                                    int i_block_number) {
    // Allow adding a new block if it's immediately after the last block of the
    // i_node
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

void rwl_wrlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_wrlock(rwl) != 0) {
        perror("Failed to lock RWlock");
        exit(EXIT_FAILURE);
    }
}

void rwl_rdlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_rdlock(rwl) != 0) {
        perror("Failed to lock RWlock");
        exit(EXIT_FAILURE);
    }
}

void rwl_unlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_unlock(rwl) != 0) {
        perror("Failed to unlock RWlock");
        exit(EXIT_FAILURE);
    }
}
