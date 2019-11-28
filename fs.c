/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#include "fs.h"
#include "lib/bst.h"
#include "lib/inodes.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sync.h"


int obtainNewInumber(uid_t owner, permission ownerPerm, permission othersPerm) {
	int i=inode_create(owner, ownerPerm, othersPerm);
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

void create(tecnicofs* fs, char *name, int inumber,int sockfd) {
	int key = hash(name, numBuckets);

	sync_wrlock(&(fs->bsts[key].bstLock));

	// If file exists, delete the inode that was created before
	if (search(fs->bsts[key].bstRoot, name)) { 
		inode_delete(inumber);
		write(sockfd,"FILE ALREADY EXISTS",100);
	}
	//If not, add it to the bst
	else{ 
		fs->bsts[key].bstRoot = insert(fs->bsts[key].bstRoot, name, inumber);
		write(sockfd,"FILE CREATED",100);
	}
	sync_unlock(&(fs->bsts[key].bstLock));
}

void delete(tecnicofs* fs, char *name, uid_t uID, int sockfd) {
	int key = hash(name, numBuckets),iNumber=0;
	uid_t owner;
	printf("%d",lookup(fs,name));
	sync_wrlock(&(fs->bsts[key].bstLock));
	node* searchNode = search(fs->bsts[key].bstRoot, name);
	if (searchNode) {
		iNumber = searchNode->inumber;

		inode_get(iNumber,&owner,NULL,NULL,NULL,0);

		if(owner==uID){
			//remove from bst
			fs->bsts[key].bstRoot = remove_item(fs->bsts[key].bstRoot, name);
			//remove from table
			inode_delete(iNumber);
			write(sockfd,"FILE DELETED",100);
		}
		else{
			write(sockfd,"PERMISSION DENIED",100);
		}
	}
	else{
		write(sockfd,"FILE NOT FOUND",100);
	}

	sync_unlock(&(fs->bsts[key].bstLock));
}

int lookup(tecnicofs* fs, char *name) {
	int key = hash(name, numBuckets);

	sync_rdlock(&(fs->bsts[key].bstLock));
	
	int inumber = 0;
	node* searchNode = search(fs->bsts[key].bstRoot, name);
	if (searchNode) {
		inumber = searchNode->inumber;
	}
	
	sync_unlock(&(fs->bsts[key].bstLock));

	return inumber;
}

void renameFile(tecnicofs* fs, char *name1, char* name2, int sockfd) {
	int key1 = hash(name1, numBuckets);
	int key2 = hash(name2, numBuckets);
	int iNumber;
	// force to always lock the tree with lower key first
	if (key1 > key2) { int temp = key1; key1 = key2; key2 = temp; }
	
	// lock the first
	sync_wrlock(&(fs->bsts[key1].bstLock));
	// lock the second if is different
	if(key1 != key2) sync_wrlock(&(fs->bsts[key2].bstLock));

	node* searchNode = search(fs->bsts[key1].bstRoot, name1);
	if(!searchNode)
		write(sockfd,"OLD FILE NOT FOUND",100);
	else if(search(fs->bsts[key2].bstRoot,name2))
		write(sockfd,"NEW FILE ALREADY EXISTS",100);
	else{
		iNumber=searchNode->inumber;
		fs->bsts[key1].bstRoot = remove_item(fs->bsts[key1].bstRoot, name1); // delete
		fs->bsts[key2].bstRoot = insert(fs->bsts[key2].bstRoot, name2, iNumber); // create
		write(sockfd,"FILE RENAMED",100);
	}
	sync_unlock(&(fs->bsts[key1].bstLock));
	if(key1 != key2) sync_unlock(&(fs->bsts[key2].bstLock));
}

void print_tecnicofs_tree(FILE * fp, tecnicofs *fs) {
	int i;

	for (i = 0; i < numBuckets; i++) {
		if (fs->bsts[i].bstRoot)
			print_tree(fp, fs->bsts[i].bstRoot);
	}
}
