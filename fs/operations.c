#include "operations.h"
#include "utils.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static pthread_mutex_t tfs_open_mutex;

int tfs_init() {
    state_init();

    if (pthread_mutex_init(&tfs_open_mutex, NULL) != 0) {
        perror("Failed to init Mutex");
        exit(EXIT_FAILURE);
    }

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();

    if (pthread_mutex_destroy(&tfs_open_mutex) != 0) {
        perror("Failed to destroy Mutex");
        exit(EXIT_FAILURE);
    }

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

    mutex_lock(&tfs_open_mutex);
    inum = tfs_lookup(name);
    if (inum >= 0) {
        mutex_unlock(&tfs_open_mutex);
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
            mutex_unlock(&tfs_open_mutex);
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            mutex_unlock(&tfs_open_mutex);
            inode_delete(inum);
            return -1;
        }
        mutex_unlock(&tfs_open_mutex);
        offset = 0;
    } else {
        mutex_unlock(&tfs_open_mutex);
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
    return inode_write(fhandle, buffer, to_write);
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    return inode_read(fhandle, buffer, len);
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
