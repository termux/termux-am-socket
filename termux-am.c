#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#ifndef PREFIX
#define PREFIX "/data/data/com.termux/files/usr"
#endif


#define SOCKET_PATH PREFIX"/../am-socket"

#define QUOTE "\""
#define SPACE " "



void send_blocking(const int fd, const void* data, int len) {
    while (len > 0) {
        int ret = send(fd, data, len, MSG_NOSIGNAL);
        if (ret == -1) {
            perror("Socket write error");
            exit(1);
        }
        len -= ret;
        data += ret;
    }
}


bool recv_part(const int fd, char* data, int len) {
    while (len > 0) {
        int ret = recv(fd, data, 1, 0);
        if (ret == -1) {
            perror("Socket write error");
            exit(1);
        }
        if (*data == '\0' || ret == 0) {
            return true;
        }
        data++;
        len--;
    }
    return false;
}


int main(int argc, char* argv[]) {
    struct sockaddr_un adr = {.sun_family = AF_UNIX};
    if (strlen(SOCKET_PATH) >= sizeof(adr.sun_path)) {
        fputs( "Socket path too long.", stderr);
        return 1;
    }
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("Could not create socket");
        return 1;
    }
    strncpy(adr.sun_path, SOCKET_PATH, sizeof(adr.sun_path)-1);
    if (connect(fd, (struct sockaddr*) &adr, sizeof(adr)) == -1) {
        perror("Could not connect to socket");
        close(fd);
        return 1;
    }
    
    for (int i = 1; i<argc; i++) {
        send_blocking(fd, QUOTE, sizeof(QUOTE));
        send_blocking(fd, argv[i], strlen(argv[i]));
        send_blocking(fd, QUOTE, sizeof(QUOTE));
        send_blocking(fd, SPACE, sizeof(SPACE));
    }
    shutdown(fd, SHUT_WR);
    
    int exit_code;
    
    {
        char tmp[10];
        memset(tmp, '\0', sizeof(tmp));
        if (! recv_part(fd, tmp, sizeof(tmp)-1)) {
            fputs("Exit code too long", stderr);
            exit(1);
        }
        errno = 0;
        exit_code = strtol(tmp, NULL, 10);
        if (errno != 0) {
            perror("Exit code not a valid number");
            exit(1);
        }
    }
    
    {
        char tmp[4096];
        bool ret;
        do {
            memset(tmp, '\0', sizeof(tmp));
            ret = recv_part(fd, tmp, sizeof(tmp)-1);
            fputs(tmp, stdout);
        } while (! ret);
    }
    
    {
        char tmp[4096];
        bool ret;
        do {
            memset(tmp, '\0', sizeof(tmp));
            ret = recv_part(fd, tmp, sizeof(tmp)-1);
            fputs(tmp, stderr);
        } while (! ret);
    }
    
    return exit_code;
}
