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

#define FILE_NAME_MAX_LEN 10
#define FILES_TO_CREATE 23

void *create_file(void *arg);

int main() {

    pthread_t tid[FILES_TO_CREATE];
    assert(tfs_init() != -1);

    for (int i = 0; i < FILES_TO_CREATE; ++i) {
        int *file_i = malloc(sizeof(int));
        *file_i = i + 1;
        if (pthread_create(&tid[i], NULL, create_file, file_i) != 0) {
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < FILES_TO_CREATE; ++i) {
        pthread_join(tid[i], NULL);
    }

    tfs_destroy();
    printf("Successful test.\n");

    return 0;
}

void *create_file(void *arg) {
    int file_i = *((int *)arg);
    free(arg);
    sleep(1);

    char path[FILE_NAME_MAX_LEN] = {'/'};
    sprintf(path + 1, "%d", file_i);

    printf("%s\n", path);

    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    printf("f = %d\n", f);

    tfs_close(f);

    printf("%d closed\n", file_i);

    return NULL;
}
