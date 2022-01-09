#include "operations.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern pthread_rwlock_t inode_locks[INODE_TABLE_SIZE]; /* TODO */

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
                inode_truncate(inum);
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
    int inumber = file->of_inumber;
    inode_t *inode = inode_get(inumber);
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
    rwl_wrlock(&inode_locks[inumber]);

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
                return -1;
            }
            if (inode_set_block_number_at_index(inode, (int)current_block_i,
                                                new_block) < 0) {
                rwl_unlock(&inode_locks[inumber]);
                return -1;
            }
        }
        /* Get block to write to */
        void *block = data_block_get(
            inode_get_block_number_at_index(inode, (int)current_block_i));
        if (block == NULL) {
            rwl_unlock(&inode_locks[inumber]);
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
    return (ssize_t)written;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    int inumber = file->of_inumber;
    inode_t *inode = inode_get(inumber);
    if (inode == NULL) {
        return -1;
    }
    rwl_rdlock(&inode_locks[inumber]);

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    // TODO should be int, but needs casting
    size_t current_block_i = file->of_offset / BLOCK_SIZE;

    size_t read = to_read;

    while (to_read > 0) {
        size_t to_read_block = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);
        /* if remaining to_read does not need the whole block */
        if (to_read_block > to_read) {
            to_read_block = to_read;
        }

        void *block = data_block_get(
            inode_get_block_number_at_index(inode, (int)current_block_i));
        if (block == NULL) {
            rwl_unlock(&inode_locks[inumber]);
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
    return (ssize_t)read;
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {

    // open at the start of the file
    int source_file = tfs_open(source_path, 0);
    /* if file doesn't exist */
    if (source_file == -1) {
        return -1;
    }
    /* flag "w" - crates empty file, if it exists it clears the content :) */
    FILE *dest_file = fopen(dest_path, "w");
    if (dest_file == NULL) {
        tfs_close(source_file); // ignore result since we return -1 anyway
        return -1;
    }

    char buffer[BLOCK_SIZE];

    ssize_t read;

    while ((read = tfs_read(source_file, buffer, BLOCK_SIZE)) > 0) {
        fwrite(buffer, sizeof(char), (size_t)read, dest_file);
    }
    if (fclose(dest_file) != 0) {
        tfs_close(source_file); // ignore result since we return -1 anyway
        return -1;
    }
    if (tfs_close(source_file) != 0) {
        return -1;
    }
    if (read < 0) {
        return -1;
    }
    return 0;
}
