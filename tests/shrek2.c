#include "fs/operations.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define BUFFER_LEN 128

int main() {

    FILE *fd = fopen("shrek.txt", "r");

    char *path = "/f1";
    char buffer[BUFFER_LEN];

    assert(tfs_init() != -1);

    int f;
    ssize_t r;
    int m = 0;
    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    memset(buffer, 0, sizeof(buffer));
    size_t bytes_read = fread(buffer, sizeof(char), strlen(buffer) + 1, fd);

    while (bytes_read > 0) {
        /* read the contents of the file */
        r = tfs_write(f, buffer, strlen(buffer));
        assert(r == strlen(buffer));
        memset(buffer, 0, sizeof(buffer));
        bytes_read = fread(buffer, sizeof(char), strlen(buffer) + 1, fd);

        m++;
    }
    tfs_close(f);

    tfs_copy_to_external_fs(path, "fs.txt");

    f = tfs_open(path, 0);
    assert(f != -1);

    assert(tfs_close(f) != -1);

    printf("\nSuccessful test.\n");

    return 0;
}
