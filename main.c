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

#define MYPORT "4950"
#define BACKLOG 5

int *peer_fd;
char **peer_list;
int peer_count = 0;
float delay_time;
float marker_delay;
int token = 0;
char *file;
int marker_start;
int state_req;
int snapshot_starter = 0;
int listener;



struct state
    {
        int id;
        int predecessor;
        int successor;
        int state_counter;
        int record;
    } ss;

struct token {};

int create_listen_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p);
void initialize_peer_send_list(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p);
int create_send_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p, char *host, int token);
void get_peer_list(char *filename);
int receive(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p, char hosts[10][20], int current_host);
void serialize_message(const struct state *msg, char *buffer, int buffer_size);
void unserialize_message(char *buffer, int buffer_size, struct state *msg);
int pings_complete(int *pings_received, int host_count);
int not_in_closed_channels(int *closed_channels, int fd);
void command_inputs(int argc, char *argv[]);

int main(int argc, char *argv[])
{
                fprintf(stderr, "started");



    struct addrinfo hints, *servinfo, *p;
    char hostname[10];
    hostname[9] = '\0';
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
    int received_marker = 0;
    int marker_len = strlen(marker);
    int marker_channel;
    ss.id = -1;
    ss.predecessor = -1;
    ss.successor = -1;
    ss.state_counter = 0;
    struct timeval tv;
    fd_set master;   // master file descriptor list
    fd_set read_fds; // temp file descriptor list for select()
    int fdmax;       // max file descriptor number

    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    int channel_i = 0;




    ss.state_counter = snapshot_starter;
    int closed_channels[5];

    listener = create_listen_socket(hints, servinfo, p);

        if (listen(listener, BACKLOG) == -1)
    {
        perror("server: listen");
    }

    sleep(2);

        initialize_peer_send_list(hints, servinfo, p);


    FD_SET(listener, &master); // add listener to the master set
    fdmax = listener;          // keep track of the biggest file descriptor
    fprintf(stderr, "{id: %d, state: %d, predecessor: %d, successor: %d}\n", ss.id, ss.state_counter, ss.predecessor, ss.successor);
            fprintf(stderr, "hi\n");
    int nbytes;

   while (1)
{
    read_fds = master;
        fprintf(stderr, "select start\n");

    if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
    {
        perror("select");
        exit(4);
    }

            fprintf(stderr, "select end\n");


    // run thru existing connections looking for data to read
    for (int i = 0; i <= fdmax; i++)
    {
        if (FD_ISSET(i, &read_fds))
        {
            fprintf(stderr, "isset\n");

            if (i == listener)
            {
                fprintf(stderr, "is listen_fd\n");

                // handle new connections
                // accept from channels
                socklen_t addr_size = sizeof their_addr;
                int accept_fd = accept(listener, (struct sockaddr *)&their_addr, &addr_size);
                fcntl(accept_fd, F_SETFL, O_NONBLOCK);

                if (accept_fd == -1)
                {
                    perror("accept");
                }
                else
                {
                    FD_SET(accept_fd, &master); // add to master set
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
                if (recv(i, buf, 256, 0) <= 0)
                {
                    perror("recv");
                    close(i);
                    FD_CLR(i, &master); // remove from master set
                }
                else {
                fprintf(stderr, "received: %s\n", buf);

                for (int j = 0; j <= fdmax; j++)
                {
                    if (FD_ISSET(j, &master))
                    {
                        if (j != listen_fd && j != i)
                        {
                            // if have token, send it to successor
                            if (token == 1)
                            {
                                fprintf(stderr, "I have the token. Attempting to send:\n");
                                msg_len = strlen(msg);

                                if (send(j, msg, msg_len, 0) == -1)
                                {
                                    perror("send");
                                }
                                fprintf(stderr, "{id: %d, state: %d, sender: %d, receiver: %d}\n", ss.id, ss.state_counter, ss.id, ss.successor);
                                token = 0;
                            }

                            if (received_marker == 1 && not_in_closed_channels(closed_channels, j) == 1)
                            {
                                fprintf(stderr, "I have the marker. Attempting to send:\n");

                                if (send(j, marker, marker_len, 0) == -1)
                                {
                                    perror("send");
                                }
                                fprintf(stderr, "{id: %d,  sender: %d, receiver: %d, msg: ''marker'', state: %d, has_token:%d}\n", ss.id, ss.predecessor, ss.successor, ss.state_counter, token);
                            }
                        }
                    }
                }
            }
            }
        }
    }
    
    // fprintf(stderr, "{id: %d, state: %d, sender: %d, receiver: %d}\n", ss.id, ss.state_counter, ss.predecessor, ss.id);
}


    return 0;
}

void get_peer_list(char *filename)
{
    char hostname[10];
    hostname[9] = '\0';
    FILE *fptr;
    peer_list = malloc(10 * sizeof(char *));
    if (peer_list == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    int host_i = 0;
    // fprintf(stderr, "Opening host file:%s\n", filename);
    // Open file
    fptr = fopen(filename, "r");
    if (fptr == NULL)
    {
        printf("Cannot open file \n");

        exit(0);
    }

    gethostname(hostname, 10);
    fprintf(stderr, "hostname: %s\n", hostname);
    char name[10];
    // Read contents from file
    while (fscanf(fptr, "%s", name) == 1)

    {
        peer_list[host_i] = strdup(name);
        // fprintf(stderr, "peerlist[%d]: %s\n", host_i, peer_list[host_i]);

        if (strcmp(peer_list[host_i], hostname) == 0)
        {
            ss.id = host_i;
            ss.predecessor = ss.id - 1;
            ss.successor = ss.id + 1;
          
        }
        host_i++;
    }
      if (ss.id == 0)
        {
        ss.predecessor = peer_count - 1;
            }
            if (ss.id == peer_count - 1)
    {
        ss.successor = 1;
    }
    peer_count = host_i;
    if (peer_count <= 0) {
        fprintf(stderr, "peer_count is invalid");
    }
    fclose(fptr);
}

int create_send_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p, char *host, int token)
{
    int sock_fd;
    char ipstr[100];
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
        if (token == 1) {
    hints.ai_socktype = SOCK_DGRAM;
    } else {
    hints.ai_socktype = SOCK_STREAM;
    }
    if ((status = getaddrinfo(host, MYPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    int yes = 1;

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (token != 1) {
        if (connect(sock_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("server: connect");
            continue;
        }
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "error in socket");
    }
            int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);


    freeaddrinfo(servinfo);

    return sock_fd;
}

void initialize_peer_send_list(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p)
{
    peer_fd = (int*) malloc((peer_count) * sizeof(int));

    memset(peer_fd, 0, (peer_count) * sizeof(int));

    for (int i = 0; i < peer_count; i++)
    {
            peer_fd[i] = create_send_socket(hints, servinfo, p, peer_list[i], 0);
    }
                



}

int create_listen_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p)
{

    int listener_fd;
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
        /** step 3 **/
        if ((listener_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        /** step 4 **/
        if (bind(listener_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("server: bind");
            continue;
        }

        break;
    }

    return listener_fd;
}

int receive(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p, char hosts[10][20], int current_host)
{
    int recv_fd;
    memset(&hints, 0, sizeof hints);
    memset(&servinfo, 0, sizeof servinfo);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    fprintf(stderr, "receiving from: %s\n", hosts[current_host]);
    int status = getaddrinfo(hosts[current_host], MYPORT, &hints, &servinfo);

    if (status != 0)
    {
        fprintf(stderr, "getaddrinfo-talker: %s\n", gai_strerror(status));
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        recv_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (recv_fd == -1)
        {
            perror("peer: socket");
            continue;
        }
        if (connect(recv_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(recv_fd);
            perror("client: connect");
            continue;
        }
        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structur
    fprintf(stderr, "completed receiving\n");

    return recv_fd;
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

void command_inputs(int argc, char *argv[])
{
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
}