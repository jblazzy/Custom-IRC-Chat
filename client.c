// custom IRC client
// created by Josh Blaz, Gezim Saciri, Ben Leipert
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
#include <cassert>

#define PORT "9034"     // the port client will be connecting to
#define MAXDATASIZE 100 // max number of bytes we can get at once

int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    while (total < *len)
    {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }
    *len = total;            // return number actually sent here
    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *)sa)->sin_addr);

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    // command line error, will not need later
    if (argc != 2)
    {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    // create struct for address info
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // get socket info
    int status;
    if ((status = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    // loop through all the results and connect to the first we can
    int sockfd;
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }

    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);

    printf("client: connecting to %s\n", s);
    freeaddrinfo(servinfo); // all done with this structure
    // connection established

    //=================================================================================================
    // TEST STUFF

    /*int n; // declare socket value
    int send(int sockfd, const void *msg, int len, int flags) (always set flags to 0) - returns #bytes sent out 
    char* setName = "NAME gay69\r\n";
    // format with sprintf() later %s 
    //n = send(sockfd, setName, strlen(setName), 0);

    
    //Prompt: Create or Join a channel?
    //        : if they want to join a channel, fetch the list of channels -> LIST command (FIND = LIST)
    //        else: CREATE a channel, but can't be a name in LIST, so we need to call LIST either way
    

    //format with sprintf() later %s 
    n = send(sockfd, joinChannel, strlen(joinChannel), 0);
    */
    //========================================================================================================

    // only need to listen to one socket this entire time, it's the server's socket.
    // we want to use select on sockfd, the socket we're using for the server connection,
    // and STDIN, because that's where we'll get input that we want to send

    fd_set master; // master file descriptor list
    FD_ZERO(&master);
    FD_SET(sockfd, &master); // want to be listening to server
    FD_SET(0, &master);      // want to be listening to stdin

    int fdmax = sockfd; // biggest file descriptor
    int i, nbytes;
    char recieve_buf[1024]; // buffer for recieving data
    long unsigned int iBytes = 256;
    char *buf = (char *)malloc(sizeof(char *) * iBytes); // buffer for sending data

    //===================SELECT FOR MULTI I/O===========================================
    for (;;)
    {
        //printf("INLOOP\n");
        fd_set copy = master;
        if (select(fdmax + 1, &copy, NULL, NULL, NULL) == -1) // setting last val to NULL means no timeout
        {
            perror("select");
            exit(4);
        }
        // run through the existing connections looking for data to read
        for (i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &copy))
            {
                if (i == sockfd) // found data from server
                {
                    // handle data from a client
                    if ((nbytes = recv(i, recieve_buf, sizeof recieve_buf, 0)) == -1)
                    {
                        // no data from server
                        perror("recv");
                        exit(5);
                    }

                    if (recieve_buf[nbytes - 1] == '\n') //ignore newline that's typed when client presses enter
                    {
                        nbytes--;
                    }
                    recieve_buf[nbytes] = '\0';

                    printf("client: received '%s'\n", recieve_buf);
                }
                else if (i == 0)
                {
                    // get data using getline, not recv cause stdin isn't a socket
                    int iBytesRead = getline(&buf, &iBytes, stdin);
                    if (iBytesRead == -1)
                    {
                        printf("error: no data read in from getline \n");
                    }

                    // reading data from stdin, need to send to server
                    int len = strlen(buf);
                    if (sendall(sockfd, buf, &len) == -1)
                    {
                        perror("sendall");
                        printf("We only sent %d bytes because of the error!\n", len);
                    }
                }
            }
        } // END looping through file descriptors
    }     // END for(;;)

    return 0;
}