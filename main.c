#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <time.h>
#include "fs.h"

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100

#ifdef MUTEX
    pthread_mutex_t commandMut;
    pthread_mutex_t treeMut;
#elif RWLOCK
    pthread_mutex_t commandMut;
    pthread_rwlock_t treeRw;
#endif

int numberThreads = 0;
tecnicofs* fs;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;
int numBuckets= 0;
char fileLocation[MAX_INPUT_SIZE];

/*_________________________Multithreading_Aux_Functions_________________________*/

void initLocks(){
    #ifdef MUTEX
    	if(pthread_mutex_init(&treeMut,NULL)!=0 || pthread_mutex_init(&commandMut,NULL)!=0){
            fprintf(stderr, "Error: Could not init locks.\n");
            exit(EXIT_FAILURE);
        }
        
	#elif RWLOCK
    	if(pthread_mutex_init(&commandMut,NULL)!=0 || pthread_rwlock_init(&treeRw,NULL)!= 0){
            fprintf(stderr, "Error: Could not init locks.\n");
            exit(EXIT_FAILURE);
        }
	#endif
}
void lockCommand() {
    #ifdef MUTEX
        if(pthread_mutex_lock(&commandMut) != 0) {
            fprintf(stderr, "Error: Could not lock mutex\n");
            exit(EXIT_FAILURE);
        }
    #elif RWLOCK
        if(pthread_mutex_lock(&commandMut) != 0) {
            fprintf(stderr, "Error: Could not lock mutex\n");
            exit(EXIT_FAILURE);
        }
    #endif
}
void unlockCommand() {
    #ifdef MUTEX
        if(pthread_mutex_unlock(&commandMut) != 0) {
            fprintf(stderr, "Error: Could not unlock mutex\n");
            exit(EXIT_FAILURE);
        }
    #elif RWLOCK
        if(pthread_mutex_unlock(&commandMut) != 0) {
            fprintf(stderr, "Error: Could not unlock mutex\n");
            exit(EXIT_FAILURE);
        }
    #endif
}
void writeLockTree() {
    #ifdef MUTEX
        if(pthread_mutex_lock(&treeMut) != 0) {
            fprintf(stderr, "Error: Could not lock mutex\n");
            exit(EXIT_FAILURE);
        }
    #elif RWLOCK
        if(pthread_rwlock_wrlock(&treeRw) != 0) {
            fprintf(stderr, "Error: Could not lock rwlock\n");
            exit(EXIT_FAILURE);
        }
    #endif
}
void readLockTree() {
    #ifdef MUTEX
        if(pthread_mutex_lock(&treeMut) != 0) {
            fprintf(stderr, "Error: Could not lock mutex\n");
            exit(EXIT_FAILURE);
        }
    #elif RWLOCK
        if(pthread_rwlock_rdlock(&treeRw) != 0) {
            fprintf(stderr, "Error: Could not lock rwlock\n");
            exit(EXIT_FAILURE);
        }
    #endif
}
void unlockTree() {
    #ifdef MUTEX
        if(pthread_mutex_unlock(&treeMut) != 0) {
            fprintf(stderr, "Error: Could not unlock mutex\n");
            exit(EXIT_FAILURE);
        }
    #elif RWLOCK
        if(pthread_rwlock_unlock(&treeRw) != 0) {
            fprintf(stderr, "Error: Could not unlock rwlock\n");
            exit(EXIT_FAILURE);
        }
    #endif
}
void destroyLocks(){
    #ifdef MUTEX
    	pthread_mutex_destroy(&treeMut);
        pthread_mutex_destroy(&commandMut);
	#elif RWLOCK
        pthread_mutex_destroy(&commandMut);
        pthread_rwlock_destroy(&treeRw);
	#endif
}

/*_________________________End_Of_Aux_Functions_________________________*/

static void displayUsage (const char* appName){
    printf("Usage: %s\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc != 5) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
    strcpy(fileLocation,argv[1]);

    numberThreads= atoi(argv[3]);
    if(numberThreads<=0){
        fprintf(stderr,"Invalid number of threads.\n");
        displayUsage(argv[0]);   
    }
    
    numBuckets= atoi(argv[4]);
    if(numBuckets<=0){
        fprintf(stderr,"Invalid number of buckets.\n");
        displayUsage(argv[0]);   
    }
}

int insertCommand(char* data) {
    if(numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[numberCommands++], data);
        return 1;
    }
    return 0;
}

char* removeCommand() {
    if((numberCommands > 0)){
        numberCommands--;
        return inputCommands[headQueue++];
    }
    return NULL;
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    //exit(EXIT_FAILURE);
}

void processInput(){
    FILE *file = fopen(fileLocation,"r");
    char line[MAX_INPUT_SIZE];

    while (fgets(line, sizeof(line)/sizeof(char), file)) {
        char token;
        char name[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c %s", &token, name);

        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
            case 'l':
            case 'd':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            case '#':
                break;
            default: { /* error */
                errorParse();
            }
        }
    }
    fclose(file);
}

void * applyCommands(){
    while(1){
        lockCommand();
        if(numberCommands>0){

            const char* command = removeCommand();

            if (command == NULL){
                unlockCommand();
                continue;
            }
            char token;
            char name[MAX_INPUT_SIZE];
            int numTokens = sscanf(command, "%c %s", &token, name);

            if (numTokens != 2) {
                fprintf(stderr, "Error: invalid command in Queue\n");
                exit(EXIT_FAILURE);
            }

            int searchResult;
            int iNumber;
            switch (token) {
                case 'c':
                    iNumber = obtainNewInumber(fs);

                    unlockCommand();
                    writeLockTree();

                    create(fs, name, iNumber);

                    unlockTree();

                    break;
                case 'l':
                    unlockCommand();
                    readLockTree();

                    searchResult = lookup(fs, name);

                    unlockTree();

                    if(!searchResult)
                        printf("%s not found\n", name);
                    else
                        printf("%s found with inumber %d\n", name, searchResult);

                    break;
                case 'd':
                    unlockCommand();
                    writeLockTree();
                    
                    delete(fs, name);
                    
                    unlockTree();
                    
                    break;
                default: { /* error */
                    unlockCommand();

                    fprintf(stderr, "Error: command to apply\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
        else{
            unlockCommand();
            return NULL;
        }
    }
}

void runThreads(int numberThreads){
    struct timeval start, finish;
    double elapsed;
    int i = 0;
    pthread_t threads[numberThreads];
    
    if(gettimeofday(&start)==NULL){
        fprintf(stderr, "Error: gettimeofday failed.\n");
        exit(EXIT_FAILURE);
    }
    
    for(i = 0;i < numberThreads; ++i){
        if (pthread_create(&threads[i], 0, applyCommands, NULL) != 0){
            fprintf(stderr, "Error: Could not create thread\n");
            exit(EXIT_FAILURE);
        }
    }
    for (i = 0; i < numberThreads; ++i)
        pthread_join(threads[i], NULL);

    if(gettimeofday(&finish)==NULL){
        fprintf(stderr, "Error: gettimeofday failed.\n");
        exit(EXIT_FAILURE);
    }
    elapsed= (double)(finish.tv_sec) +(double)(finish.tv_usec / 1000000.0) - \ 
            (double)(start.tv_sec) +(double)(start.tv_usec / 1000000.0);
    printf("TecnicoFS completed in %.4f seconds.\n",elapsed);
}

int main(int argc, char* argv[]) {
    initLocks();
    parseArgs(argc, argv);
    if(FILE *out= fopen(argv[2],"w")==NULL){
        fprintf(stderr,"ERROR: Couldn't open file.\n");
        exit(EXIT_FAILURE);
    }
    fs = new_tecnicofs();
    processInput();

    runThreads(numberThreads);

    print_tecnicofs_tree(out, fs);
    fclose(out);
    free_tecnicofs(fs);
    destroyLocks();

    exit(EXIT_SUCCESS);
}
