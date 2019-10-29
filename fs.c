/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#include "fs.h"
#include "lib/bst.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sync.h"


int obtainNewInumber(tecnicofs* fs) {
	int newInumber = ++(fs->nextINumber);
	return newInumber;
}

tecnicofs* new_tecnicofs(){
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

void free_tecnicofs(tecnicofs* fs){
	int i;

	for (i = 0; i < numBuckets; i++) {
		free_tree(fs->bsts[i].bstRoot);
		sync_destroy(&(fs->bsts[i].bstLock));
	}

	free(fs->bsts);
	free(fs);
}

void create(tecnicofs* fs, char *name, int inumber){
	int key = hash(name, numBuckets);

	sync_wrlock(&(fs->bsts[key].bstLock));
	fs->bsts[key].bstRoot = insert(fs->bsts[key].bstRoot, name, inumber);
	sync_unlock(&(fs->bsts[key].bstLock));
}

void delete(tecnicofs* fs, char *name){
	int key = hash(name, numBuckets);

	sync_wrlock(&(fs->bsts[key].bstLock));
	fs->bsts[key].bstRoot = remove_item(fs->bsts[key].bstRoot, name);
	sync_unlock(&(fs->bsts[key].bstLock));
}

int lookup(tecnicofs* fs, char *name){
	int key = hash(name, numBuckets);

	sync_rdlock(&(fs->bsts[key].bstLock));
	int inumber = 0;
	node* searchNode = search(fs->bsts[key].bstRoot, name);
	if ( searchNode ) {
		inumber = searchNode->inumber;
	}
	sync_unlock(&(fs->bsts[key].bstLock));
	return inumber;
}

void print_tecnicofs_tree(FILE * fp, tecnicofs *fs){
	int i;

	for (i = 0; i < numBuckets; i++) { print_tree(fp, fs->bsts[i].bstRoot); }
}
