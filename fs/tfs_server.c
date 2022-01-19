#include "operations.h"
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

static int pipe_in;
static int pipe_out;

void handle_tfs_mount();
void handle_tfs_unmount(int session_id);
void handle_tfs_open(int session_id);
void handle_tfs_close(int session_id);
void handle_tfs_write(int session_id);
void handle_tfs_read(int session_id);
void handle_tfs_shutdown_after_all_closed(int session_id);

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    if (tfs_init() == 1) {
        printf("Failed to init tfs\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    unlink(pipename);

    if (mkfifo(pipename, 0777) < 0) {
        perror("Failed to create pipe");
        exit(EXIT_FAILURE);
    }

    bool exit_server = false;

    while (!exit_server) {

        pipe_in = open(pipename, O_RDONLY);
        if (pipe_in < 0) {
            printf("sadge\n");
            perror("Failed to open server pipe");
            exit(EXIT_FAILURE);
        }

        ssize_t bytes_read = 0;
        char op_code;
        // main listener loop
        while (bytes_read == 0) {
            bytes_read = read(pipe_in, &op_code, sizeof(char));
        }

        if (bytes_read < 0) {
            perror("Failed to read");
        }

        if (bytes_read > 0) {
            switch (op_code) {
            case TFS_OP_CODE_MOUNT:
                handle_tfs_mount();
                break;
            case TFS_OP_CODE_UNMOUNT:
                handle_tfs_unmount(0);
                break;
            case TFS_OP_CODE_OPEN:
                handle_tfs_open(0);
                break;
            case TFS_OP_CODE_CLOSE:
                handle_tfs_close(0);
                break;
            case TFS_OP_CODE_WRITE:
                handle_tfs_write(0);
                break;
            case TFS_OP_CODE_READ:
                handle_tfs_read(0);
                break;
            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
                handle_tfs_shutdown_after_all_closed(0);
                exit_server = true;
                break;
            default:
                break;
            }
        }
    }
    printf("died: %d\n", pipe_in);

    unlink(pipename);

    return 0;
}

void handle_tfs_mount() {
    char client_pipe_name[PIPE_STRING_LENGTH];
    read(pipe_in, client_pipe_name, sizeof(char) * PIPE_STRING_LENGTH);
    // TODO handle read and pipe open errors (?)
    pipe_out = open(client_pipe_name, O_WRONLY);

    // FIXME for now, session id is always 0
    int session_id = 0;
    write(pipe_out, &session_id, sizeof(int));
}

void handle_tfs_unmount(int session_id) {
    (void)session_id;

    // TODO just have a single session now, nothing to do

    int return_value = 0;
    write(pipe_out, &return_value, sizeof(int));
    close(pipe_out);
}

void handle_tfs_open(int session_id) {
    (void)session_id;

    char file_name[PIPE_STRING_LENGTH];
    int flags;
    read(pipe_in, &session_id, sizeof(int));
    read(pipe_in, file_name, sizeof(char) * PIPE_STRING_LENGTH);
    read(pipe_in, &flags, sizeof(int));

    int result = tfs_open(file_name, flags);

    write(pipe_out, &result, sizeof(int));
}

void handle_tfs_close(int session_id) {
    (void)session_id;

    int fhandle;

    read(pipe_in, &session_id, sizeof(int));

    read(pipe_in, &fhandle, sizeof(int));

    int result = tfs_close(fhandle);

    write(pipe_out, &result, sizeof(int));
}

void handle_tfs_write(int session_id) {
    (void)session_id;

    int fhandle;
    size_t len;

    read(pipe_in, &session_id, sizeof(int));
    read(pipe_in, &fhandle, sizeof(int));
    read(pipe_in, &len, sizeof(size_t));

    char *buffer = malloc(sizeof(char) * len);
    read(pipe_in, buffer, sizeof(char) * len);

    int result = (int)tfs_write(fhandle, buffer, len);
    free(buffer);
    write(pipe_out, &result, sizeof(int));
}

void handle_tfs_read(int session_id) {
    (void)session_id;

    int fhandle;
    size_t len;

    read(pipe_in, &session_id, sizeof(int));

    read(pipe_in, &fhandle, sizeof(int));
    read(pipe_in, &len, sizeof(size_t));

    char *buffer = malloc(sizeof(char) * len);

    int result = (int)tfs_read(fhandle, buffer, len);

    write(pipe_out, &result, sizeof(int));

    if (result > 0) {
        // so far so good
        write(pipe_out, buffer, (sizeof(char) * (size_t)result));
    }
}

void handle_tfs_shutdown_after_all_closed(int session_id) {
    (void)session_id;

    int result = /*tfs_destroy_after_all_closed();*/ 0;

    // TODO shutdown server
    printf("TODO shutdown\n");

    write(pipe_out, &result, sizeof(int));
}
