#include "config.h"
#include "http.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    struct sockaddr_in addr;
    if (server < 0) return 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PI_MONITOR_PORT);
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(server, 32) < 0) { perror("listen"); return 1; }
    for (;;) {
        int fd = accept(server, NULL, NULL);
        if (fd >= 0) handle_connection(fd);
        else if (errno != EINTR) continue;
    }
}
