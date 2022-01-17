#include "tecnicofs_client_api.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    (void)client_pipe_path;
    (void)server_pipe_path;
    /* TODO: Implement this */

    char buffer = TFS_OP_CODE_MOUNT;

    int pipe_out = open(server_pipe_path, O_WRONLY);
    if (pipe_out < 0) {
        perror("cannot open pipe");
        return -1;
    }

    write(pipe_out, &buffer, sizeof(char));

    close(pipe_out);

    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
}

int tfs_open(char const *name, int flags) {
    (void)name;
    (void)flags;
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    (void)fhandle;
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    (void)fhandle;
    (void)buffer;
    (void)len;
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    (void)fhandle;
    (void)buffer;
    (void)len;
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
