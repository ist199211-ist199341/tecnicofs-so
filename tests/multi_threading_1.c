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

void *function1(void *input);
void *function2(void *input);

int main() {

    pthread_t tid[2];
    assert(tfs_init() != -1);

    if (pthread_create(&tid[0], NULL, function1, (void *)("shrek.txt")) != 0)
        exit(EXIT_FAILURE);
    if (pthread_create(&tid[1], NULL, function2, (void *)("shrek3.txt")) != 0)
        exit(EXIT_FAILURE);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    // tfs_copy_to_external_fs("/f1", "fs1.txt");
    // tfs_copy_to_external_fs("/f3", "fs3.txt");

    printf("Successful test.\n");
    tfs_destroy();
    return 0;
}

void *function1(void *input) {
    FILE *fd = fopen((char *)input, "r");

    char *path = "/f1";
    char buffer[BUFFER_LEN];

    int f;
    ssize_t r;
    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    size_t bytes_read = fread(buffer, sizeof(char), BUFFER_LEN, fd);

    while (bytes_read > 0) {
        /* read the contents of the file */
        r = tfs_write(f, buffer, bytes_read);
        assert(r == bytes_read);
        bytes_read = fread(buffer, sizeof(char), BUFFER_LEN, fd);
    }
    tfs_close(f);
    tfs_copy_to_external_fs("/f1", "fs1.txt");

    return NULL;
}
void *function2(void *input) {
    FILE *fd = fopen((char *)input, "r");

    char *path = "/f3";
    char buffer[BUFFER_LEN];

    int f;
    ssize_t r;
    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    size_t bytes_read = fread(buffer, sizeof(char), BUFFER_LEN, fd);

    while (bytes_read > 0) {
        /* read the contents of the file */
        r = tfs_write(f, buffer, bytes_read);
        assert(r == bytes_read);
        bytes_read = fread(buffer, sizeof(char), BUFFER_LEN, fd);
    }
    tfs_close(f);
    tfs_copy_to_external_fs("/f3", "fs3.txt");

    return NULL;
}