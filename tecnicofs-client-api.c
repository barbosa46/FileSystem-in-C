#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <string.h>
#include "tecnicofs-client-api.h"

int sockfd = -1;

int tfsMount(char * address) {
    int servlen;
    struct sockaddr_un serv_addr;

    if (sockfd != -1)
        return TECNICOFS_ERROR_OPEN_SESSION;

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return TECNICOFS_ERROR_OTHER;

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, address);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

    if(connect(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        return TECNICOFS_ERROR_CONNECTION_ERROR;

    return 0;
}

int tfsUnmount() {
    if (sockfd == -1)
        return TECNICOFS_ERROR_NO_OPEN_SESSION;

    close(sockfd);
    return 0;
}

int tfsCreate(char *filename, permission ownerPermissions, permission othersPermissions) {
    char buf[100];

    snprintf(buf, 100, "c %s %d%d", filename, ownerPermissions, othersPermissions);
    if (write(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (read(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;

    if (strcmp(buf, "FILE ALREADY EXISTS") == 0)
        return TECNICOFS_ERROR_FILE_ALREADY_EXISTS;

    return 0;
}

int tfsDelete(char *filename) {
    char buf[100];

    snprintf(buf, 100, "d %s", filename);
    if (write(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (read(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;

    if (strcmp(buf, "PERMISSION DENIED") == 0)
        return TECNICOFS_ERROR_PERMISSION_DENIED;
    if (strcmp(buf, "FILE NOT FOUND") == 0)
        return TECNICOFS_ERROR_FILE_NOT_FOUND;

    return 0;
}

int tfsRename(char *filenameOld, char *filenameNew) {
    char buf[100];

    snprintf(buf, 100, "r %s %s", filenameOld, filenameNew);
    if (write(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (read(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;

    if (strcmp(buf, "PERMISSION DENIED") == 0)
        return TECNICOFS_ERROR_PERMISSION_DENIED;
    if (strcmp(buf, "OLD FILE NOT FOUND") == 0)
        return TECNICOFS_ERROR_FILE_NOT_FOUND;
    if (strcmp(buf, "NEW FILE ALREADY EXISTS") == 0)
        return TECNICOFS_ERROR_FILE_ALREADY_EXISTS;

    return 0;
}
