#include "tfs_server.h"
#include "operations.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

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

static worker_t workers[SIMULTANEOUS_CONNECTIONS];
static bool free_workers[SIMULTANEOUS_CONNECTIONS];

static int pipe_in;
static int pipe_out;

static char *pipename;

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, close_server_by_user);

    if (init_server() != 0) {
        printf("Failed to init server\n");
        return EXIT_FAILURE;
    }

    if (tfs_init() != 0) {
        printf("Failed to init tfs\n");
        return EXIT_FAILURE;
    }

    pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    if (unlink(pipename) != 0 && errno != ENOENT) {
        perror("Failed to delete pipe");
        exit(EXIT_FAILURE);
    }

    if (mkfifo(pipename, 0777) < 0) {
        perror("Failed to create pipe");
        exit(EXIT_FAILURE);
    }

    bool exit_server = false;

    while (!exit_server) {
        pipe_in = open(pipename, O_RDONLY);
        if (pipe_in < 0) {
            perror("Failed to open server pipe");
            unlink(pipename);

            exit(EXIT_FAILURE);
        }

        ssize_t bytes_read;
        char op_code;

        bytes_read = read(pipe_in, &op_code, sizeof(char));

        // main listener loop
        while (bytes_read > 0) {

            switch (op_code) {
            case TFS_OP_CODE_MOUNT:
                handle_tfs_mount();
                break;
            case TFS_OP_CODE_UNMOUNT:
                read_id_and_launch_function(handle_tfs_unmount);
                break;
            case TFS_OP_CODE_OPEN:
                read_id_and_launch_function(handle_tfs_open);
                break;
            case TFS_OP_CODE_CLOSE:
                read_id_and_launch_function(handle_tfs_close);
                break;
            case TFS_OP_CODE_WRITE:
                read_id_and_launch_function(handle_tfs_write);
                break;
            case TFS_OP_CODE_READ:
                read_id_and_launch_function(handle_tfs_read);
                break;
            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
                read_id_and_launch_function(
                    handle_tfs_shutdown_after_all_closed);
                exit_server = true;
                break;
            default:
                break;
            }
            bytes_read = read(pipe_in, &op_code, sizeof(char));
        }

        if (bytes_read < 0) {
            perror("Failed to read pipe");
            close(pipe_in);
            if (unlink(pipename) != 0) {
                perror("Failed to delete pipe");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_FAILURE);
        }
        close(pipe_in);
    }
    if (unlink(pipename) != 0) {
        perror("Failed to delete pipe");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int init_server() {
    for (int i = 0; i < SIMULTANEOUS_CONNECTIONS; ++i) {
        workers[i].session_id = i;
        mutex_init(&workers[i].lock);
        if (pthread_cond_init(&workers[i].cond, NULL) != 0) {
            return -1;
        }
        if (pthread_create(&workers[i].tid, NULL, session_worker,
                           &workers[i]) != 0) {
            return -1;
        }
        free_workers[i] = false;
    }
    return 0;
}

int get_available_worker() {
    for (int i = 0; i < SIMULTANEOUS_CONNECTIONS; ++i) {
        if (free_workers[i] == false) {
            free_workers[i] = true;
            return i;
        }
    }
    return -1;
}

int free_worker(int session_id) {
    if (free_workers[session_id] == false) {
        return -1;
    }
    free_workers[session_id] = false;
    return 0;
}

void handle_tfs_mount() {
    char client_pipe_name[PIPE_STRING_LENGTH];
    read_pipe(pipe_in, client_pipe_name, sizeof(char) * PIPE_STRING_LENGTH);
    // TODO handle read_pipe and pipe open errors (?)
    int session_id = get_available_worker();
    pipe_out = open(client_pipe_name, O_WRONLY);

    if (session_id < 0) {
        printf("The number of sessions was exceeded.\n");
    } else {
        printf("The session number %d was created with success.\n", session_id);
    }
    write_pipe(pipe_out, &session_id, sizeof(int));
}

void handle_tfs_unmount(int session_id) {

    // TODO just have a single session now, nothing to do
    int return_value = free_worker(session_id);
    write_pipe(pipe_out, &return_value, sizeof(int));
    close(pipe_out);

    printf("The session number %d was destroyed with success.\n", session_id);
}

void handle_tfs_open(int session_id) {
    (void)session_id;

    char file_name[PIPE_STRING_LENGTH];
    int flags;
    read_pipe(pipe_in, file_name, sizeof(char) * PIPE_STRING_LENGTH);
    read_pipe(pipe_in, &flags, sizeof(int));

    int result = tfs_open(file_name, flags);

    write_pipe(pipe_out, &result, sizeof(int));
}

void handle_tfs_close(int session_id) {
    (void)session_id;

    int fhandle;

    read_pipe(pipe_in, &fhandle, sizeof(int));

    int result = tfs_close(fhandle);

    write_pipe(pipe_out, &result, sizeof(int));
}

void handle_tfs_write(int session_id) {
    (void)session_id;
    int fhandle;
    size_t len;

    read_pipe(pipe_in, &fhandle, sizeof(int));
    read_pipe(pipe_in, &len, sizeof(size_t));

    char *buffer = malloc(sizeof(char) * len);
    read_pipe(pipe_in, buffer, sizeof(char) * len);

    int result = (int)tfs_write(fhandle, buffer, len);
    free(buffer);
    write_pipe(pipe_out, &result, sizeof(int));
}

void handle_tfs_read(int session_id) {
    (void)session_id;

    int fhandle;
    size_t len;

    read_pipe(pipe_in, &fhandle, sizeof(int));
    read_pipe(pipe_in, &len, sizeof(size_t));

    char *buffer = malloc(sizeof(char) * len);

    int result = (int)tfs_read(fhandle, buffer, len);

    write_pipe(pipe_out, &result, sizeof(int));

    if (result > 0) {
        // so far so good
        write_pipe(pipe_out, buffer, (sizeof(char) * (size_t)result));
    }
    free(buffer);
}

void handle_tfs_shutdown_after_all_closed(int session_id) {
    (void)session_id;
    int result = tfs_destroy_after_all_closed();

    write_pipe(pipe_out, &result, sizeof(int));
}

void close_server_by_user(int singnum) {
    (void)singnum;

    // todo handle session_id
    close(pipe_out);
    printf("\nSucessfully ended the server.\n");

    unlink(pipename);

    exit(0);
}

void read_id_and_launch_function(void fn(int)) {
    int session_id;
    read_pipe(pipe_in, &session_id, sizeof(int));
    fn(session_id);
}

void *session_worker(void *args) {
    worker_t *worker = (worker_t *)args;

    (void)worker;
    // TODO

    return NULL;
}
