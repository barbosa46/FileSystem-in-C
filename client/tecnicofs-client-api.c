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
    int return_val;

    snprintf(buf, 100, "c %s %d%d", filename, ownerPermissions, othersPermissions);
    if (write(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (read(sockfd, &return_val, sizeof(return_val)) < 0)
        return TECNICOFS_ERROR_OTHER;

    return return_val;
}

int tfsDelete(char *filename) {
    char buf[100];
    int return_val;

    snprintf(buf, 100, "d %s", filename);
    if (write(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (read(sockfd, &return_val, sizeof(return_val)) < 0)
        return TECNICOFS_ERROR_OTHER;

    return return_val;
}

int tfsRename(char *filenameOld, char *filenameNew) {
    char buf[100];
    int return_val;

    snprintf(buf, 100, "r %s %s", filenameOld, filenameNew);
    if (write(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (read(sockfd, &return_val, sizeof(return_val)) < 0)
        return TECNICOFS_ERROR_OTHER;

    return return_val;
}

int tfsOpen(char *filename, permission mode) {
    char buf[100];
    int return_val;

    snprintf(buf, 100, "o %s %d", filename, mode);
    if (write(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (read(sockfd, &return_val, sizeof(return_val)) < 0)
        return TECNICOFS_ERROR_OTHER;

    return return_val;
}

int tfsClose(int fd) {
    char buf[100];
    int return_val;

    snprintf(buf, 100, "x %d", fd);
    if (write(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (read(sockfd, &return_val, sizeof(return_val)) < 0)
        return TECNICOFS_ERROR_OTHER;

    return return_val;
}

int tfsRead(int fd, char *buffer, int len) {
    char buf[100];
    int return_val;

    snprintf(buf, 100, "l %d %d", fd, len);
    if (write(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (read(sockfd, &return_val, sizeof(return_val)) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (return_val > 0 && read(sockfd, buffer, len) < 0)
        return TECNICOFS_ERROR_OTHER;

    if (len > strlen(buffer)) len = strlen(buffer);
    buffer[len - 1] = '\0';

    return return_val;
}

int tfsWrite(int fd, char *buffer, int len) {
    char buf[100];
    int return_val;

    if (strlen(buffer) > len) buffer[len] = '\0';

    snprintf(buf, 100, "w %d %s", fd, buffer);
    if (write(sockfd, buf, 100) < 0)
        return TECNICOFS_ERROR_OTHER;
    if (read(sockfd, &return_val, sizeof(return_val)) < 0)
        return TECNICOFS_ERROR_OTHER;

    return return_val;
}
