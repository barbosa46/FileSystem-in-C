/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include "constants.h"

#ifdef RWLOCK
    #define syncMech              pthread_rwlock_t
    #define syncMech_init(a,b)    pthread_rwlock_init(a,b)
    #define syncMech_destroy(a)   pthread_rwlock_destroy(a)
    #define syncMech_wrlock(a)    pthread_rwlock_wrlock(a)
    #define syncMech_rdlock(a)    pthread_rwlock_rdlock(a)
    #define syncMech_unlock(a)    pthread_rwlock_unlock(a)
#elif MUTEX
    #define syncMech              pthread_mutex_t
    #define syncMech_init(a,b)    pthread_mutex_init(a,b)
    #define syncMech_destroy(a)   pthread_mutex_destroy(a)
    #define syncMech_wrlock(a)    pthread_mutex_lock(a)
    #define syncMech_rdlock(a)    pthread_mutex_lock(a)
    #define syncMech_unlock(a)    pthread_mutex_unlock(a)
#else //Abstract Sequential
    #define syncMech              void*
    #define syncMech_init(a,b)    do_nothing(a)
    #define syncMech_destroy(a)   do_nothing(a)
    #define syncMech_wrlock(a)    do_nothing(a)
    #define syncMech_rdlock(a)    do_nothing(a)
    #define syncMech_unlock(a)    do_nothing(a)
#endif

void sync_init(syncMech* sync);
void sync_destroy(syncMech* sync);
void sync_wrlock(syncMech* sync);
void sync_rdlock(syncMech* sync);
void sync_unlock(syncMech* sync);
void mutex_init(pthread_mutex_t* mutex);
void mutex_lock(pthread_mutex_t* mutex);
void mutex_unlock(pthread_mutex_t* mutex);
void mutex_destroy(pthread_mutex_t* mutex);
void init_sem(sem_t* sem, int value);
void destroy_sem(sem_t* sem);
void wait_sem(sem_t* sem);
void trywait_sem(sem_t* sem);
void post_sem(sem_t* sem);
int do_nothing(void* a);

#endif /* SYNC_H */
