/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#ifndef FS_H
#define FS_H

#include "lib/bst.h"
#include "lib/hash.h"
#include "tecnicofs-api-constants.h"
#include "sync.h"
#include "lib/inodes.h"

#define MAX_CLIENTS 100

typedef struct bst {
    node* bstRoot;
    syncMech bstLock;
} bst;

typedef struct tecnicofs {
    bst *bsts;
    int nextINumber;
} tecnicofs;

typedef struct openedFile {
    int key;
    int iNumber;
    permission mode;
} openedFile;       /* filetable entry; contains file and client info */

struct threadArg {
    int newSockfd;
    int index;
    uid_t uID;
};

openedFile *opened[MAX_CLIENTS][5];

extern int numBuckets, num_connects;

int obtainNewInumber(uid_t owner, permission ownerPerm, permission othersPerm);
tecnicofs* new_tecnicofs();
void free_tecnicofs(tecnicofs* fs);
void create(tecnicofs* fs, char *name, uid_t uID, int sockfd,permission owner, permission others);
void deleteFile(tecnicofs* fs, char *name, uid_t uID, int sockfd);
void renameFile(tecnicofs* fs, char *name, char* new_name, uid_t uID, int sockfd);
void openFile(tecnicofs* fs, char *name, int mode, uid_t uID, openedFile** filetable, int sockfd);
void closeFile(tecnicofs* fs, int fd, openedFile** filetable, int sockfd);
void readFile(tecnicofs* fs, int fd, uid_t uID, openedFile** filetable, int len, int sockfd);
void writeFile(tecnicofs* fs, int fd, uid_t uID, openedFile** filetable, char* buffer, int sockfd);
int lookup(tecnicofs* fs, char *name);
void print_tecnicofs_tree(FILE * fp, tecnicofs *fs);

#endif /* FS_H */
