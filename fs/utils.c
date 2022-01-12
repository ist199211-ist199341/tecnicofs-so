#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void rwl_wrlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_wrlock(rwl) != 0) {
        perror("Failed to lock RWlock");
        exit(EXIT_FAILURE);
    }
}

void rwl_rdlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_rdlock(rwl) != 0) {
        perror("Failed to lock RWlock");
        exit(EXIT_FAILURE);
    }
}

void rwl_unlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_unlock(rwl) != 0) {
        perror("Failed to unlock RWlock");
        exit(EXIT_FAILURE);
    }
}

void mutex_lock(pthread_mutex_t *mutex) {
    if (pthread_mutex_lock(mutex) != 0) {
        perror("Failed to lock Mutex");
        exit(EXIT_FAILURE);
    }
}

void mutex_unlock(pthread_mutex_t *mutex) {
    if (pthread_mutex_unlock(mutex) != 0) {
        perror("Failed to unlock Mutex");
        exit(EXIT_FAILURE);
    }
}
