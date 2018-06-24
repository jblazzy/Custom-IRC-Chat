#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MYPORT "9034"

struct client
{
    char *name;    // store this once we get NAME command
    char *channel; // client's channel(s)? TODO maybe make this bigger to allow for larger channel names
    int fd;        // file descriptor

    struct client *next;
};

client *create_client(int fd)
{
    printf("CREATING CLIENT\n");
    // set default names to this
    char *n = (char *)"defaultdefaultdefaultdefault";

    // create client and malloc space for its values
    struct client *new_client = (client *)malloc(sizeof(client));
    new_client->name = (char *)malloc((strlen(n) + 1) * sizeof(char));
    new_client->channel = (char *)malloc((strlen(n) + 1) * sizeof(char));

    // set default values
    strcpy(new_client->name, n);
    strcpy(new_client->channel, n);
    new_client->fd = fd;
    new_client->next = NULL;

    return new_client;
}

client *identify_client(client *HEAD, int fdTarget)
{
    // take a file descripter and return * to client with that FD, NULL if not found
    printf("IDENTIFYING CLIENT\n");
    struct client *current = HEAD;
    while (current)
    {
        if (current->fd == fdTarget)
        {
            return current;
        }
        current = current->next;
    }
    printf("END OF ID CLIENT\n\n\n");
    return NULL;
}

bool valid_user(client *current_client)
{
    // to check against the defualt values
    char *n = (char *)"defaultdefaultdefaultdefault";
    //printf("name is: %s \n", current_client->name);
    //printf("channel is: %s \n", current_client->channel);
    if (!strcmp(current_client->name, n))
    {
        printf("works\n");
        printf("please set a valid username\n");
        return false;
    }
    if (!strcmp(current_client->channel, n))
    {
        printf("please set a valid channel\n");
        return false;
    }
    return true;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *)sa)->sin_addr);

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int sendall(int s, char *buf, int len)
{
    int total = 0;       // how many bytes we've sent
    int bytesleft = len; // how many we have left to send
    int n;
    while (total < len)
    {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }
    len = total;             // return number actually sent here
    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

client *append(struct client *HEAD, struct client *newClient)
{
    printf("APPENDING CLIENT\n");
    // could pass in head by reference
    if (!HEAD)
    {
        HEAD = newClient;
        HEAD->next = NULL;
        return HEAD;
    }

    struct client *current;
    current = HEAD;
    while (current->next)
    {
        current = current->next;
    }

    current->next = newClient;

    printf("END OF APPEND CLIENT\n\n\n");

    return HEAD;
}

int main()
{
    //=================CREATE LISTENING SOCKET==========================================

    // create a struct for our address info
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // IPv4 or v6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    // get our socket info
    int status; // need to store it for error reporting
    if ((status = getaddrinfo(NULL, MYPORT, &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // select and bind to our socket
    int listening;
    for (p = res; p != NULL; p = p->ai_next)
    {
        listening = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listening < 0)
            continue;

        int yes = 1; // for setsockopt()
        // lose the "address already in use" error message
        setsockopt(listening, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listening, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listening);
            continue;
        }
        break;
    }
    // failed to bind
    if (p == NULL)
    {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(res);

    // listen
    if (listen(listening, 10) == -1)
    {
        perror("listen");
        exit(3);
    }

    fd_set master; // master file descriptor list
    FD_ZERO(&master);
    FD_SET(listening, &master);

    int fdmax = listening; // biggest file descriptor
    int i, j;
    int client_number = 0; // keeps track of the how many clients we have

    //struct client *client_array[40]; // create our array of clients
    struct client *HEAD = NULL;

    //===================SELECT FOR MULTI I/O===========================================
    printf("RUNNING SELECT\n");
    for (;;)
    {
        fd_set copy = master;
        if (select(fdmax + 1, &copy, NULL, NULL, NULL) == -1) // setting last val to NULL means no timeout
        {
            perror("select");
            exit(4);
        }
        // run through the existing connections looking for data to read
        for (i = 0; i <= fdmax; i++)
        {
            //printf("1\n");
            if (FD_ISSET(i, &copy))
            {
                //printf("2\n");
                // found data to read from connection
                if (i == listening)
                {
                    // handle new connection
                    struct sockaddr_storage remoteaddr; // client address
                    socklen_t addrlen = sizeof remoteaddr;

                    int newfd = accept(listening, (struct sockaddr *)&remoteaddr, &addrlen);
                    if (newfd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        FD_SET(newfd, &master);
                        if (newfd > fdmax)
                        {
                            fdmax = newfd;
                        }
                        char remoteIP[INET6_ADDRSTRLEN];
                        printf("selectserver: new connection from %s on "
                               "socket %d \n",
                               inet_ntop(remoteaddr.ss_family,
                                         get_in_addr((struct sockaddr *)&remoteaddr),
                                         remoteIP, INET6_ADDRSTRLEN),
                               newfd);

                        // create a client for this new socket, so that it may be identified again in the future
                        struct client *new_client;
                        new_client = create_client(newfd);
                        HEAD = append(HEAD, new_client); //adding a connection, append the new struct
                        client_number = client_number + 1;
                    }
                }
                else
                {
                    //printf("3\n");
                    // handle data from a client
                    char buf[9999]; // buffer for client data
                    memset(buf, 0, 9999);

                    int nbytes;
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0)
                    {
                        // got connection closed or eror by client
                        if (nbytes == 0)
                        {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        }
                        else
                        {
                            perror("recv");
                        }
                        close(i);
                        FD_CLR(i, &master); // remove from master set
                    }
                    else
                    {
                        //printf("4\n");
                        // create tokenized buffer
                        char token_buf[200];
                        memset(token_buf, 0, 200);
                        strcpy(token_buf, buf);
                        char *command = strtok(token_buf, " "); //strtok returns first split element

                        // identifies the socket with the correct structure
                        struct client *current_client = identify_client(HEAD, i);
                        printf("client socket = %i \n", current_client->fd);

                        // set name
                        if (!strcmp(command, "NAME")) // string compare if == 0 -> strings are equal
                        {
                            char *name = strtok(NULL, token_buf); // get to next token, ie the name
                            strcpy(current_client->name, name);
                            printf("client name set as = %s \n", current_client->name);
                        }
                        printf("client name set to: %s \n", current_client->name);
                        // set channel
                        if (!strcmp(command, "ADD")) // ADD (channel) command
                        {
                            char *channelName = strtok(NULL, token_buf);
                            strcpy(current_client->channel, channelName);
                            printf("channel set to: %s \n", current_client->channel);
                        }
                        printf("channel set to: %s \n", current_client->channel);
                        /*
                        else if (strcmp(command[0], "#") && sendMessages) 
                        {
                            // check list of channels against client's channel name
                            // if no match -> create channel
                            // otherwise prompt client for another channel name
                            
                            if(sendMessages){
                            // specify channel? // currentchannel?
                            // send buffer to other clients in channel
                            }
                        }
                        */

                        /*
                        Below - only send to structs with current client's channel
                        */

                        /* struct client *current = HEAD;
                        while (current)
                        {
                            if (current->fd == fdTarget)
                            {
                                return current;
                            }
                            current = current->next;
                        }*/
                        if (valid_user(current_client))
                        {
                            for (j = 0; j <= fdmax; j++)
                            {
                                // send to everyone...
                                if (FD_ISSET(j, &master))
                                {
                                    // except the listening and ourselves
                                    if (j != listening && j != i)
                                    {
                                        if (sendall(j, buf, nbytes) == -1)
                                        {
                                            perror("send");
                                        }
                                    }
                                }
                            }
                        }
                    }
                } // END handle data from client
            }     // END got new incoming connection
        }         // END looping through file descriptors
    }             // END for(;;)
    return 0;
}