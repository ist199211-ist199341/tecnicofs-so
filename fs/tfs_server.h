#ifndef TFS_SERVER_H
#define TFS_SERVER_H

#include "common/common.h"
#include "config.h"
#include <pthread.h>
#include <stdint.h>
typedef struct {
    int8_t data[WORKER_BUFFER_LEN];
    size_t offset;
} buffer_t;

/* Represents a worker */
typedef struct {
    int session_id;
    packet_t packet;
    int pipe_out;
    int to_execute;
    pthread_t tid;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} worker_t;

int init_server();

int get_available_worker();
int free_worker(int session_id);

void handle_tfs_mount(worker_t *worker);
void handle_tfs_unmount(worker_t *worker);
void handle_tfs_open_worker(worker_t *worker);
void handle_tfs_open_worker(worker_t *worker);
void handle_tfs_write(worker_t *worker);
void handle_tfs_read(worker_t *worker);
void handle_tfs_close(worker_t *worker);
void handle_tfs_shutdown_after_all_closed(worker_t *worker);

void close_server_by_user(int s);
void read_id_and_launch_function(void fn(int));

void *session_worker(void *args);

int buffer_write(buffer_t *buffer, void *data, size_t size);
void buffer_read(buffer_t *buffer, void *data, size_t size);

#endif
