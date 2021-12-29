#include "fs/operations.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_LEN 128

void *function(void *input);

int main() {

    pthread_t tid[2];
    char *path = "/f1";
    assert(tfs_init() != -1);

    if (pthread_create(&tid[0], NULL, function, (void *)("shrek.txt")) != 0)
        exit(EXIT_FAILURE);
    if (pthread_create(&tid[1], NULL, function, (void *)("shrek3.txt")) != 0)
        exit(EXIT_FAILURE);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    tfs_copy_to_external_fs(path, "fs.txt");

    printf("Successful test.\n");
    tfs_destroy();
    return 0;
}

void *function(void *input) {
    FILE *fd = fopen((char *)input, "r");

    char *path = "/f1";
    char buffer[BUFFER_LEN];

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

    return NULL;
}