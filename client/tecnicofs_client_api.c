#include "tecnicofs_client_api.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int pipe_in;
static int pipe_out;
static int session_id;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {

    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_MOUNT;
    char pipename[40];

    strcpy(pipename, client_pipe_path);

    unlink(pipename);

    if (mkfifo(pipename, 0777) < 0) {
        return -1;
    }

    pipe_out = open(server_pipe_path, O_WRONLY);
    if (pipe_out < 0) {
        return -1;
    }

    write(pipe_out, &op_code, sizeof(char));
    write(pipe_out, pipename, sizeof(char) * 40);

    pipe_in = open(client_pipe_path, O_RDONLY);
    if (pipe_in < 0) {
        return -1;
    }

    read(pipe_in, &session_id, sizeof(int));

    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_UNMOUNT;

    int return_value;

    write(pipe_out, &op_code, sizeof(char));
    write(pipe_out, &session_id, sizeof(int));

    read(pipe_in, &return_value, sizeof(int));

    close(pipe_out);
    close(pipe_in);

    return return_value;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_OPEN;

    char buffer[40];

    strcpy(buffer, name);

    int return_value;

    write(pipe_out, &op_code, sizeof(char));
    write(pipe_out, &session_id, sizeof(int));
    write(pipe_out, buffer, sizeof(char) * 40);
    write(pipe_out, &flags, sizeof(int));

    read(pipe_in, &return_value, sizeof(int));

    return return_value;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_CLOSE;

    int return_value;

    write(pipe_out, &op_code, sizeof(char));
    write(pipe_out, &session_id, sizeof(int));
    write(pipe_out, &fhandle, sizeof(int));

    read(pipe_in, &return_value, sizeof(int));

    return return_value;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {

    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_WRITE;

    int return_value;

    write(pipe_out, &op_code, sizeof(char));
    write(pipe_out, &session_id, sizeof(int));
    write(pipe_out, &fhandle, sizeof(int));
    write(pipe_out, &len, sizeof(size_t));
    write(pipe_out, buffer, sizeof(char) * len);

    read(pipe_in, &return_value, sizeof(int));
    return return_value;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_READ;

    int bytes_read;

    write(pipe_out, &op_code, sizeof(char));
    write(pipe_out, &session_id, sizeof(int));
    write(pipe_out, &fhandle, sizeof(int));
    write(pipe_out, &len, sizeof(size_t));

    read(pipe_in, &bytes_read, sizeof(int));

    printf("result: %d\n", bytes_read);
    fflush(stdout);

    char test[bytes_read];

    if (bytes_read > 0)
        read(pipe_out, test, sizeof(char) * (size_t)bytes_read);

    printf("buff: %s\n", (char *)test);

    strcpy(buffer, test);
    return (ssize_t)bytes_read;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */

    char op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;

    int return_value;

    write(pipe_out, &op_code, sizeof(char));
    write(pipe_out, &session_id, sizeof(int));

    read(pipe_in, &return_value, sizeof(int));

    return return_value;
}
