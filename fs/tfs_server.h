#ifndef TFS_SERVER_H
#define TFS_SERVER_H

#include "common/common.h"
#include "config.h"
#include <pthread.h>
#include <stdbool.h>

/* Represents a packet */
typedef struct {
    char opcode;
    char client_pipe[PIPE_STRING_LENGTH];
    char file_name[PIPE_STRING_LENGTH];
    int flags;
    int fhandle;
    size_t len;
    char *buffer;
} packet_t;

/* Represents a worker */
typedef struct {
    int session_id;
    packet_t packet;
    int pipe_out;
    bool to_execute;
    pthread_t tid;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} worker_t;

/*
 * Initializes the server.
 * Returns 0 if successful, -1 otherwise.
 */
int init_server();

/*
 * Returns a worker.
 * Returns session_id if there is a worker available, -1 otherwise.
 */
int get_available_worker();

/*
 * Changes the state of the worker to free.
 * Returns 0 if successful, -1 otherwise.
 */
int free_worker(int session_id);

int parse_tfs_open_packet();
int parse_tfs_close_packet();
int parse_tfs_write_packet();
int parse_tfs_read_packet();

int wrap_packet_parser_fn(int parser_fn(worker_t *), char op_code);

void *session_worker(void *args);

int handle_tfs_mount();
void handle_tfs_unmount(worker_t *worker);
void handle_tfs_open_worker(worker_t *worker);
void handle_tfs_open_worker(worker_t *worker);
void handle_tfs_write(worker_t *worker);
void handle_tfs_read(worker_t *worker);
void handle_tfs_close(worker_t *worker);
void handle_tfs_shutdown_after_all_closed(worker_t *worker);

void close_server_by_user(int s);

#endif
