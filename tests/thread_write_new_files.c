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

char *shrek[12] = {"shrek1.txt",    "shrek2.txt",    "shrek3.txt",
                   "shrek4.txt",    "shrek1-fs.txt", "shrek2-fs.txt",
                   "shrek3-fs.txt", "shrek4-fs.txt", "/f1",
                   "/f2",           "/f3",           "/f4"};

void *function(void *input);

int main() {

    pthread_t tid[4];
    assert(tfs_init() != -1);
    int array[4] = {1, 2, 3, 4};

    if (pthread_create(&tid[0], NULL, function, (void *)(&array[0])) != 0)
        exit(EXIT_FAILURE);
    if (pthread_create(&tid[1], NULL, function, (void *)(&array[1])) != 0)
        exit(EXIT_FAILURE);
    if (pthread_create(&tid[2], NULL, function, (void *)(&array[2])) != 0)
        exit(EXIT_FAILURE);
    if (pthread_create(&tid[3], NULL, function, (void *)(&array[3])) != 0)
        exit(EXIT_FAILURE);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[2], NULL);
    pthread_join(tid[3], NULL);

    // tfs_copy_to_external_fs("/f1", "fs1.txt");
    // tfs_copy_to_external_fs("/f3", "fs3.txt");

    printf("Successful test.\n");
    tfs_destroy();
    return 0;
}

void *function(void *input) {
    int num = *((int *)input);
    FILE *fd = fopen(shrek[num - 1], "r");

    char buffer[BUFFER_LEN];

    char path[4];

    strncpy(path, shrek[num + 7], 3);

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
    tfs_copy_to_external_fs(path, shrek[num + 3]);
    printf("diff %s %s\n", shrek[num - 1], shrek[num + 3]);
    return NULL;
}
