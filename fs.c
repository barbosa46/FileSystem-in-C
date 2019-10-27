#include "fs.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef MUTEX
    pthread_mutex_t treeMutexs[numBuckets];
#elif RWLOCK
	pthread_rwlock_t treeRWLocks[numBuckets];
#endif

void initTreeLocks(){
    #ifdef MUTEX
		int i;

		for (i = 0; i < numBuckets; i++) {
    		if(pthread_mutex_init(&treeMutexs[i], NULL) != 0){
            	fprintf(stderr, "Error: Could not init mutex.\n");
            	exit(EXIT_FAILURE);
        	}
		}
	#elif RWLOCK
		int i;

		for (i = 0; i < numBuckets; i++) {
			if(pthread_rwlock_init(&treeRWLocks[i], NULL) != 0){
				fprintf(stderr, "Error: Could not init rwlock.\n");
				exit(EXIT_FAILURE);
			}
		}
	#endif
}

void destroyTreeLocks(){
	#ifdef MUTEX
		int i;

		for (i = 0; i < numBuckets; i++) {
    		if(pthread_mutex_destroy(&treeMutexs[i], NULL) != 0){
            	fprintf(stderr, "Error: Could not destroy mutex.\n");
            	exit(EXIT_FAILURE);
        	}
		}
	#elif RWLOCK
		int i;

		for (i = 0; i < numBuckets; i++) {
			if(pthread_rwlock_destroy(&treeRWLocks[i], NULL) != 0){
				fprintf(stderr, "Error: Could not destroy rwlock.\n");
				exit(EXIT_FAILURE);
			}
		}
	#endif
}

int obtainNewInumber(tecnicofs* fs) {
	int newInumber = ++(fs->nextINumber);
	return newInumber;
}

tecnicofs* new_tecnicofs(){
	tecnicofs*fs = malloc(sizeof(tecnicofs));
	if (!fs) {
		perror("failed to allocate tecnicofs");
		exit(EXIT_FAILURE);
	}

	fs->bstRoots = (node **) malloc(sizeof(node*) * numBuckets);
	if (!fs->bstRoots) {
		perror("failed to allocate tecnicofs");
		exit(EXIT_FAILURE);
	}
	fs->nextINumber = 0;

	initTreeLocks();

	return fs;
}

void free_tecnicofs(tecnicofs* fs){
	int i;

	for (i = 0; i < numBuckets; i++) { free_tree(fs->bstRoots[i]); }
	free(fs->bstRoots);
	free(fs);

	destroyTreeLocks();
}

void create(tecnicofs* fs, char *name, int inumber){
	int key = hash(name, numBuckets);

	#ifdef MUTEX
	    pthread_mutex_t *mut = &treeMutexs[key];
		if(pthread_mutex_lock(mut) != 0) {
            fprintf(stderr, "Error: Could not lock mutex\n");
            exit(EXIT_FAILURE);
        }
	#elif RWLOCK
		pthread_rwlock_t *rwlock = &treeRWLocks[key];
		if(pthread_rwlock_wrlock(rwlock) != 0) {
            fprintf(stderr, "Error: Could not lock rwlock\n");
            exit(EXIT_FAILURE);
        }
	#endif

	fs->bstRoots[key] = insert(fs->bstRoots[key], name, inumber);

	#ifdef MUTEX
		if(pthread_mutex_unlock(mut) != 0) {
            fprintf(stderr, "Error: Could not unlock mutex\n");
            exit(EXIT_FAILURE);
        }
	#elif RWLOCK
		if(pthread_rwlock_unlock(rwlock) != 0) {
            fprintf(stderr, "Error: Could not unlock rwlock\n");
            exit(EXIT_FAILURE);
        }
	#endif
}

void delete(tecnicofs* fs, char *name){
	int key = hash(name, numBuckets);

	#ifdef MUTEX
	    pthread_mutex_t *mut = &treeMutexs[key];
		if(pthread_mutex_lock(mut) != 0) {
            fprintf(stderr, "Error: Could not lock mutex\n");
            exit(EXIT_FAILURE);
        }
	#elif RWLOCK
		pthread_rwlock_t *rwlock = &treeRWLocks[key];
		if(pthread_rwlock_wrlock(rwlock) != 0) {
            fprintf(stderr, "Error: Could not lock rwlock\n");
            exit(EXIT_FAILURE);
        }
	#endif

	fs->bstRoots[key] = remove_item(fs->bstRoots[key], name);

	#ifdef MUTEX
		if(pthread_mutex_unlock(mut) != 0) {
            fprintf(stderr, "Error: Could not unlock mutex\n");
            exit(EXIT_FAILURE);
        }
	#elif RWLOCK
		if(pthread_rwlock_unlock(rwlock) != 0) {
            fprintf(stderr, "Error: Could not unlock rwlock\n");
            exit(EXIT_FAILURE);
        }
	#endif
}

int lookup(tecnicofs* fs, char *name){
	int key = hash(name, numBuckets);

	#ifdef MUTEX
	    pthread_mutex_t *mut = &treeMutexs[key];
		if(pthread_mutex_lock(mut) != 0) {
            fprintf(stderr, "Error: Could not lock mutex\n");
            exit(EXIT_FAILURE);
        }
	#elif RWLOCK
		pthread_rwlock_t *rwlock = &treeRWLocks[key];
		if(pthread_rwlock_rdlock(rwlock) != 0) {
            fprintf(stderr, "Error: Could not lock rwlock\n");
            exit(EXIT_FAILURE);
        }
	#endif

	node* searchNode = search(fs->bstRoots[key], name);
	if ( searchNode ) return searchNode->inumber;

	#ifdef MUTEX
		if(pthread_mutex_unlock(mut) != 0) {
            fprintf(stderr, "Error: Could not unlock mutex\n");
            exit(EXIT_FAILURE);
        }
	#elif RWLOCK
		if(pthread_rwlock_unlock(rwlock) != 0) {
            fprintf(stderr, "Error: Could not unlock rwlock\n");
            exit(EXIT_FAILURE);
        }
	#endif

	return 0;
}

void print_tecnicofs(FILE * fp, tecnicofs *fs){
	int i;

	for (i = 0; i < numBuckets; i++) { print_tree(fp, fs->bstRoots[i]); }
}
