#ifndef FS_H
#define FS_H
#include <pthread.h>
#include "lib/bst.h"
#include "lib/hash.h"

typedef struct tecnicofs {
    node** bstRoots;
    int nextINumber;
} tecnicofs;

extern int numBuckets;

void initTreeLocks();
void destroyTreeLocks();
int obtainNewInumber(tecnicofs* fs);
tecnicofs* new_tecnicofs();
void free_tecnicofs(tecnicofs* fs);
void create(tecnicofs* fs, char *name, int inumber);
void delete(tecnicofs* fs, char *name);
int lookup(tecnicofs* fs, char *name);
void print_tecnicofs(FILE * fp, tecnicofs *fs);

#endif /* FS_H */
