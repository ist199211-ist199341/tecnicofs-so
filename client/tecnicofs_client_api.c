#include "tecnicofs_client_api.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define write_pipe(pipe, buffer, size)                                         \
    if (write(pipe, buffer, size) != size)                                     \
        return -1;

#define read_pipe(pipe, buffer, size)                                          \
    if (read(pipe, buffer, size) != size)                                      \
        return -1;

static int pipe_in;
static int pipe_out;
static int session_id;

static char pipename[PIPE_STRING_LENGTH];

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {

    /* TODO: Implement this */

    int op_code = TFS_OP_CODE_MOUNT;

    strcpy(pipename, client_pipe_path);

    unlink(pipename);

    if (mkfifo(pipename, 0777) < 0) {
        return -1;
    }

    pipe_out = open(server_pipe_path, O_WRONLY);
    if (pipe_out < 0) {
        return -1;
    }

    write_pipe(pipe_out, &op_code, sizeof(int));
    write_pipe(pipe_out, pipename, sizeof(char) * PIPE_STRING_LENGTH);

    pipe_in = open(client_pipe_path, O_RDONLY);
    if (pipe_in < 0) {
        return -1;
    }

    read_pipe(pipe_in, &session_id, sizeof(int));

    if (session_id == -1) {
        close(pipe_in);
        unlink(pipename);

        return -1;
    }

    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_UNMOUNT;

    int return_value;

    write_pipe(pipe_out, &op_code, sizeof(char));
    write_pipe(pipe_out, &session_id, sizeof(int));

    read_pipe(pipe_in, &return_value, sizeof(int));

    close(pipe_out);
    close(pipe_in);

    unlink(pipename);

    return return_value;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */

    // DIVERTE_TE A FZR ISTO

    char buffer[PIPE_BUFFER_MAX_LEN];

    char dummy[4];

    char name1[PIPE_STRING_LENGTH];

    for (int i = 0; i < PIPE_STRING_LENGTH; i++) {
        name1[i] = 0;
    }

    snprintf(dummy, sizeof(int), "%d", TFS_OP_CODE_OPEN);

    strcat(buffer, dummy);

    snprintf(dummy, sizeof(int), "%d", session_id);

    strcat(buffer, dummy);

    strcpy(name1, name);
    strcat(buffer, name1);

    snprintf(dummy, sizeof(int), "%d", flags);
    strcat(buffer, dummy);

    write_pipe(pipe_out, buffer, sizeof(char) * PIPE_BUFFER_MAX_LEN);

    int return_value;
    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_CLOSE;

    int return_value;

    write_pipe(pipe_out, &op_code, sizeof(char));
    write_pipe(pipe_out, &session_id, sizeof(int));
    write_pipe(pipe_out, &fhandle, sizeof(int));

    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {

    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_WRITE;

    int return_value;

    write_pipe(pipe_out, &op_code, sizeof(char));
    write_pipe(pipe_out, &session_id, sizeof(int));
    write_pipe(pipe_out, &fhandle, sizeof(int));
    write_pipe(pipe_out, &len, sizeof(size_t));
    write_pipe(pipe_out, buffer, sizeof(char) * len);

    read_pipe(pipe_in, &return_value, sizeof(int));
    return return_value;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_READ;

    int bytes_read;

    write_pipe(pipe_out, &op_code, sizeof(char));
    write_pipe(pipe_out, &session_id, sizeof(int));
    write_pipe(pipe_out, &fhandle, sizeof(int));
    write_pipe(pipe_out, &len, sizeof(size_t));

    read_pipe(pipe_in, &bytes_read, sizeof(int));

    if (bytes_read > 0)
        read_pipe(pipe_in, buffer, sizeof(char) * (size_t)bytes_read);

    return (ssize_t)bytes_read;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;

    int return_value;

    write_pipe(pipe_out, &op_code, sizeof(char));
    write_pipe(pipe_out, &session_id, sizeof(int));

    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}
