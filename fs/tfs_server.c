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
static pthread_mutex_t free_worker_lock;

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
        char op_code;

        bytes_read = read(pipe_in, &op_code, sizeof(char));

        // main listener loop
        while (bytes_read > 0) {

            switch (op_code) {
            case TFS_OP_CODE_MOUNT:
                handle_tfs_mount();
                break;
            case TFS_OP_CODE_UNMOUNT:
                wrap_packet_parser_fn(NULL, op_code);
                break;
            case TFS_OP_CODE_OPEN:
                wrap_packet_parser_fn(parse_tfs_open_packet, op_code);
                break;
            case TFS_OP_CODE_CLOSE:
                wrap_packet_parser_fn(parse_tfs_close_packet, op_code);
                break;
            case TFS_OP_CODE_WRITE:
                wrap_packet_parser_fn(parse_tfs_write_packet, op_code);
                break;
            case TFS_OP_CODE_READ:
                wrap_packet_parser_fn(parse_tfs_read_packet, op_code);
                break;
            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
                wrap_packet_parser_fn(NULL, op_code);
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
        workers[i].to_execute = false;
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
    mutex_init(&free_worker_lock);
    return 0;
}

int get_available_worker() {
    mutex_lock(&free_worker_lock);
    for (int i = 0; i < SIMULTANEOUS_CONNECTIONS; ++i) {
        if (free_workers[i] == false) {
            free_workers[i] = true;
            mutex_unlock(&free_worker_lock);
            return i;
        }
    }
    mutex_unlock(&free_worker_lock);
    printf("ALL workers are full\n");
    return -1;
}

int free_worker(int session_id) {
    mutex_lock(&free_worker_lock);
    if (free_workers[session_id] == false) {
        mutex_unlock(&free_worker_lock);
        return -1;
    }
    free_workers[session_id] = false;
    mutex_unlock(&free_worker_lock);

    return 0;
}

int handle_tfs_mount() {
    char client_pipe_name[PIPE_STRING_LENGTH];
    read_pipe(pipe_in, client_pipe_name, sizeof(char) * PIPE_STRING_LENGTH);

    int session_id = get_available_worker();
    int pipe_out = open(client_pipe_name, O_WRONLY);

    if (session_id < 0) {
        printf("The number of sessions was exceeded.\n");
    } else {
        printf("The session number %d was created with success.\n", session_id);
        workers[session_id].pipe_out = pipe_out;
    }

    write_pipe(pipe_out, &session_id, sizeof(int));
    return 0;
}

int parse_tfs_open_packet(worker_t *worker) {
    read_pipe(pipe_in, &worker->packet.file_name,
              sizeof(char) * PIPE_STRING_LENGTH);
    read_pipe(pipe_in, &worker->packet.flags, sizeof(int));

    return 0;
}

int parse_tfs_close_packet(worker_t *worker) {
    read_pipe(pipe_in, &worker->packet.fhandle, sizeof(int));

    return 0;
}

int parse_tfs_write_packet(worker_t *worker) {
    read_pipe(pipe_in, &worker->packet.fhandle, sizeof(int));
    read_pipe(pipe_in, &worker->packet.len, sizeof(size_t));
    char *buffer = (char *)malloc(worker->packet.len * sizeof(char));
    read_pipe(pipe_in, buffer, worker->packet.len * sizeof(char));
    worker->packet.buffer = buffer;

    return 0;
}

int parse_tfs_read_packet(worker_t *worker) {
    read_pipe(pipe_in, &worker->packet.fhandle, sizeof(int));
    read_pipe(pipe_in, &worker->packet.len, sizeof(size_t));

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
    while (true) {
        mutex_lock(&worker->lock);

        while (!worker->to_execute) {
            pthread_cond_wait(&worker->cond, &worker->lock);
        }

        switch (worker->packet.opcode) {

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
        worker->to_execute = false;
        mutex_unlock(&worker->lock);
    }
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
    packet_t *packet = &worker->packet;

    int result = tfs_open(packet->file_name, packet->flags);
    write_pipe(worker->pipe_out, &result, sizeof(int));
}

void handle_tfs_close(worker_t *worker) {
    packet_t *packet = &worker->packet;

    int result = tfs_close(packet->fhandle);
    write_pipe(worker->pipe_out, &result, sizeof(int));
}

void handle_tfs_write(worker_t *worker) {
    packet_t *packet = &worker->packet;

    int result = (int)tfs_write(packet->fhandle, packet->buffer, packet->len);
    fflush(stdout);
    write_pipe(worker->pipe_out, &result, sizeof(int));

    free(worker->packet.buffer);
}

void handle_tfs_read(worker_t *worker) {
    packet_t *packet = &worker->packet;
    char *buffer = (char *)malloc(sizeof(char) * packet->len);

    int result = (int)tfs_read(packet->fhandle, buffer, packet->len);

    write_pipe(worker->pipe_out, &result, sizeof(int));
    if (result > 0) {
        write_pipe(worker->pipe_out, buffer, (size_t)result * sizeof(char));
    }
    free(buffer);
}

void handle_tfs_shutdown_after_all_closed(worker_t *worker) {
    int result = tfs_destroy_after_all_closed();

    write_pipe(worker->pipe_out, &result, sizeof(int));
}

int wrap_packet_parser_fn(int parser_fn(worker_t *), char op_code) {
    int session_id;
    read_pipe(pipe_in, &session_id, sizeof(int));

    worker_t *worker = &workers[session_id];
    mutex_lock(&worker->lock);
    worker->packet.opcode = op_code;

    worker->to_execute = true;

    int result = 0;
    if (parser_fn != NULL) {
        result = parser_fn(worker);
    }

    if (pthread_cond_signal(&worker->cond) != 0) {
        perror("Couldn't signal worker");
        exit(EXIT_FAILURE);
    }

    mutex_unlock(&worker->lock);
    return result;
}
