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

    char *shrek[8] = {"shrek1.txt",    "shrek2.txt",    "shrek3.txt",
                      "shrek4.txt",    "shrek1-fs.txt", "shrek2-fs.txt",
                      "shrek3-fs.txt", "shrek4-fs.txt"};
    int number;
    printf("Qual filme do Shrek queres obter?(1-4)\n");
    scanf("%d", &number);
    if (number <= 0 || number >= 5) {
        printf("There's no Shrek movie with that number\n");
        return 0;
    }
    number--;
    FILE *fd = fopen(shrek[number], "r");

    char *path = "/f1";
    char buffer[BUFFER_LEN];

    assert(tfs_init() != -1);

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

    tfs_copy_to_external_fs(path, shrek[number + 4]);
    printf("diff %s %s\n", shrek[number], shrek[number + 4]);

    return 0;
}
