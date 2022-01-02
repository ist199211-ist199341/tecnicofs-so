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

void *write_fn(void *input);
void *read_fn(void *input);

int main() {

    pthread_t tid[2];
    assert(tfs_init() != -1);

    int number;
    printf("Qual filme do Shrek queres obter?(1-4)\n");
    scanf("%d", &number);
    if (number <= 0 || number >= 5) {
        printf("There's no Shrek movie with that number\n");
        return 0;
    }

    char path[4];

    strncpy(path, shrek[number + 7], 3);

    if (pthread_create(&tid[0], NULL, write_fn, (void *)(&number)) != 0)
        exit(EXIT_FAILURE);
    if (pthread_create(&tid[1], NULL, read_fn, (void *)(&path)) != 0)
        exit(EXIT_FAILURE);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    printf("Successful test.\n");
    assert(tfs_destroy() == 0);
    return 0;
}

void *write_fn(void *input) {
    int num = *((int *)input);
    FILE *fd = fopen(shrek[num - 1], "r");

    char buffer[BUFFER_LEN];

    char *path = shrek[num + 7];

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
    printf("diff %s %s\n", shrek[num - 1], shrek[num + 3]);
    return NULL;
}

void *read_fn(void *input) {
    char *path = input;
    int number = path[2] - '0';
    assert(tfs_copy_to_external_fs(path, shrek[number + 3]) == 0);
    return NULL;
}