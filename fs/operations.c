#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }

        /* Truncate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                size_t remaining_size = inode->i_size;
                // TODO should be int, but needs casting
                size_t current_block_i = remaining_size / BLOCK_SIZE;
                while (remaining_size > 0) {
                    int i_data_block = inode_get_block_number_at_index(
                        inode, (int)current_block_i);
                    if (i_data_block == -1) {
                        return -1;
                    }
                    if (data_block_free(i_data_block) == -1) {
                        return -1;
                    }

                    --current_block_i;
                    remaining_size -= BLOCK_SIZE;
                }
                // TODO  Wrong value
                inode->i_size = 0;
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}

int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Determine how many bytes to write */
    if (to_write + file->of_offset > BLOCK_SIZE * INODE_BLOCK_COUNT) {
        to_write = BLOCK_SIZE * INODE_BLOCK_COUNT - file->of_offset;
    }

    // TODO should be int, but needs casting
    size_t current_block_i = file->of_offset / BLOCK_SIZE;
    size_t written = to_write;

    while (to_write > 0) {
        size_t to_write_block = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);
        /* if remaining to_write does not fill the whole block */
        if (to_write_block > to_write) {
            to_write_block = to_write;
        }

        /* If empty file, allocate new block */
        if (inode->i_size <= current_block_i * BLOCK_SIZE) {

            // TODO check if it's supposed to do this
            inode->i_size += BLOCK_SIZE;
            inode_set_block_number_at_index(inode, (int)current_block_i,
                                            data_block_alloc());
        }

        void *block = data_block_get(
            inode_get_block_number_at_index(inode, (int)current_block_i));
        if (block == NULL) {
            return -1;
        }

        /* Perform the actual write */
        memcpy(block + file->of_offset, buffer, to_write_block);

        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_write_block;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }

        ++current_block_i;
        to_write -= to_write_block;
    }

    return (ssize_t)written;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (file->of_offset + to_read >= BLOCK_SIZE * INODE_BLOCK_COUNT) {
        return -1;
    }

    // TODO should be int, but needs casting
    size_t current_block_i = file->of_offset / BLOCK_SIZE;

    while (to_read > 0) {
        size_t to_read_block = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);
        /* if remaining to_read does not need the whole block */
        if (to_read_block > to_read) {
            to_read_block = to_read;
        }

        void *block = data_block_get(
            inode_get_block_number_at_index(inode, (int)current_block_i));
        if (block == NULL) {
            return -1;
        }

        /* Perform the actual read */
        memcpy(buffer, block + file->of_offset, to_read_block);

        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_read_block;

        ++current_block_i;
        to_read -= to_read_block;
    }

    return (ssize_t)strlen(buffer);
}
