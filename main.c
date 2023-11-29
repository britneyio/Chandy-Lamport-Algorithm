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
#define BACKLOG 10

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
int incoming_closed_channels[4];
int ic_channels_count = 0;
int has_token = 0;
int snapshot_started = 0;
int all_closed = 0;
int *token_array;
int token_array_size;
pthread_mutex_t has_token_mutex = PTHREAD_MUTEX_INITIALIZER;


enum messageType {TOKEN, MARKER};

struct state
    {
        int id;
        int predecessor;
        int successor;
        int state_counter;
        int record;
        enum messageType m;
    } ss;

int received_messages[4];



int create_listen_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p);
void initialize_peer_send_list(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p);
int create_send_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p, char *host);
void get_peer_list(char *filename);
void serialize(const struct state *s, char *buffer, size_t buffer_size);
void deserialize(const char *buffer, struct state *s);
void command_inputs(int argc, char *argv[]);
void message_type_determination(struct state s, int fd);
int write_to_socket(int socket_fd, const char *data, size_t size);
void send_token();
void record_state();
void close_received_channel(int channel_received);
void send_marker();
void record_incoming_messages(struct state s);
void start_snapshot(struct state s);
void end_snapshot();
void serialize(const struct state *s, char *buffer, size_t buffer_size);
int is_closed_channel(int id, int arr[4]);
void add_token_array(int **array, int *size, int element);
void empty_array(int **array, int *size);
void *token_thread(void *arg);

int main(int argc, char *argv[])
{
    struct addrinfo hints, *servinfo, *p;
    char hostname[10];
    hostname[9] = '\0';
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    int accept_fd;
        struct timeval timeout;
    timeout.tv_sec = 1; // 1-second timeout
    timeout.tv_usec = 0;
    fd_set master;   // master file descriptor list
    fd_set read_fds; // temp file descriptor list for select()
    int fdmax;       // max file descriptor number

    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    char received_buffer[1024];
    size_t received_size;

    memset(incoming_closed_channels, -1, sizeof(int) * 4);
    token_array = (int *)malloc(sizeof(int) * 3);
    token_array_size = 1;
    memset(token_array, 0, sizeof(int) * token_array_size);


    command_inputs(argc,argv);
    get_peer_list(file);

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
    
    if (snapshot_starter == 1 && ss.state_counter == state_req) {
        //fprintf(stderr, "snapshot starter id: %d", ss.id);
        if (token == 1) {
            has_token = 1;
        }
        start_snapshot(ss);
    }
    pthread_t tid;

    if (token == 1) {
        ss.state_counter++;
        fprintf(stderr, "{id: %d, sender: %d, receiver: %d, message:‘‘token’’}\n", ss.id, ss.successor, ss.id);
        pthread_mutex_lock(&has_token_mutex);
        has_token = 1;
        pthread_mutex_unlock(&has_token_mutex);        fprintf(stderr, "{id: %d, state: %d}\n",ss.id, ss.state_counter);
        // send_token();
        // usleep(delay_time * 1000000);

    }


   while (1)
{
        if (pthread_create(&tid, NULL, token_thread, NULL) != 0) {
            perror("Thread creation failed");
            exit(0);
    }
    if (snapshot_starter == 1 && ss.state_counter == state_req) {
        start_snapshot(ss);
        snapshot_starter = 0;
    }

    read_fds = master;
    if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
    {
        perror("select");
        exit(4);
    }

    // run thru existing connections looking for data to read
    for (int i = 0; i <= fdmax; i++)
    {
        if (FD_ISSET(i, &read_fds))
        {

            if (i == listener)
            {

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

                }
            }
            else
            {
                int nbytes;
                // handle data from channels
                if((nbytes = recv(i, &received_buffer, sizeof(received_buffer), 0)) > 0)
                {
                    struct state s;
                    deserialize(received_buffer, &s);
                    message_type_determination(s, i);

                } 
                else {
                    perror("recv");
                    close(i);
                    FD_CLR(i, &master); // remove from master set
                }

            }
        }
    }

}


    return 0;
}

void message_type_determination(struct state s, int fd) {
    if (s.m == TOKEN) {
        fprintf(stderr, "{id: %d, sender: %d, receiver: %d, message:‘‘token’’}\n", ss.id, s.id, ss.id);
        pthread_mutex_lock(&has_token_mutex);
        has_token = 1;
        pthread_mutex_unlock(&has_token_mutex);
        ss.state_counter++;
        fprintf(stderr, "{id: %d, state: %d}\n",ss.id, ss.state_counter);
        // usleep(delay_time * 1000000);
        // send_token();       
    }

    if (s.m == MARKER) {

        if (snapshot_started == 1) {
            if (is_closed_channel(s.id, incoming_closed_channels) == 0) {
                    record_incoming_messages(s);
                    close_received_channel(s.id);
                    end_snapshot();

            }
        } else {
             start_snapshot(s);

        }



    }

}



void start_snapshot(struct state s) {
    fprintf(stderr, "{id:%d snapshot:‘‘started’’}\n", ss.id);
    snapshot_started = 1;
    record_state();
    record_incoming_messages(s);
    close_received_channel(s.id);
    usleep(marker_delay * 1000000);
    send_marker();
}

void record_state() {
    if (has_token == 1) {
        ss.record = 1;
    }
}


void close_received_channel(int channel_id) {
    if (channel_id != ss.id) {
    fprintf(stderr, "{id:%d, snapshot:‘‘channel closed’’, channel:%d-%d, queue:[%s]}\n", ss.id, channel_id, ss.id, received_messages[channel_id] == 1 ? "Token" : "");
    incoming_closed_channels[ic_channels_count]= channel_id;
    ic_channels_count++;
    } 
}


void record_incoming_messages(struct state s) {
    if (s.id != ss.id) {
   received_messages[s.id] = s.record;
    }
}

void add_token_array(int **array, int *size, int element) {
    (*size)++;
    *array = realloc(*array, (*size) * sizeof(int));
    if (*array == NULL) {
        printf("Memory reallocation failed. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    (*array)[(*size) - 1] = element;
}

void empty_array(int **array, int *size) {
    free(*array);
    *size = 0;
    *array = NULL;
}


void end_snapshot() {
    all_closed = 0;
    for (int i = 0; i < ic_channels_count; i++) {
        //fprintf(stderr, "cl : %d", closed_channels[i]);
        if (incoming_closed_channels[i] > -1) {
            all_closed++;
        }
    }

    fprintf(stderr, "all_closed: %d\n", all_closed);
    if (all_closed == peer_count - 1) {
        empty_array(&token_array, &token_array_size);
        snapshot_started = 0;
        memset(incoming_closed_channels, -1, sizeof(int) * 4);
        ic_channels_count = 0;
        all_closed = 0;
        fprintf(stderr, "{id:%d, snapshot:‘‘complete’’}\n", ss.id);
    } 
    if (all_closed > peer_count -1) {
        fprintf(stderr, "ERROR : received more than possible channels");
    }
}

int is_closed_channel(int id, int arr[4]) {
    for (int i = 0; i < peer_count -1; i++) {
        if (id == arr[i]) {
            return 1;
        }
    }
    return 0;
}
void send_marker() {
    struct state s;
    s.id = ss.id;
    s.predecessor = ss.predecessor;
    s.successor = ss.successor;
    s.m = MARKER;
    s.state_counter = ss.state_counter;
    s.record = ss.record;

    int err;

    int buffer_size = sizeof(int) * 5 + sizeof(enum messageType);
    char *buffer = (char *)malloc(buffer_size);
    serialize(&s, buffer, buffer_size);

    for (int i = 0; i < peer_count; i++) {
        if (i != ss.id) {
            if ((err = write_to_socket(peer_fd[i], buffer, buffer_size)) != 0){
                fprintf(stderr, "error writing my message\n");
            }
            else{
                fprintf(stderr, "{id:%d, sender:%d, receiver:%d, msg:‘‘marker’’, state:%d, has_token:%s}\n", ss.id, ss.id, i,ss.state_counter, ss.record ? "YES" : "NO");
            }
        }
    }
    free(buffer);
}


void send_token() {
    struct state s;
    s.id = ss.id;
    s.predecessor = ss.predecessor;
    s.successor = ss.successor;
    s.m = TOKEN;
    s.state_counter = ss.state_counter;
    s.record = 0;

    int err;

    int buffer_size = sizeof(int) * 5 + sizeof(enum messageType);
    char *buffer = (char *)malloc(buffer_size);
    serialize(&s, buffer, buffer_size);

    if ((err = write_to_socket(peer_fd[ss.successor], buffer, buffer_size)) != 0)
    {
        fprintf(stderr, "error writing my message\n");
    }
    else
    {
        fprintf(stderr, "{id: %d, sender: %d, receiver: %d, message:‘‘token’’}\n", ss.id, ss.id, ss.successor);
        pthread_mutex_lock(&has_token_mutex);
        has_token = 0;
        pthread_mutex_unlock(&has_token_mutex);
    }
    free(buffer);

}

void *token_thread(void *arg) {
    if(has_token) {
    usleep(delay_time * 1000000);
    send_token();
    }
}

int write_to_socket(int socket_fd, const char *data, size_t size)
{
    ssize_t bytes_written = send(socket_fd, data, size, MSG_DONTWAIT);
    if (bytes_written < 0)
    {
        perror("Error writing to socket");
        return -1;
    }
    return 0;
}

// Function to serialize a struct state
void serialize(const struct state *s, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%d,%d,%d,%d,%d,%d", s->id, s->predecessor, s->successor, s->state_counter, s->record, s->m);
}

// Function to deserialize a string into a struct state
void deserialize(const char *buffer, struct state *s) {
    sscanf(buffer, "%d,%d,%d,%d,%d,%d", &s->id, &s->predecessor, &s->successor, &s->state_counter, &s->record, &s->m);
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
    //fprintf(stderr, "hostname: %s\n", hostname);
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
        peer_count = host_i;
        //fprintf(stderr, "peer_count: %d", peer_count);

      if (ss.id == 0)
        {
        ss.predecessor = peer_count - 1;
            }
    if (ss.id == peer_count - 1)
    {
        ss.successor = 0;
    }
        // fprintf(stderr, "{id: %d, predecessor: %d, successor: %d, message:‘‘token’’}\n", ss.id, ss.predecessor, ss.successor);

    fclose(fptr);
}

int create_send_socket(struct addrinfo hints, struct addrinfo *servinfo, struct addrinfo *p, char *host)
{
    int sock_fd;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    
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

        
        if (connect(sock_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("server: connect");
            continue;
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
            if (i != ss.id) {
            peer_fd[i] = create_send_socket(hints, servinfo, p, peer_list[i]);
            }
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


void command_inputs(int argc, char *argv[])
{
        for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-t") == 0)
        {
            delay_time = atof(argv[i + 1]);
        }
        if (strcmp(argv[i], "-x") == 0)
        {
            token = 1;
        }
        if (strcmp(argv[i], "-h") == 0)
        {
            file = argv[i + 1];
        }
        if (strcmp(argv[i], "-m") == 0)
        {
            marker_delay = atof(argv[i + 1]);
        }
        if (strcmp(argv[i], "-s") == 0)
        {
            state_req = atoi(argv[i + 1]);
            snapshot_starter = 1;
        }
    }
}