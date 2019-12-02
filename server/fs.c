/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#include "fs.h"
#include "lib/bst.h"
#include "lib/inodes.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sync.h"
#include "tecnicofs-api-constants.h"


int obtainNewInumber(uid_t owner, permission ownerPerm, permission othersPerm) {
	int i = inode_create(owner, ownerPerm, othersPerm);
	return i;
}

tecnicofs* new_tecnicofs() {
	int i;

	tecnicofs*fs = malloc(sizeof(tecnicofs));
	if (!fs) {
		perror("failed to allocate tecnicofs");
		exit(EXIT_FAILURE);
	}

	fs->bsts = (bst *) calloc(numBuckets, sizeof(bst));
	if (!fs->bsts) {
		perror("failed to allocate tecnicofs");
		exit(EXIT_FAILURE);
	}

	fs->nextINumber = 0;
	for (i = 0; i < numBuckets; i++) {
		fs->bsts[i].bstRoot = NULL;
		sync_init(&(fs->bsts[i].bstLock));
	}

	return fs;
}

void free_tecnicofs(tecnicofs* fs) {
	int i;

	for (i = 0; i < numBuckets; i++) {
		free_tree(fs->bsts[i].bstRoot);
		sync_destroy(&(fs->bsts[i].bstLock));
	}

	free(fs->bsts);
	free(fs);
}

/*
 * Since the inumber is no longer "the number of files added before" but the index of
 * the first free slot on the inode table and the function that gives the its value
 * already allocates the inode, we are forced to obtain the inumber while we have the
 * lock of the bst.
 */
void create(tecnicofs* fs, char *name, uid_t uID, int sockfd,permission owner, permission others) {
	int key = hash(name, numBuckets), return_val = 0;
	int inumber;

	sync_wrlock(&(fs->bsts[key].bstLock));

	if (search(fs->bsts[key].bstRoot, name))
		return_val = TECNICOFS_ERROR_FILE_ALREADY_EXISTS;
	else {
		inumber = obtainNewInumber(uID,owner,others);
		fs->bsts[key].bstRoot = insert(fs->bsts[key].bstRoot, name, inumber);
	}

	if(write(sockfd, &return_val, sizeof(return_val)) < 1)
		perror("Error writing to socket");

	sync_unlock(&(fs->bsts[key].bstLock));
}

void deleteFile(tecnicofs* fs, char *name, uid_t uID, int sockfd) {
	int key = hash(name, numBuckets), iNumber = 0, return_val = 0;
	uid_t owner;

	sync_wrlock(&(fs->bsts[key].bstLock));
	node* searchNode = search(fs->bsts[key].bstRoot, name);
	if (searchNode) {
		iNumber = searchNode->inumber;

		inode_get(iNumber, &owner, NULL, NULL, NULL, 0);

		/* cycles through active clients' filetables */
		if(owner == uID) {
			for (int i = 0; i < num_connects; i++) {
				for (int j = 0; j < 5; j++) {
					if (opened[i][j] && iNumber == opened[i][j]->iNumber)
						return_val = TECNICOFS_ERROR_FILE_IS_OPEN;
				}
			}

			if (return_val != TECNICOFS_ERROR_FILE_IS_OPEN) {
				fs->bsts[key].bstRoot = remove_item(fs->bsts[key].bstRoot, name); /* remove from bst */
				inode_delete(iNumber);		/* remove from inode table */
			}
		} else
			return_val = TECNICOFS_ERROR_PERMISSION_DENIED;
	} else
		return_val = TECNICOFS_ERROR_FILE_NOT_FOUND;

	if(write(sockfd, &return_val, sizeof(return_val)) < 1)
		perror("Error writing to socket");

	sync_unlock(&(fs->bsts[key].bstLock));
}

void renameFile(tecnicofs* fs, char *name, char* new_name, uid_t uID, int sockfd) {
	int key1 = hash(name, numBuckets);
	int key2 = hash(new_name, numBuckets);
	int iNumber, return_val = 0;
	uid_t owner;

	/* force to always lock the tree with lower key first */
	if (key1 > key2) { int temp = key1; key1 = key2; key2 = temp; }

	sync_wrlock(&(fs->bsts[key1].bstLock));
	if(key1 != key2) sync_wrlock(&(fs->bsts[key2].bstLock));

	node* searchNode = search(fs->bsts[key1].bstRoot, name);
	if (!searchNode)
		return_val = TECNICOFS_ERROR_FILE_NOT_FOUND;
	else if (search(fs->bsts[key2].bstRoot, new_name))
		return_val = TECNICOFS_ERROR_FILE_ALREADY_EXISTS;
	else {
		iNumber = searchNode->inumber;
		inode_get(iNumber, &owner, NULL, NULL, NULL, 0);

		/* cycles through active clients' filetables */
		if(owner == uID) {
			for (int i = 0; i < num_connects; i++) {
				for (int j = 0; j < 5; j++) {
					if (opened[i][j] && iNumber == opened[i][j]->iNumber)
						return_val = TECNICOFS_ERROR_FILE_IS_OPEN;
				}
			}

			if (return_val != TECNICOFS_ERROR_FILE_IS_OPEN) {
				fs->bsts[key1].bstRoot = remove_item(fs->bsts[key1].bstRoot, name); // delete
				fs->bsts[key2].bstRoot = insert(fs->bsts[key2].bstRoot, new_name, iNumber); // create
			}
		} else
			return_val = TECNICOFS_ERROR_PERMISSION_DENIED;
	}

	if(write(sockfd, &return_val, sizeof(return_val)) < 1)
		perror("Error writing to socket");

	sync_unlock(&(fs->bsts[key1].bstLock));
	if(key1 != key2) sync_unlock(&(fs->bsts[key2].bstLock));
}

void openFile(tecnicofs* fs, char *name, int mode, uid_t uID, openedFile** filetable, int sockfd) {
	int key = hash(name, numBuckets), iNumber = 0, return_val = 0;
	permission ownerPerm, othersPerm, perm;
	uid_t owner;

	sync_rdlock(&(fs->bsts[key].bstLock));

	node* searchNode = search(fs->bsts[key].bstRoot, name);
	if (searchNode) {
		iNumber = searchNode->inumber;

		inode_get(iNumber, &owner, &ownerPerm, &othersPerm, NULL, 0);

		/* sets permissions acording to client */
		(owner == uID) ? (perm = ownerPerm) : (perm = othersPerm);

		if (perm == mode || perm == RW) {
			int i = 0;
			while (i < 5) {
				/* opens file in the first empty position of the client filetable */
				if (!filetable[i]) {
					openedFile *file = (openedFile *) malloc(sizeof(openedFile));

					file->key = key;
					file->iNumber = iNumber;
					file->mode = mode;

					filetable[i] = file;
					return_val = i;

					break;
				} i++;
			} if (i == 5) return_val = TECNICOFS_ERROR_MAXED_OPEN_FILES;
		} else return_val = TECNICOFS_ERROR_PERMISSION_DENIED;
	} else return_val = TECNICOFS_ERROR_FILE_NOT_FOUND;

	if(write(sockfd, &return_val, sizeof(return_val)) < 1)
		perror("Error writing to socket");

	sync_unlock(&(fs->bsts[key].bstLock));
}

void closeFile(tecnicofs* fs, int fd, openedFile** filetable, int sockfd) {
	int return_val = 0;

	if (filetable[fd]) {
		free(filetable[fd]);
		filetable[fd] = NULL;
	} else
		return_val = TECNICOFS_ERROR_FILE_NOT_OPEN;

	if(write(sockfd, &return_val, sizeof(return_val)) < 1)
		perror("Error writing to socket");
}

void readFile(tecnicofs* fs, int fd, uid_t uID,openedFile** filetable, int len, int sockfd) {
	int return_val = 0, iNumber = 0, key = 0;
	char buffer[len];
	uid_t owner;
	permission ownerPerm, othersPerm, perm;

	if (!filetable[fd])
		return_val = TECNICOFS_ERROR_FILE_NOT_OPEN;

	/* reads from file if opened in read/read-write mode */
	else if (filetable[fd]->mode == READ || filetable[fd]->mode == RW) {
		iNumber = filetable[fd]->iNumber;
		key = filetable[fd]->key;

		sync_rdlock(&fs->bsts[key].bstLock);

		inode_get(iNumber, &owner, &ownerPerm, &othersPerm, buffer, len);
		(owner == uID) ? (perm = ownerPerm) : (perm = othersPerm);

		if (perm == READ || perm == RW) return_val = strlen(buffer) - 1;
		else return_val = TECNICOFS_ERROR_PERMISSION_DENIED;

		sync_unlock(&fs->bsts[key].bstLock);

	} else
		return_val = TECNICOFS_ERROR_INVALID_MODE;

	if (write(sockfd, &return_val, sizeof(return_val)) < 1)
		perror("Error writing to socket");
	if (return_val > 0) {
		if(write(sockfd, buffer, len) < 1)
			perror("Error writing to socket");
	}
}

void writeFile(tecnicofs* fs, int fd, uid_t uID, openedFile** filetable, char* buffer, int sockfd){
	int return_val = 0, iNumber = 0,key = 0;
	uid_t owner;
	permission ownerPerm, othersPerm, perm;

	if (!filetable[fd])
		return_val = TECNICOFS_ERROR_FILE_NOT_OPEN;

	/* reads from file if opened in write/read-write mode */
	else if (filetable[fd]->mode == WRITE || filetable[fd]->mode == RW) {
		iNumber = filetable[fd]->iNumber;
		key = filetable[fd]->key;

		sync_rdlock(&fs->bsts[key].bstLock);

		inode_get(iNumber, &owner, &ownerPerm, &othersPerm, NULL, 0);
		(owner == uID) ? (perm = ownerPerm) : (perm = othersPerm);

		if(perm == WRITE || perm == RW){
			inode_set(iNumber, buffer, strlen(buffer));
			return_val = 0;
		} else
			return_val = TECNICOFS_ERROR_PERMISSION_DENIED;

		sync_unlock(&fs->bsts[key].bstLock);
	} else
		return_val = TECNICOFS_ERROR_INVALID_MODE;

	if(write(sockfd, &return_val, sizeof(return_val)) < 1)
		perror("Error writing to socket");
}

int lookup(tecnicofs* fs, char *name) {
	int key = hash(name, numBuckets);

	sync_rdlock(&(fs->bsts[key].bstLock));

	int inumber = 0;
	node* searchNode = search(fs->bsts[key].bstRoot, name);
	if (searchNode)
		inumber = searchNode->inumber;

	sync_unlock(&(fs->bsts[key].bstLock));

	return inumber;
}


void print_tecnicofs_tree(FILE * fp, tecnicofs *fs) {
	int i;

	for (i = 0; i < numBuckets; i++) {
		if (fs->bsts[i].bstRoot)
			print_tree(fp, fs->bsts[i].bstRoot);
	}
}
