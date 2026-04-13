#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#define MAX_CLIENTS 100
#define PORT 9000

struct Client {
    int fd;
    int logged_in;
};

int check_login(const char *user, const char *pass) {
    FILE *f = fopen("users.txt", "r");
    if (!f) return 0;
    char f_user[256], f_pass[256];
    while (fscanf(f, "%255s %255s", f_user, f_pass) == 2) {
        if (strcmp(f_user, user) == 0 && strcmp(f_pass, pass) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

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

    char *login_prompt = "Enter user and pass: ";
    char *login_ok = "Login successful. Enter commands:\n";
    char *login_fail = "Login failed. Try again:\n";

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
                            clients[j].logged_in = 0;
                            break;
                        }
                    }
                    send(new_fd, login_prompt, strlen(login_prompt), 0);
                } else {
                    char buffer[1024];
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

                        if (!clients[client_idx].logged_in) {
                            char user[256], pass[256];
                            if (sscanf(buffer, "%255s %255s", user, pass) == 2) {
                                if (check_login(user, pass)) {
                                    clients[client_idx].logged_in = 1;
                                    send(i, login_ok, strlen(login_ok), 0);
                                } else {
                                    send(i, login_fail, strlen(login_fail), 0);
                                }
                            } else {
                                send(i, login_prompt, strlen(login_prompt), 0);
                            }
                        } else {
                            char cmd[1050];
                            snprintf(cmd, sizeof(cmd), "%s > out.txt 2>&1", buffer);
                            system(cmd);

                            FILE *f = fopen("out.txt", "r");
                            if (f) {
                                char file_buf[2048];
                                int n;
                                while ((n = fread(file_buf, 1, sizeof(file_buf), f)) > 0) {
                                    send(i, file_buf, n, 0);
                                }
                                fclose(f);
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
