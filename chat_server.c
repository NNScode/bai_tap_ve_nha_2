#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>

#define MAX_CLIENTS 100
#define PORT 9000

struct Client {
    int fd;
    int registered;
    char id[64];
    char name[64];
};

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(listener, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(listener, 10);

    struct Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;

    fd_set all_fds, read_fds;
    FD_ZERO(&all_fds);
    FD_SET(listener, &all_fds);
    int max_fd = listener;

    char *prompt = "client_id: client_name\n";

    while (1) {
        read_fds = all_fds;
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) break;

        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == listener) {
                    int new_fd = accept(listener, NULL, NULL);
                    FD_SET(new_fd, &all_fds);
                    if (new_fd > max_fd) max_fd = new_fd;

                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd == -1) {
                            clients[j].fd = new_fd;
                            clients[j].registered = 0;
                            break;
                        }
                    }
                    send(new_fd, prompt, strlen(prompt), 0);
                } else {
                    char buffer[2048];
                    memset(buffer, 0, sizeof(buffer));
                    int bytes = recv(i, buffer, sizeof(buffer) - 1, 0);

                    int client_idx = -1;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd == i) {
                            client_idx = j;
                            break;
                        }
                    }

                    if (bytes <= 0) {
                        close(i);
                        FD_CLR(i, &all_fds);
                        if (client_idx != -1) clients[client_idx].fd = -1;
                    } else {
                        buffer[strcspn(buffer, "\r\n")] = 0;
                        if (strlen(buffer) == 0) continue;

                        if (!clients[client_idx].registered) {
                            char id[64], name[64];
                            if (sscanf(buffer, "%63[^:]: %63s", id, name) == 2) {
                                strcpy(clients[client_idx].id, id);
                                strcpy(clients[client_idx].name, name);
                                clients[client_idx].registered = 1;
                            } else {
                                send(i, prompt, strlen(prompt), 0);
                            }
                        } else {
                            time_t now = time(NULL);
                            struct tm *t = localtime(&now);
                            char time_str[64];
                            strftime(time_str, sizeof(time_str), "%Y/%m/%d %I:%M:%S%p", t);

                            char send_buf[3000];
                            snprintf(send_buf, sizeof(send_buf), "%s %s: %s\n", time_str, clients[client_idx].id, buffer);

                            for (int j = 0; j < MAX_CLIENTS; j++) {
                                if (clients[j].fd != -1 && clients[j].registered && clients[j].fd != i) {
                                    send(clients[j].fd, send_buf, strlen(send_buf), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    close(listener);
    return 0;
}
