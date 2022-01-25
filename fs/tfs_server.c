#include "tfs_server.h"
#include "operations.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
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

        packet_t packet;

        bytes_read = read(pipe_in, &packet, sizeof(packet));

        while (bytes_read > 0) {
            // read op code

            int op_code;
            int session_id;

            op_code = packet.opcode;

            if (op_code == TFS_OP_CODE_MOUNT) {
                // read pipe_out
                session_id = get_available_worker();

                if (session_id == -1) {
                    exit(EXIT_FAILURE);
                }
                workers[session_id].to_execute = TFS_OP_CODE_MOUNT;

            } else {

                // read session_id
                session_id = packet.session_id;

                workers[session_id].to_execute = op_code;
            }
            workers[session_id].packet = packet;

            pthread_cond_signal(&workers[session_id].cond);

            bytes_read = read(pipe_in, &packet, sizeof(packet));
        }

        if (bytes_read < 0) {
            perror("Failed to read pipe");
            close(pipe_in);
            if (unlink(pipename) != 0) {
                perror("Failed to delete pipe");
            }
            exit(EXIT_FAILURE);
        }
        if (close(pipe_in) < 0) {
            perror("Failed to close pipe");
            exit(EXIT_FAILURE);
        }
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
        workers[i].to_execute = 0;
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

void close_server_by_user(int singnum) {
    (void)singnum;

    // todo handle session_id
    printf("\nSucessfully ended the server.\n");

    unlink(pipename);

    exit(0);
}

void *session_worker(void *args) {
    worker_t *worker = (worker_t *)args;
    while (1) {
        mutex_lock(&worker->lock);

        while (worker->to_execute == 0) {
            pthread_cond_wait(&worker->cond, &worker->lock);
        }

        switch (worker->to_execute) {

        case TFS_OP_CODE_MOUNT:
            handle_tfs_mount(worker);
            break;
        case TFS_OP_CODE_UNMOUNT:
            handle_tfs_unmount(worker);
            break;
        case TFS_OP_CODE_OPEN:
            handle_tfs_open_worker(worker);
            break;
        case TFS_OP_CODE_CLOSE:
            handle_tfs_close(worker);
            break;
        case TFS_OP_CODE_WRITE:
            handle_tfs_write(worker);
            break;
        case TFS_OP_CODE_READ:
            handle_tfs_read(worker);
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
            handle_tfs_shutdown_after_all_closed(worker);
            break;
        default:
            break;
        }
        worker->to_execute = 0;
        mutex_unlock(&worker->lock);
    }
}
void handle_tfs_mount(worker_t *worker) {

    char pipe_client[PIPE_STRING_LENGTH];
    int result;

    strcpy(pipe_client, worker->packet.client_pipe);

    int pipe_out = open(pipe_client, O_WRONLY);
    if (pipe_out < 0) {
        perror("Failed to open server pipe");
        unlink(pipename);

        exit(EXIT_FAILURE);
    }

    worker->pipe_out = pipe_out;
    result = 0;
    write_pipe(worker->pipe_out, &result, sizeof(int));
}

void handle_tfs_unmount(worker_t *worker) {
    // TODO
    int result = 0;

    write_pipe(worker->pipe_out, &result, sizeof(int));

    close(worker->pipe_out);

    if (free_worker(worker->session_id) == -1) {
        perror("Failed to free worker");
        exit(EXIT_FAILURE);
    }
}

void handle_tfs_open_worker(worker_t *worker) {

    char name[MAX_FILE_NAME];

    int flags;

    strcpy(name, worker->packet.file_name);

    flags = worker->packet.flags;

    int result = tfs_open(name, flags);

    write_pipe(worker->pipe_out, &result, sizeof(int));
}

void handle_tfs_close(worker_t *worker) {
    int fhandle;

    fhandle = worker->packet.fhandle;

    int result = tfs_close(fhandle);

    write_pipe(worker->pipe_out, &result, sizeof(int));
}

void handle_tfs_write(worker_t *worker) {
    int fhandle;
    size_t len;

    fhandle = worker->packet.fhandle;

    len = worker->packet.len;

    char buffer[len];

    strcpy(buffer, worker->packet.buffer);

    size_t result = (size_t)tfs_write(fhandle, buffer, len);

    write_pipe(worker->pipe_out, &result, sizeof(int));
}

void handle_tfs_read(worker_t *worker) {
    int fhandle;
    size_t len;

    fhandle = worker->packet.fhandle;

    len = worker->packet.len;

    char buffer[len];

    int result = (int)tfs_read(fhandle, buffer, len);

    write_pipe(worker->pipe_out, &result, sizeof(int));
    write_pipe(worker->pipe_out, &buffer, (size_t)result * sizeof(char));
}

void handle_tfs_shutdown_after_all_closed(worker_t *worker) {
    int result = tfs_destroy_after_all_closed();

    write_pipe(worker->pipe_out, &result, sizeof(int));
}

int buffer_write(buffer_t *buffer, void *data, size_t size) {

    if (buffer->offset + size > PIPE_BUFFER_MAX_LEN) {
        return -1;
    }
    memcpy(buffer->data + buffer->offset, data, size);
    buffer->offset += size;
    return 0;
}

void buffer_read(buffer_t *buffer, void *data, size_t size) {
    memcpy(data, buffer->data + buffer->offset, size);
    buffer->offset += size;
}
