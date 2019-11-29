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

void create(tecnicofs* fs, char *name, int inumber, int sockfd) {
	int key = hash(name, numBuckets), return_val = 0;

	sync_wrlock(&(fs->bsts[key].bstLock));

	/* if file exists, delete the inode that was created before; if not,
	   add it to the bst */
	if (search(fs->bsts[key].bstRoot, name)) {
		return_val = TECNICOFS_ERROR_FILE_ALREADY_EXISTS;

		inode_delete(inumber);
	} else
		fs->bsts[key].bstRoot = insert(fs->bsts[key].bstRoot, name, inumber);

	write(sockfd, &return_val, sizeof(return_val));

	sync_unlock(&(fs->bsts[key].bstLock));
}

void delete(tecnicofs* fs, char *name, uid_t uID, int sockfd) {
	int key = hash(name, numBuckets), iNumber = 0, return_val = 0;
	uid_t owner;

	sync_wrlock(&(fs->bsts[key].bstLock));
	node* searchNode = search(fs->bsts[key].bstRoot, name);
	if (searchNode) {
		iNumber = searchNode->inumber;

		inode_get(iNumber, &owner, NULL, NULL, NULL, 0);

		if(owner == uID) {
			fs->bsts[key].bstRoot = remove_item(fs->bsts[key].bstRoot, name); /* remove from bst */
			inode_delete(iNumber);		/* remove from inode table */
		} else
			return_val = TECNICOFS_ERROR_PERMISSION_DENIED;
	} else
		return_val = TECNICOFS_ERROR_FILE_NOT_FOUND;

	write(sockfd, &return_val, sizeof(return_val));

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

		if(owner == uID) {
			fs->bsts[key1].bstRoot = remove_item(fs->bsts[key1].bstRoot, name); // delete
			fs->bsts[key2].bstRoot = insert(fs->bsts[key2].bstRoot, new_name, iNumber); // create
		} else
			return_val = TECNICOFS_ERROR_PERMISSION_DENIED;
	}

	write(sockfd, &return_val, sizeof(return_val));

	sync_unlock(&(fs->bsts[key1].bstLock));
	if(key1 != key2) sync_unlock(&(fs->bsts[key2].bstLock));
}

void openFile(tecnicofs* fs, char *name, int mode, uid_t uID, openedFile** filetable, int sockfd) {
	int key = hash(name, numBuckets), iNumber = 0, return_val = 0;
	permission ownerPerm, othersPerm, perm;
	uid_t owner;

	node* searchNode = search(fs->bsts[key].bstRoot, name);
	if (searchNode) {
		iNumber = searchNode->inumber;

		inode_get(iNumber, &owner, &ownerPerm, &othersPerm, NULL, 0);

		(owner == uID) ? (perm = ownerPerm) : (perm = othersPerm);
		if(perm == mode || perm == RW) {
			int i = 0;

			while (i < 5) {
				if (filetable[i]) {
					filetable[i]->iNumber = iNumber;
					filetable[i]->mode = mode;

					return_val = i;
				}

				i++;
			}

			return_val = TECNICOFS_ERROR_MAXED_OPEN_FILES;

		} else
			return_val = TECNICOFS_ERROR_PERMISSION_DENIED;
	} else
		return_val = TECNICOFS_ERROR_FILE_NOT_FOUND;

	write(sockfd, &return_val, sizeof(return_val));
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
