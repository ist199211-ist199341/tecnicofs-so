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
void write_fn(int input);

int main() {

    pthread_t tid[2];
    int operations[2] = {0, 1};
    assert(tfs_init() != -1);

    write_fn(0);
    write_fn(1);

    // A ORDEM IMPORTA :/
    // truncate
    if (pthread_create(&tid[0], NULL, function, &operations[0]) != 0)
        exit(EXIT_FAILURE);
    // append
    if (pthread_create(&tid[1], NULL, function, &operations[1]) != 0)
        exit(EXIT_FAILURE);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    tfs_copy_to_external_fs(shrek[8], "append-trunc1.txt");
    tfs_copy_to_external_fs(shrek[9], "append-trunc2.txt");
    tfs_destroy();
    printf("Successful test.\n");

    return 0;
}
void *function(void *input) {
    int num = *((int *)input);
    char *path = shrek[num + 8];

    char buffer[BUFFER_LEN];
    char letter = 'T' - (char)(num * ('T' - 'A'));

    memset(buffer, letter, BUFFER_LEN);

    printf("%c\n", letter);

    int f;
    ssize_t r;
    if (num) {
        f = tfs_open(path, TFS_O_APPEND);
    } else {
        f = tfs_open(path, TFS_O_TRUNC);
    }
    assert(f != -1);

    for (int i = 0; i < 15; i++) {
        /* read the contents of the file */
        r = tfs_write(f, buffer, BUFFER_LEN);
        assert(r == BUFFER_LEN);
    }
    return NULL;
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