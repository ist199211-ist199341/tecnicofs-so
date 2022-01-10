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

#define BUFFER_LEN 120

#define BUFFER_LEN 120
#define THREAD_COUNT 8
#define REPEAT_COUNT 100
#define FILL_COUNT (BUFFER_LEN * REPEAT_COUNT)

int file_id;

char *input_files[] = {
    "input1.txt",
    "input2.txt",
    "input3.txt",
    "input4.txt",
};
char *tfs_files[] = {"/f1", "/f2", "/f3", "/f4"};
int mode[] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1}; // 0 = append, 1 = truncate

int write_to_file();
void *truncate_or_append(void *input);
void verify_append(char *source_file, char *tfs_file);

/* Tests if the TFS files are copied to the external FS concurrently. After
 * copying, compares the contents between the original and the copy, deleting
 * the copy. */
int main() {
    assert(tfs_init() != -1);

    file_id = write_to_file();

    int array[THREAD_COUNT];

    pthread_t tid[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; ++i) {
        array[i] = i;
        assert(pthread_create(&tid[i], NULL, truncate_or_append, &array[i]) ==
               0);
    }

    for (int i = 0; i < THREAD_COUNT; ++i) {
        assert(pthread_join(tid[i], NULL) == 0);
    }

    tfs_close(file_id);

    tfs_copy_to_external_fs(tfs_files[0], "a.txt");

    printf("Successful test.\n");
    assert(tfs_destroy() == 0);
    return 0;
}

int write_to_file() {

    // open source file
    FILE *fd = fopen(input_files[file_id], "r");
    assert(fd != NULL);

    char buffer[BUFFER_LEN];
    char *path = tfs_files[file_id];

    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    /* read the contents of the file */
    ssize_t r;
    size_t bytes_read = fread(buffer, sizeof(char), BUFFER_LEN, fd);

    while (bytes_read > 0) {
        r = tfs_write(f, buffer, bytes_read);
        assert(r == bytes_read);
        bytes_read = fread(buffer, sizeof(char), BUFFER_LEN, fd);
    }

    assert(fclose(fd) == 0);
    return f;
}

void *truncate_or_append(void *input) {
    int thread_num = *((int *)input);

    char buffer[BUFFER_LEN];
    // A for append (mode = 0), T for truncate (mode = 1)
    int letter = 'A' + thread_num;

    memset(buffer, letter, BUFFER_LEN);
    buffer[BUFFER_LEN - 1] = '\n';
    ssize_t r;

    for (int i = 0; i < REPEAT_COUNT; i++) {
        /* write the contents of the file */
        r = tfs_write(file_id, buffer, BUFFER_LEN);
        assert(r == BUFFER_LEN);
    }

    buffer[BUFFER_LEN - 2] = '\n';
    r = tfs_write(file_id, buffer, BUFFER_LEN);
    assert(r == BUFFER_LEN);
    return NULL;
}

void *write_from_external(void *input) {
    int dummy = *((int *)input);
    dummy++;

    FILE *fd = fopen(input_files[file_id], "r");
    assert(fd != NULL);

    char buffer[BUFFER_LEN];

    int f = tfs_open(tfs_files[file_id], TFS_O_CREAT | TFS_O_TRUNC);
    assert(f != -1);

    ssize_t r;
    size_t bytes_read = fread(buffer, sizeof(char), BUFFER_LEN, fd);

    while (bytes_read > 0) {
        /* write the contents of the file */
        r = tfs_write(f, buffer, bytes_read);
        assert(r == bytes_read);
        bytes_read = fread(buffer, sizeof(char), BUFFER_LEN, fd);
    }

    assert(tfs_close(f) == 0);
    assert(fclose(fd) == 0);

    return NULL;
}

void verify_truncate(char *tfs_file) {
    char buffer[BUFFER_LEN];
    char buffer_control[BUFFER_LEN];
    memset(buffer_control, 'T', BUFFER_LEN);

    int f;
    f = tfs_open(tfs_file, 0);
    assert(f != -1);

    ssize_t total_read = 0;
    ssize_t r = tfs_read(f, buffer, BUFFER_LEN);
    while (r > 0) {
        /* read the contents of the file */
        total_read += r;
        assert(memcmp(buffer_control, buffer, (size_t)r) == 0);
        r = tfs_read(f, buffer, BUFFER_LEN);
    }

    assert(total_read == FILL_COUNT);

    assert(tfs_close(f) == 0);
}

void verify_append(char *source_file, char *tfs_file) {
    FILE *fd = fopen(source_file, "r");
    assert(fd != NULL);

    char buffer_external[BUFFER_LEN];
    char buffer_tfs[BUFFER_LEN];
    char buffer_control[BUFFER_LEN];
    memset(buffer_control, 'A', BUFFER_LEN);

    int f = tfs_open(tfs_file, 0);
    assert(f != -1);

    size_t total_read = 0; // total 'A's read
    size_t bytes_read_external =
        fread(buffer_external, sizeof(char), BUFFER_LEN, fd);
    ssize_t bytes_read_tfs = tfs_read(f, buffer_tfs, BUFFER_LEN);
    while (bytes_read_tfs > 0) {
        assert(strncmp(buffer_external, buffer_tfs, bytes_read_external) == 0);
        if (bytes_read_external < BUFFER_LEN) {
            // reached the part of 'A's
            assert(memcmp(buffer_control, buffer_tfs + bytes_read_external,
                          (size_t)bytes_read_tfs - bytes_read_external) == 0);
            total_read += (size_t)bytes_read_tfs - bytes_read_external;
            bytes_read_external = 0;
        } else {
            bytes_read_external =
                fread(buffer_external, sizeof(char), BUFFER_LEN, fd);
        }
        bytes_read_tfs = tfs_read(f, buffer_tfs, BUFFER_LEN);
    }

    assert(total_read == FILL_COUNT);
    assert(bytes_read_external == 0);
    assert(bytes_read_tfs == 0);

    assert(tfs_close(f) == 0);
    assert(fclose(fd) == 0);
}
