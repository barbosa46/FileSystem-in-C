/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#ifndef FS_H
#define FS_H

#include "lib/bst.h"
#include "lib/hash.h"
#include "tecnicofs-api-constants.h"
#include "sync.h"
#include "lib/inodes.h"

typedef struct bst {
    node* bstRoot;
    syncMech bstLock;
} bst;

typedef struct tecnicofs {
    bst *bsts;
    int nextINumber;
} tecnicofs;

extern int numBuckets;

int obtainNewInumber(uid_t owner, permission ownerPerm, permission othersPerm);
tecnicofs* new_tecnicofs();
void free_tecnicofs(tecnicofs* fs);
void create(tecnicofs* fs, char *name, int inumber,int sockfd);
void delete(tecnicofs* fs, char *name, uid_t uID, int sockfd);
void renameFile(tecnicofs* fs, char *name1, char* name2, int iNumber);
int lookup(tecnicofs* fs, char *name);
void print_tecnicofs_tree(FILE * fp, tecnicofs *fs);

#endif /* FS_H */
