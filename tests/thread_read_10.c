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

#define BUFFER_LEN 10000
#define THREAD_NUM 20

char *shrek[12] = {"shrek1.txt",    "shrek2.txt",    "shrek3.txt",
                   "shrek4.txt",    "shrek1-fs.txt", "shrek2-fs.txt",
                   "shrek3-fs.txt", "shrek4-fs.txt", "/f1",
                   "/f2",           "/f3",           "/f4"};

void write_fn(int input);
void *read_fn(void *input);

int main() {

    pthread_t tid[THREAD_NUM];
    int index[1] = {0};
    assert(tfs_init() != -1);

    write_fn(0);

    for (int i = 0; i < THREAD_NUM; i++) {
        if (pthread_create(&tid[i], NULL, read_fn, (void *)(&index[0])) != 0)
            exit(EXIT_FAILURE);
    }

    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_join(tid[i], NULL);
    }
    tfs_copy_to_external_fs(shrek[8], "shrek1-fs.txt");
    tfs_destroy();
    printf("Successful test.\n");

    return 0;
}

void write_fn(int input) {

    FILE *fd = fopen(shrek[input], "r");

    char buffer[BUFFER_LEN];

    char *path = shrek[input + 8];

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
    assert(tfs_close(f) == 0);
}

void *read_fn(void *input) {
    int num = *((int *)input);
    char *path = shrek[num + 8];
    int fd = tfs_open(path, 0);
    char output[BUFFER_LEN + 1];
    assert(tfs_read(fd, output, BUFFER_LEN) == BUFFER_LEN);
    output[BUFFER_LEN] = '\0';
    printf("%s", output);
    return NULL;
}