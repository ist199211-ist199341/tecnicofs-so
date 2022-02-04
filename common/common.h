#ifndef COMMON_H
#define COMMON_H

#include <limits.h>
#include <sys/types.h>

/* tfs_open flags */
enum {
    TFS_O_CREAT = 0b001,
    TFS_O_TRUNC = 0b010,
    TFS_O_APPEND = 0b100,
};

/* operation codes (for client-server requests) */
enum {
    TFS_OP_CODE_MOUNT = 1,
    TFS_OP_CODE_UNMOUNT = 2,
    TFS_OP_CODE_OPEN = 3,
    TFS_OP_CODE_CLOSE = 4,
    TFS_OP_CODE_WRITE = 5,
    TFS_OP_CODE_READ = 6,
    TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED = 7
};

#define PIPE_STRING_LENGTH (40)

#define PIPE_BUFFER_MAX_LEN (PIPE_BUF)

/*
 * Same as POSIX's read, but handles EINTR correctly.
 */
ssize_t try_read(int fd, void *buf, size_t count);

/*
 * Same as POSIX's write, but handles EINTR correctly.
 */
ssize_t try_write(int fd, const void *buf, size_t count);

/* check if all the content was written to the pipe. */
#define write_pipe(pipe, buffer, size)                                         \
    if (try_write(pipe, buffer, size) != size) {                               \
        return -1;                                                             \
    }

/* check if all the content was read from the pipe. */
#define read_pipe(pipe, buffer, size)                                          \
    if (try_read(pipe, buffer, size) != size) {                                \
        return -1;                                                             \
    }

#endif /* COMMON_H */
