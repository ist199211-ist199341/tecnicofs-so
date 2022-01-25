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
        return -1;                                                             \
    }

#define read_pipe(pipe, buffer, size)                                          \
    if (read(pipe, buffer, size) != size) {                                    \
        return -1;                                                             \
    }

void packetcpy(void *packet, size_t *packet_offset, void const *data,
               size_t size) {
    memcpy(packet + *packet_offset, data, size);
    *packet_offset += size;
}

static int pipe_in;
static int pipe_out;
static int session_id;

static char pipename[PIPE_STRING_LENGTH] = {0};

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    strcpy(pipename, client_pipe_path);
    unlink(pipename);

    if (mkfifo(pipename, 0777) < 0) {
        return -1;
    }

    pipe_out = open(server_pipe_path, O_WRONLY);
    if (pipe_out < 0) {
        return -1;
    }

    /* len = opcode (char) + pipename (char * PIPE_STRING_LENGTH) */

    size_t packet_len = sizeof(char) + sizeof(char) * PIPE_STRING_LENGTH;
    size_t packet_offset = 0;
    int8_t *packet = (int8_t *)malloc(packet_len);
    if (packet == NULL) {
        return -1;
    }

    char op_code = TFS_OP_CODE_MOUNT;

    packetcpy(packet, &packet_offset, &op_code, sizeof(char));
    packetcpy(packet, &packet_offset, pipename,
              sizeof(char) * PIPE_STRING_LENGTH);

    write_pipe(pipe_out, packet, packet_len);
    free(packet);

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
    /* len = opcode (char) + session_id (int) */

    size_t packet_len = sizeof(char) + sizeof(int);
    size_t packet_offset = 0;
    int8_t *packet = (int8_t *)malloc(packet_len);
    if (packet == NULL) {
        return -1;
    }

    char op_code = TFS_OP_CODE_UNMOUNT;

    packetcpy(packet, &packet_offset, &op_code, sizeof(char));
    packetcpy(packet, &packet_offset, &session_id, sizeof(int));

    write_pipe(pipe_out, packet, packet_len);
    free(packet);

    int return_value;
    read_pipe(pipe_in, &return_value, sizeof(int));

    close(pipe_out);
    close(pipe_in);

    unlink(pipename);

    return return_value;
}

int tfs_open(char const *name, int flags) {
    /* len = opcode (char) + session_id (int) + name (char[40]) + flags (int) */

    size_t packet_len =
        sizeof(char) + 2 * sizeof(int) + sizeof(char) * PIPE_STRING_LENGTH;
    size_t packet_offset = 0;
    int8_t *packet = (int8_t *)malloc(packet_len);
    if (packet == NULL) {
        return -1;
    }

    char op_code = TFS_OP_CODE_OPEN;
    char file_name[PIPE_STRING_LENGTH] = {0};
    strcpy(file_name, name);

    packetcpy(packet, &packet_offset, &op_code, sizeof(char));
    packetcpy(packet, &packet_offset, &session_id, sizeof(int));
    packetcpy(packet, &packet_offset, file_name,
              sizeof(char) * PIPE_STRING_LENGTH);
    packetcpy(packet, &packet_offset, &flags, sizeof(int));

    write_pipe(pipe_out, packet, packet_len);
    free(packet);

    int return_value;
    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}

int tfs_close(int fhandle) {
    /* len = opcode (char) + session_id (int) + fhandle (int) */

    size_t packet_len = sizeof(char) + 2 * sizeof(int);
    size_t packet_offset = 0;
    int8_t *packet = (int8_t *)malloc(packet_len);
    if (packet == NULL) {
        return -1;
    }

    char op_code = TFS_OP_CODE_CLOSE;

    packetcpy(packet, &packet_offset, &op_code, sizeof(char));
    packetcpy(packet, &packet_offset, &session_id, sizeof(int));
    packetcpy(packet, &packet_offset, &fhandle, sizeof(int));

    write_pipe(pipe_out, packet, packet_len);
    free(packet);

    int return_value;
    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* len = opcode (char) + session_id (int) + fhandle (int) + len (size_t) +
     * content (char[len]) */

    size_t packet_len =
        sizeof(char) + 2 * sizeof(int) + sizeof(size_t) + sizeof(char) * len;
    size_t packet_offset = 0;
    int8_t *packet = (int8_t *)malloc(packet_len);
    if (packet == NULL) {
        return -1;
    }

    char op_code = TFS_OP_CODE_WRITE;

    packetcpy(packet, &packet_offset, &op_code, sizeof(char));
    packetcpy(packet, &packet_offset, &session_id, sizeof(int));
    packetcpy(packet, &packet_offset, &fhandle, sizeof(int));
    packetcpy(packet, &packet_offset, &len, sizeof(size_t));
    packetcpy(packet, &packet_offset, buffer, sizeof(char) * len);

    write_pipe(pipe_out, packet, packet_len);
    free(packet);

    int return_value;

    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* len = opcode (char) + session_id (int) + fhandle (int) + len (size_t) */

    size_t packet_len = sizeof(char) + 2 * sizeof(int) + sizeof(size_t);
    size_t packet_offset = 0;
    int8_t *packet = (int8_t *)malloc(packet_len);
    if (packet == NULL) {
        return -1;
    }

    char op_code = TFS_OP_CODE_READ;

    packetcpy(packet, &packet_offset, &op_code, sizeof(char));
    packetcpy(packet, &packet_offset, &session_id, sizeof(int));
    packetcpy(packet, &packet_offset, &fhandle, sizeof(int));
    packetcpy(packet, &packet_offset, &len, sizeof(size_t));

    write_pipe(pipe_out, packet, packet_len);
    free(packet);

    int bytes_read;
    read_pipe(pipe_in, &bytes_read, sizeof(int));
    if (bytes_read > 0) {
        read_pipe(pipe_in, buffer, sizeof(char) * (size_t)bytes_read);
    }

    return (ssize_t)bytes_read;
}

int tfs_shutdown_after_all_closed() {
    /* len = opcode (char) + session_id (int) */

    size_t packet_len = sizeof(char) + sizeof(int);
    size_t packet_offset = 0;
    int8_t *packet = (int8_t *)malloc(packet_len);
    if (packet == NULL) {
        return -1;
    }

    char op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;

    packetcpy(packet, &packet_offset, &op_code, sizeof(char));
    packetcpy(packet, &packet_offset, &session_id, sizeof(int));

    write_pipe(pipe_out, packet, packet_len);
    free(packet);

    int return_value;
    read_pipe(pipe_in, &return_value, sizeof(int));

    return return_value;
}
