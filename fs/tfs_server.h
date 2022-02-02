#ifndef TFS_SERVER_H
#define TFS_SERVER_H

#include "common/common.h"
#include "config.h"
#include <pthread.h>
#include <stdbool.h>

/* Represents a packet */
typedef struct {
    char opcode;
    char client_pipe[PIPE_STRING_LENGTH + 1];
    char file_name[PIPE_STRING_LENGTH + 1];
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
 * Returns 0 if successful, -1 if worker is already free.
 */
int free_worker(int session_id);

/*
 * Reads the content of the pipe for the tfs_open function.
 * Returns 0 if successful, -1 otherwise.
 */
int parse_tfs_open_packet();

/*
 * Reads the content of the pipe for the tfs_close function.
 * Returns 0 if successful, -1 otherwise.
 */
int parse_tfs_close_packet();

/*
 * Reads the content of the pipe for the tfs_write function.
 * Returns 0 if successful, -1 otherwise.
 */
int parse_tfs_write_packet();

/*
 * Reads the content of the pipe for the tfs_read function.
 * Returns 0 if successful, -1 otherwise.
 */
int parse_tfs_read_packet();

/* Given the opcode, it executes the associated parser function.
 * Input:
 * - parser_fn: function to be executed
 * - op_code: op_code of the function to be used
 * Returns the result of the parser function.
 */
int wrap_packet_parser_fn(int parser_fn(worker_t *), char op_code);

/* The worker thread main function, it waits for a signal, handles the request
 * and waits for the next request.
 * Input:
 * - args: worker
 */
void *session_worker(void *args);

/* Mounts the client to the server
 * Returns 0.
 */
int handle_tfs_mount();

/* Unmounts the client of the server
 * Input:
 * - worker:  worker that is going to handle the function
 */
void handle_tfs_unmount(worker_t *worker);

/* Executes tfs_open
 * Input:
 * - worker:  worker that is going to handle the function
 */
void handle_tfs_open_worker(worker_t *worker);

/* Executes tfs_write
 * Input:
 * - worker: worker that is going to handle the function
 */
void handle_tfs_write(worker_t *worker);

/* Executes tfs_read
 * Input:
 * - worker: worker that is going to handle the function
 */
void handle_tfs_read(worker_t *worker);

/* Executes tfs_close
 * Input:
 * - worker: worker that is going to handle the function
 */
void handle_tfs_close(worker_t *worker);

/* Executes tfs_shutdown
 * Input:
 * - worker: worker that is going to handle the function
 */
void handle_tfs_shutdown_after_all_closed(worker_t *worker);

/*
 * Handles the SIGINT signal.
 * Closes the server.
 */
void close_server_by_user(int s);

/*
 * Handles the SIGPIPE signal.
 * Closes the server.
 */
void close_server_by_pipe_broken(int s);

#endif
