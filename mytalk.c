#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pwd.h>
#include <poll.h>
#include <ctype.h>

#define LISTEN_BACKLOG 50
#define MAX_PORT 65535
#define MIN_PORT 1024
#define MAX 80
#define SIZE 1024
#define MAXLEN 1000
#define LOCAL 0
#define REMOTE (LOCAL + 1)


#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)

int v_flag, a_flag, n_flag;


void chat(int sock_fd) {
        int len_t;
        char buff_s[SIZE];
        struct pollfd fds[REMOTE + 1];
        int num_fd;

        fds[LOCAL].fd = STDIN_FILENO;
        fds[LOCAL].events = POLLIN;
        fds[LOCAL].revents = 0;
        fds[REMOTE] = fds[LOCAL];
        fds[REMOTE].fd = sock_fd;

        num_fd = 2;

        if (v_flag) {
                printf("v_flag: %d\n", v_flag);
                printf("a_flag: %d\n", a_flag);
                printf("N_flag: %d\n", n_flag);
        }

        if (!n_flag) {
                start_windowing();
        }

        while (num_fd != 0) {

                if (poll(fds, num_fd, -1) < 0) {
                        handle_error("poll error");
                        stop_windowing();

                }
                if (fds[1].revents & POLLIN) {
                        if ((len_t = recv(sock_fd, buff_s,
                                SIZE, 0)) != 0) {
                                write_to_output(buff_s, len_t);
                        }
                        else {
                                if (num_fd != 0) {
                                    buff_s[len_t] = '\0';
                                    write_to_output(buff_s, len_t);
                                    close(sock_fd);
                                    write_to_output(exit, strlen(exit));
                                }
                        }
                }
                if (fds[0].revents & POLLIN) {
                        update_input_buffer();
                        if (has_whole_line()) {
                                len_t = read_from_input(buff_s, SIZE);
                                len_t = send(sock_fd, buff_s, len_t, 0);
                                if (has_hit_eof()) {
                                        close(sock_fd);
                                        num_fd = 0;
                                }
                        }
                }
        }
        stop_windowing();
}


int server(int sfd, int cfd, int port) {
                struct sockaddr_in server_addr;
                struct sockaddr_in client_addr;
                char buff_s[SIZE];
                char recv_buff[SIZE];
                socklen_t len;
                int recv_len;

                memset(&recv_buff, 0, SIZE);

                if (v_flag) {
                        printf("server mode\n");
                }

                if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                        handle_error("socket error");
                }

                if (v_flag) {
                        printf("socket created ...\n");
                }

                memset(&server_addr, 0, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(port);
                server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

                if (bind(sfd, (struct sockaddr *) &server_addr,
                        sizeof(server_addr)) == -1)
                        handle_error("bind");

                if (v_flag) {
                        printf("bind successful ...\n");
                }

                if (listen(sfd, LISTEN_BACKLOG) == -1)
                        handle_error("listen error");

                if (v_flag) {
                        printf("listening ...\n");
                }

                len  = sizeof(client_addr);
                cfd = accept(sfd, (struct sockaddr *) &client_addr,
                        &len);

                if (v_flag) {
                        printf("accepted ...\n");
                }

                len  = sizeof(client_addr);
                getsockname(cfd, (struct sockaddr *)
                        &client_addr, &len);

                if (cfd == -1)
                        handle_error("accept error ...");

                recv_len = recv(cfd, recv_buff, SIZE, 0);
                recv_buff[recv_len] = '\0';

                printf("Mytalk request from %s", recv_buff);
                /* if a_flag not set, ask to accept */
                if (a_flag == 0) {
                        printf(".  Accept (y/n)?\n");
                        fflush(stdout);
                        scanf("%s", buff_s);

                        int i;
                        for (i = 0; i < strlen(buff_s); i++) {
                                buff_s[i] = tolower(buff_s[i]);
                        }


                        if (strcmp(buff_s, "yes") == 0 ||
                                strcmp(buff_s, "y") == 0) {
                                send(cfd, "ok", 2, 0);
                                chat(cfd);
                        }
                        else {
                                send(cfd, "no", 2, 0);
                        }
                }
                /* if a_flag set, just accept */
                else {
                        printf("\n");
                        send(cfd, "ok", 2, 0);
                        chat(cfd);
                }

        return 0;
}


int client(int sfd, int cfd, int port,
        char *port_string, char *hostname) {
                struct addrinfo hints, *servinfo;
                int rv;
                struct passwd *pwd;
                char buf[SIZE];
                memset(&buf, 0, SIZE);

                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_protocol = 0;
                hints.ai_flags = 0;


                if (v_flag) {
                        printf("client mode\n");
                }

                if ((rv = getaddrinfo(hostname, port_string,
                        &hints, &servinfo)) != 0) {
                        fprintf(stderr, "getaddrinfo: %s\n",
                                gai_strerror(rv));
                        return 1;
                }

                if (v_flag) {
                        printf("getaddrinfo successful ...\n");
                }

                if ((sfd = socket(servinfo->ai_family,
                        servinfo->ai_socktype,
                                servinfo->ai_protocol)) == -1) {
                        handle_error("client: socket");
                }

                if (v_flag) {
                        printf("socket created ...\n");
                }

                if (connect(sfd, servinfo->ai_addr,
                        servinfo->ai_addrlen) == -1) {
                        close(sfd);
                        handle_error("client: connect");
                }

                if (v_flag) {
                        printf("connected ...\n");
                }
                /* get user and hostname */
                if ((pwd = getpwuid(getuid())) == NULL) {
                        handle_error("pwd error");
                }
                sprintf(buf, "%s@%s", pwd->pw_name, hostname);

                if (v_flag > 1) {
                        printf("user: %s\nhostname: %s\n",
                                pwd->pw_name, hostname);
                }

                fflush(stdout);

                send(sfd, buf, strlen(buf), 0);
                printf("Waiting for response from %s.\n", hostname);
                memset(&buf, 0, SIZE);
                if (recv(sfd, buf, SIZE, 0) == -1) {
                        handle_error("recv error");
                }
                if (strcmp(buf, "ok") == 0) {
                        chat(sfd);
                }
                else {
                        printf("%s declined connection\n", hostname);
                }

        return 0;
}

int main(int argc, char *argv[])
{

        v_flag = a_flag = n_flag = 0;
        int flag_count;
        int other_arg_count;
        int c;
        char *hostname;
        hostname = NULL;
        int port;
        char *port_string;
        char *ptr;
        int sfd, cfd;
        sfd = cfd = 0;


        if (argc < 2)
                handle_error("too few arguments");

        while ((c = getopt (argc, argv, "vaN")) != -1) {
                switch (c) {
                        case 'v':
                                v_flag++;
                                break;
                        case 'a':
                                a_flag++;
                                break;
                        case 'N':
                                n_flag++;
                                break;
                        default:
                                handle_error("unknown flag");
                                break;
                }
        }

        /* for verbosity */
        if (v_flag) {
                printf("v_flag: %d\n", v_flag);
                printf("a_flag: %d\n", a_flag);
                printf("N_flag: %d\n", n_flag);
        }


        flag_count = v_flag + a_flag + n_flag;
        other_arg_count = argc - flag_count;

        port = strtol(argv[argc-1], &ptr, 10);
        port_string = argv[argc-1];
        if (port <= MIN_PORT || port > MAX_PORT)
                handle_error("invalid port error");
        /* server mode */
        if (other_arg_count == 2) {
                server(sfd, cfd, port);
        }
        /* client mode */
        else if (other_arg_count == 3) {
                hostname = argv[argc - 2];
                client(sfd, cfd, port, port_string, hostname);

        }
        else {
                handle_error("wrong number of arguments");

        }

        close(sfd);
        close(cfd);
                return 0;
}
