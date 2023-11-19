#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/time.h>
#include <pthread.h>

#define MYPORT "4950"
#define BACKLOG 5




int create_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p);
int create_send_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p, char *host);
void *get_in_addr(struct sockaddr *sa);
int pings_complete(int *pings_received, int host_count);
int not_in_closed_channels(int *closed_channels, int fd);



int main(int argc, char *argv[])
{
    struct state
    {
        int id;
        int predecessor;
        int successor;
        int state_counter;
        int record;
    };

    struct addrinfo hints, *servinfo, *p;
    char hostname[10];
    hostname[9] = '\0';
    float delay_time;
    float marker_delay;
    int listen_fd;
    int token = 0;
    int host_count;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    int accept_fd;
    char *msg = "Token";
    char *marker = "marker";
    int msg_len;
    char s[INET6_ADDRSTRLEN];
    int recv_fd;
    char buf[256];
    struct state ss;
    int state_req;
    int received_marker = 0;
    int marker_start;
    int marker_len = strlen(marker);
    int marker_channel;
    ss.id = -1;
    ss.predecessor = -1;
    ss.successor = -1;
    ss.state_counter = 0;
    int snapshot_starter = 0;
    struct timeval tv;
    fd_set read_fds; // read_fds file descriptor list
    int fdmax;       // max file descriptor number

    FD_ZERO(&read_fds);
    char *file;
    int channel_i = 0;

    for (int i = 0; i < argc; i++)
    {
        fprintf(stderr, "argv[%d] = '%s' \n", i, argv[i]);
        if (strcmp(argv[i], "-t") == 0)
        {
            delay_time = atof(argv[i + 1]);
        }
        if (strcmp(argv[i], "-x") == 0)
        {
            token = 1;
            fprintf(stderr, "token = %d \n", token);
        }
        if (strcmp(argv[i], "-h") == 0)
        {
            file = argv[i + 1];
        }
        if (strcmp(argv[i], "-m") == 0)
        {
            marker_delay = atof(argv[i + 1]);
            snapshot_starter = 1;
        }
        if (strcmp(argv[i], "-s") == 0)
        {
            state_req = atoi(argv[i + 1]);
            marker_start = 1;
        }
    }

    // get peer names
    FILE *fptr;
    char hosts[10][20];
    int host_i = 0;
    fprintf(stderr, "Opening host file\n");
    // Open file
    fptr = fopen(file, "r");
    if (fptr == NULL)
    {
        printf("Cannot open file \n");

        exit(0);
    }

    gethostname(hostname, 10);
    // Read contents from file
    while (fscanf(fptr, "%19s", hosts[host_i]) == 1)
    {
        if (strcmp(hosts[host_i], hostname) == 0)
        {
            ss.id = host_i;
        }
        host_i++;
    }
    host_count = host_i;

    // setting up the ring application
    ss.predecessor = ss.id - 1;
    ss.successor = ss.id + 1;
    if (ss.id == 0)
    {
        ss.predecessor = host_count - 1;
    }
    if (ss.id == host_count - 1)
    {
        ss.successor = 1;
    }
    
    listen_fd = create_socket(hints, servinfo, p);
    if (listen(listen_fd, BACKLOG) == -1)
    {
        perror("server: listen");
    }
    sleep(2);
    int *peer_fd = malloc(host_count * sizeof(int));
    for (int i = 0; i < host_count; i++)
    {
        if (i != ss.id)
        {
            peer_fd[i] = create_send_socket(hints, servinfo, p, hosts[i]);
        }
    }

    fprintf(stderr, "all sockets connected\n");

    msg_len = strlen(msg);
    if (token == 1)
    {
        sleep(delay_time);
        int result = send(peer_fd[0], msg, msg_len, 0);
        if (result == -1)
        {
            perror("send");
        }
        fprintf(stderr, "message sent. result: %d\n", result);
    }

    ss.state_counter = snapshot_starter;
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);
    fdmax = listen_fd; // keep track of the biggest file descriptor
    int nbytes;
        struct timeval timeout;
        timeout.tv_sec = 2; // 1-second timeout
        timeout.tv_usec = 0;
      FD_SET(listen_fd, &read_fds); // add listener to the master set\      

    while (1)
    {
        // fprintf(stderr, "{id: %d, state: %d, predecessor: %d, successor: %d}\n", ss.id, ss.state_counter, ss.predecessor, ss.successor);


        // if every channel is closed then snapshot ends
        // if (pings_complete(pings_received, host_count))
        // {
        //     received_marker = 0;
        //     fprintf(stderr, "{id: %d, snapshot:''complete''}", ss.id);
        //
        //     // then empty all queues
        // }
        // if (marker_start == 1)
        // {
        //     fprintf(stderr, "starts with marker\n");
        //     if (ss.state_counter == state_req)
        //     {
        //         fprintf(stderr, "{id: %d, snapshot:''started''}", ss.id);
        //         received_marker = 1;
        //     }
        // }

        // fprintf(stderr, "select start\n");

        if (select(fdmax + 1, &read_fds, NULL, NULL, &timeout) == -1)
        {
            perror("select");
            exit(4);
        }

        // run thru existing connections looking for data to read
        for (int i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                if (i == listen_fd)
                {
                    // handle new connections
                    // accept from channels
                    socklen_t addr_size = sizeof their_addr;
                    int accept_fd = accept(listen_fd, (struct sockaddr *)&their_addr, &addr_size);
                    fcntl(accept_fd, F_SETFL, O_NONBLOCK);
                    fprintf(stderr, "accepted, fd: %d\n", accept_fd);

                    if (accept_fd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {

                        FD_SET(accept_fd, &read_fds); // add to read_fds set
                        if (accept_fd > fdmax)
                        {
                            fdmax = accept_fd;
                        }
                        char s[INET6_ADDRSTRLEN];
                        inet_ntop(their_addr.ss_family,
                                  get_in_addr((struct sockaddr *)&their_addr),
                                  s, sizeof s);
                        fprintf(stderr, "server: got connection from %s\n", s);
                    }
                }
                else
                {
                    // handle data from channels
                    if ((nbytes = recv(i, buf, 255, 0)) <= 0)
                    {
                        if (nbytes == 0)
                        {
                            fprintf(stderr, "socket hung up\n");
                        }
                        else
                        {
                            perror("recv");
                        }
                        close(i);
                        FD_CLR(i, &read_fds); // remove from read_fds set
                        continue;
                    }
                    // fprintf(stderr, "receieved\n");
                    else
                    {
                        fprintf(stderr, "received: %s\n", buf);
                        if (strstr(buf, msg) != NULL)
                        {
                            token = 1;
                            fprintf(stderr, "token acquired\n");

                        } else {

                            fprintf(stderr, "token wasnt counted\n");
                        }
                        for (int j = 0; j <= fdmax; j++)
                        {

                            // if (FD_ISSET(j, &read_fds))
                            // {
                            // fprintf(stderr, "send isset\n");

                                if (j != listen_fd && j != i)
                                {
                            fprintf(stderr, "not listen or ourself\n");

                                    // if have token, send it to successor
                                    if (token == 1)
                                    {
                                        fprintf(stderr, "I have the token. Attempting to send:\n");
                                        msg_len = strlen(msg);

                                        if (send(j, msg, msg_len, 0) == -1)
                                        {
                                            perror("send error");
                                        }
                                        fprintf(stderr, "{id: %d, state: %d, sender: %d, receiver: %d}\n", ss.id, ss.state_counter, ss.id, ss.successor);
                                        token = 0;
                                    }

                                    // if (received_marker == 1 && not_in_closed_channels(closed_channels, j) == 1)
                                    // {
                                    //     fprintf(stderr, "I have the marker. Attempting to send:\n");

                                    //     if (send(j, marker, marker_len, 0) == -1)
                                    //     {
                                    //         perror("send");
                                    //     }
                                    //     fprintf(stderr, "{id: %d,  sender: %d, receiver: %d, msg: ''marker'', state: %d, has_token:%d}\n", ss.id, ss.predecessor, ss.successor, ss.state_counter, token);
                                    // }
                                }
                            // }
                        }
                    }
                }
            }
        }
    }
    // fprintf(stderr, "{id: %d, state: %d, sender: %d, receiver: %d}\n", ss.id, ss.state_counter, ss.predecessor, ss.id);

    return 0;
}

int create_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p)
{
    int sock_fd;
    fprintf(stderr, "Listening socket pthread created\n");
    char ipstr[100];
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    int yes = 1;

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        void *ina;

        /** step 1 **/
        if (p->ai_family == AF_INET)
        { // IPv4
            struct sockaddr_in *sa4 = (struct sockaddr_in *)p->ai_addr;
            ina = &(sa4->sin_addr);
        }
        else
        { // IPv6
            struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)p->ai_addr;
            ina = &(sa6->sin6_addr);
        }

        /** step 2 **/
        inet_ntop(p->ai_family, ina, ipstr, sizeof ipstr);

        /** step 3 **/
        if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        /** step 4 **/
        if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("server: bind");
            continue;
        }

        fprintf(stderr, "socket binded: %s\n", ipstr);
        int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

        break;
    }

    freeaddrinfo(servinfo);

    fprintf(stderr, "Listening on interface: %s\n", ipstr);

    return sock_fd;
}

int create_send_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p, char *host)
{
    int sock_fd;
    fprintf(stderr, "Sending socket  created\n");
    char ipstr[100];
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((status = getaddrinfo(host, MYPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    int yes = 1;

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        void *ina;

        /** step 1 **/
        if (p->ai_family == AF_INET)
        { // IPv4
            struct sockaddr_in *sa4 = (struct sockaddr_in *)p->ai_addr;
            ina = &(sa4->sin_addr);
        }
        else
        { // IPv6
            struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)p->ai_addr;
            ina = &(sa6->sin6_addr);
        }

        /** step 2 **/
        inet_ntop(p->ai_family, ina, ipstr, sizeof ipstr);

        /** step 3 **/
        if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        /** step 4 **/
        if (connect(sock_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("server: connect");
            continue;
        }

        fprintf(stderr, "socket connected: %s\n", ipstr);
        int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

        break;
    }

    freeaddrinfo(servinfo);

    fprintf(stderr, "connecting on interface: %s\n", ipstr);

    return sock_fd;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int pings_complete(int *pings_received, int host_count)
{
    for (int i = 0; i < host_count; i++)
    {
        if (pings_received[i] == 0)
        {
            return 0;
        }
    }
    return 1;
}

int not_in_closed_channels(int *closed_channels, int fd)
{
    for (int i = 0; i < sizeof closed_channels; i++)
    {
        if (closed_channels[i] == fd)
        {
            return 0;
        }
    }
    return 1;
}
