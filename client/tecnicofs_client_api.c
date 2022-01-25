#include "tecnicofs_client_api.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define write_pipe(pipe, buffer, size)                                         \
    if (write(pipe, buffer, size) != size) {                                   \
        perror("Failed to write to pipe");                                     \
        exit(EXIT_FAILURE);                                                    \
    }

#define read_pipe(pipe, buffer, size)                                          \
    if (read(pipe, buffer, size) != size) {                                    \
        perror("Failed to read from pipe");                                    \
        exit(EXIT_FAILURE);                                                    \
    }

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

    packet_t packet;

    packet.opcode = op_code;
    strcpy(packet.client_pipe, pipename);

    write_pipe(pipe_out, &packet, sizeof(packet));

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

    int op_code = TFS_OP_CODE_UNMOUNT;

    int return_value;

    packet_t packet;

    packet.opcode = op_code;
    packet.session_id = session_id;

    write_pipe(pipe_out, &packet, sizeof(packet));

    read_pipe(pipe_in, &return_value, sizeof(int));

    close(pipe_out);
    close(pipe_in);

    unlink(pipename);

    return return_value;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */

    packet_t packet;

    packet.opcode = TFS_OP_CODE_OPEN;
    packet.session_id = session_id;

    strcpy(packet.file_name, name);
    packet.flags = flags;

    write_pipe(pipe_out, &packet, sizeof(packet));

    int return_value;
    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */

    int op_code = TFS_OP_CODE_CLOSE;

    packet_t packet;

    packet.opcode = op_code;
    packet.session_id = session_id;
    packet.fhandle = fhandle;

    write_pipe(pipe_out, &packet, sizeof(packet));

    int return_value;

    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {

    /* TODO: Implement this */

    int op_code = TFS_OP_CODE_WRITE;

    int return_value;

    packet_t packet;

    packet.opcode = op_code;
    packet.session_id = session_id;
    packet.fhandle = fhandle;
    packet.len = len;
    strcpy(packet.buffer, buffer);

    write_pipe(pipe_out, &packet, sizeof(packet));

    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    /* TODO: Implement this */

    (void)buffer;

    int op_code = TFS_OP_CODE_READ;

    size_t bytes_read;

    packet_t packet;

    packet.opcode = op_code;
    packet.session_id = session_id;
    packet.fhandle = fhandle;
    packet.len = len;

    write_pipe(pipe_out, &packet, sizeof(packet));

    read_pipe(pipe_in, &bytes_read, sizeof(int));

    read_pipe(pipe_in, buffer, sizeof(char) * bytes_read);

    return (ssize_t)bytes_read;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */

    int op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;

    int return_value;

    packet_t packet;

    packet.opcode = op_code;
    packet.session_id = session_id;

    write_pipe(pipe_out, &packet, sizeof(packet));

    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}
