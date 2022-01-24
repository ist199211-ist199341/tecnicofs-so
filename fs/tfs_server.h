#ifndef TFS_SERVER_H
#define TFS_SERVER_H

#include "config.h"
#include <pthread.h>

/* Represents a worker */
typedef struct {
    int session_id;
    buffer_t buffer;
    int pipe_out;
    char to_execute;
    pthread_t tid;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} worker_t;

typedef struct {
    int8_t data[WORKER_BUFFER_LEN];
    size_t offset;
} buffer_t;

int init_server();

int get_available_worker();
int free_worker(int session_id);

void handle_tfs_mount();
void handle_tfs_unmount(int session_id);
void handle_tfs_open(int session_id);
void handle_tfs_close(int session_id);
void handle_tfs_write(int session_id);
void handle_tfs_read(int session_id);
void handle_tfs_shutdown_after_all_closed(int session_id);

void close_server_by_user(int s);
void read_id_and_launch_function(void fn(int));

void *session_worker(void *args);

void handle_tfs_open_worker(int8_t *buffer, int pipe_out);

int buffer_write(buffer_t *buffer, void *data, size_t size);
void buffer_read(buffer_t *buffer, void *data, size_t size);

#endif
