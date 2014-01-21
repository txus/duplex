#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <netdb.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wait.h>

#define BUF_SIZE 1024

#define READ  0
#define WRITE 1

#define SERVER_SOCKET_ERROR -1
#define SERVER_SETSOCKOPT_ERROR -2
#define SERVER_BIND_ERROR -3
#define SERVER_LISTEN_ERROR -4
#define CLIENT_SOCKET_ERROR -5
#define CLIENT_RESOLVE_ERROR -6
#define CLIENT_CONNECT_ERROR -7
#define CREATE_PIPE_ERROR -8
#define BROKEN_PIPE_ERROR -9

typedef struct options_s {
  int local_port;

  char *remote_host;
  int remote_port;

  char *replay_host;
  int replay_port;
} Options;

Options parse_options(int argc, char *argv[]);
int create_connection(char *host, int port);
int create_socket(int port);
void forward_and_replay_data(int source_sock, int destination_sock, int replay_sock);
void forward_data(int source_sock, int destination_sock);
void handle_client(int client_sock, struct sockaddr_in client_addr, Options options);
void server_loop();
void sigchld_handler(int signal);
void sigterm_handler(int signal);

int server_sock, client_sock, remote_sock, replay_sock;

int Duplex_start(int argc, char *argv[]) {
    pid_t pid;

    int server_sock;

    Options options = parse_options(argc, argv);
    if (!options.remote_host) {
      return -1;
    }

    if ((server_sock = create_socket(options.local_port)) < 0) { // start server
        perror("Cannot run server");
        return server_sock;
    }

    signal(SIGCHLD, sigchld_handler); // prevent ended children from becoming zombies
    signal(SIGTERM, sigterm_handler); // handle KILL signal

    switch(pid = fork()) {
        case 0:
            server_loop(server_sock, options);
            break;
        case -1:
            perror("Cannot daemonize");
            return pid;
        default:
            printf("%i\n", pid);
            close(server_sock);
    }

    return 0;
}

Options parse_options(int argc, char *argv[]) {
    Options options;
    int c;

    while ((c = getopt(argc, argv, "l:h:H:p:P:")) != -1) {
        switch(c) {
            case 'l':
                options.local_port = atoi(optarg);
                break;
            case 'h':
                options.remote_host = optarg;
                break;
            case 'H':
                options.replay_host = optarg;
                break;
            case 'p':
                options.remote_port = atoi(optarg);
                break;
            case 'P':
                options.replay_port = atoi(optarg);
                break;
        }
    }

    if (!options.local_port ||
        !options.remote_host ||
        !options.remote_port ||
        !options.replay_host ||
        !options.replay_port) {
        printf("Syntax: %s -l local_port -h remote_host -p remote_port -H replay_host -P replay_port\n", argv[0]);
    }
    return options;
}

int create_socket(int port) {
    int server_sock, optval;
    struct sockaddr_in server_addr;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return SERVER_SOCKET_ERROR;
    }

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        return SERVER_SETSOCKOPT_ERROR;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        return SERVER_BIND_ERROR;
    }

    if (listen(server_sock, 20) < 0) {
        return SERVER_LISTEN_ERROR;
    }

    return server_sock;
}

/* Handle finished child process */
void sigchld_handler(int signal) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* Handle term signal */
void sigterm_handler(int signal) {
    close(client_sock);
    close(server_sock);
    exit(0);
}

void server_loop(int server_sock, Options options) {
    struct sockaddr_in client_addr;
    int addrlen = sizeof(client_addr);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addrlen);
        if (fork() == 0) { // handle client connection in a separate process
            close(server_sock);
            handle_client(client_sock, client_addr, options);
            exit(0);
        }
        close(client_sock);
    }

}

void handle_client(int client_sock, struct sockaddr_in client_addr, Options options)
{
    if ((remote_sock = create_connection(options.remote_host, options.remote_port)) < 0) {
        perror("Cannot connect to remote host");
        return;
    }

    if ((replay_sock = create_connection(options.replay_host, options.replay_port)) < 0) {
        perror("Cannot connect to replay host");
        return;
    }

    if (fork() == 0) { // a process forwarding data from client to remote socket
        forward_and_replay_data(client_sock, remote_sock, replay_sock);
        exit(0);
    }

    if (fork() == 0) { // a process forwarding data from remote socket to client
        forward_data(remote_sock, client_sock);
        exit(0);
    }

    close(remote_sock);
    close(replay_sock);
    close(client_sock);
}

/* Forward data between sockets */
void forward_data(int source_sock, int destination_sock) {
    char buffer[BUF_SIZE];
    int n;

    while ((n = recv(source_sock, buffer, BUF_SIZE, 0)) > 0) { // read data from input socket
        send(destination_sock, buffer, n, 0); // send data to output socket
    }

    shutdown(destination_sock, SHUT_RDWR); // stop other processes from using socket
    close(destination_sock);

    shutdown(source_sock, SHUT_RDWR); // stop other processes from using socket
    close(source_sock);
}

/* Forward data between sockets and replay it somwhere else*/
void forward_and_replay_data(int source_sock, int destination_sock, int replay_sock) {
    char buffer[BUF_SIZE];
    int n;

    while ((n = recv(source_sock, buffer, BUF_SIZE, 0)) > 0) { // read data from input socket
        send(destination_sock, buffer, n, 0); // send data to output socket
        send(replay_sock, buffer, n, 0); // send data to replay socket
    }

    shutdown(destination_sock, SHUT_RDWR); // stop other processes from using socket
    close(destination_sock);

    shutdown(replay_sock, SHUT_RDWR); // stop other processes from using socket
    close(replay_sock);

    shutdown(source_sock, SHUT_RDWR); // stop other processes from using socket
    close(source_sock);
}

/* Create client connection */
int create_connection(char *host, int port) {
    struct sockaddr_in server_addr;
    struct hostent *server;
    int sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return CLIENT_SOCKET_ERROR;
    }

    if ((server = gethostbyname(host)) == NULL) {
        errno = EFAULT;
        return CLIENT_RESOLVE_ERROR;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        return CLIENT_CONNECT_ERROR;
    }

    return sock;
}
