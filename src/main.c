#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <poll.h>

#define DIE(message)     \
do {                     \
    perror(message);     \
    exit(EXIT_FAILURE);  \
} while (0);             \

#define DEFAULT_PORT 8000
static int port  = DEFAULT_PORT;
static int alive = 1;

#define MAX_FD 128


void load_opt(int, char**);
void sigint_handler(int);
void cast_ipaddress(char**, struct sockaddr_in);
void close_conn(struct pollfd*, int*);
int  clean_fdset(struct pollfd**);
void echo(int, ssize_t);


int
main(int argc, char** argv)
{
    signal(SIGINT, sigint_handler);
    load_opt(argc, argv);

    int       server_sock;
    socklen_t server_siz;
    socklen_t client_siz;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    struct pollfd      rfds[MAX_FD];

    char buf[BUFSIZ];
    int  nfds = 1;

    bzero(&buf, BUFSIZ);
    bzero(&server_addr, sizeof(server_addr));
    bzero(&client_addr, sizeof(client_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port);

    server_siz = sizeof(server_addr);
    
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == server_sock)
        DIE("Cannot allocate fd.\n");

    if (0 > bind(server_sock, (struct sockaddr*)&server_addr, server_siz))
        DIE("Bind error.\n");

    listen(server_sock, 0);
    printf("Listening on port %d...\n", port);

    rfds[nfds - 1].fd     = server_sock;
    rfds[nfds - 1].events = POLLIN;

    while (alive)
    {
        int clientfd;
        int indexfd;
        int retval;

        retval = poll(rfds, nfds, 1000);
        if (-1 == retval)
            continue;

        for (indexfd = 0; indexfd < nfds; ++indexfd)
        {
            struct pollfd item = rfds[indexfd];
            int    currentfd = item.fd;
            int    nread     = 0;
            if (!(item.revents & POLLIN))
                continue;

            if (server_sock == currentfd)
            {
                char* inaddr;
                clientfd = accept(currentfd, (struct sockaddr*)&client_addr, &client_siz);
                ++nfds;

                rfds[nfds - 1].fd     = clientfd;
                rfds[nfds - 1].events = POLLIN;

                cast_ipaddress(&inaddr, client_addr);
                printf("Received connection from %s.\n", inaddr);
                continue;
            }

            // ioctl(currentfd, FIONREAD, &nread);
            // if (0 == nread)
            // {
            //     close_conn(&rfds[indexfd], &nfds);
            //     // nfds = clean_fdset((struct pollfd**)&rfds);
            //     continue;
            // }

            echo(currentfd, nread);
            close_conn(&rfds[indexfd], &nfds);
            // nfds = clean_fdset((struct pollfd**)&rfds);
        }
    }

    return EXIT_SUCCESS;
}


void
sigint_handler(int signo)
{
    alive = 0;
    // printf("Shutdown gracefully...\n");
    DIE("Shutdown gracefully...\n");
}

void load_opt(int argc, char** argv)
{
    int ret;

    while (1)
    {
        int option_index = 0;

        static struct option opts[] = {
            {"port", required_argument, 0, 'p'}
        };

        ret = getopt_long(argc, argv, ":p", opts, &option_index);
        if (-1 == ret)
            break;

        switch (ret)
        {
        case 'p':
            if (!optarg)
                break;

            port = atoi(optarg);
            if (!port)
                port = DEFAULT_PORT;

            printf("Port: %d\n", port);
            break;

        default:
            break;
        }
    }
}

void
cast_ipaddress(char** addr, struct sockaddr_in client_addr)
{
    *addr = (char*)malloc(INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(client_addr.sin_addr), *addr, INET_ADDRSTRLEN);
}


void
close_conn(struct pollfd* item, int* nfds)
{
    shutdown(item->fd, SHUT_RDWR);
    printf("Fd %d disconnected.\n", item->fd);

    item->fd     = -1;
    item->events = 0;

    --(*nfds);
}


void echo(int fd, ssize_t total)
{
    char buf[BUFSIZ];
    bzero(&buf, BUFSIZ);

    ssize_t nread = 0;

    do {
        nread = read(fd, buf, BUFSIZ);
        write(fd, buf, nread);
    } while (nread >= BUFSIZ);
}
