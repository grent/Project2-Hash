#include "network.h"

/*
 * Create and return the main webserver socket.
 * This should only be called once upon server startup.
 */
int prepare_server_socket(unsigned short port) {
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        PRINT_ERROR("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        PRINT_ERROR("setsockopt");
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = PF_INET;
    sin.sin_port = htons(port);

    if (bind(sock, (const struct sockaddr*)&sin, sizeof(sin)) < 0) {
        PRINT_ERROR("bind");
        return -1;
    }

    if (listen(sock, 128) < 0) {
        PRINT_ERROR("listen");
        return -1;
    }

    return sock;
}

/*
 * Get the file name being requested with the current HTTP
 * GET request.  Requires the network socket, a buffer to put
 * the file name in, and the size of the buffer.
 * 
 * It returns 0 on success and -1 on failure.
 *
 * This function is fairly brittle: it doesn't implement HTTP
 * in a fully correct manner and only implements GET requests.
 * Still, good'nuf for this project, and simpler than using
 * libwww or some-such.
 */
int getrequest(int sock, char *reqbuffer, int buffsize) {
    int nbrv = fcntl(sock, F_SETFL, O_NONBLOCK);
    if (nbrv < 0) {
        fprintf(stderr, "Failure in getrequest to set socket in non-blocking mode.  Continuing anyway.\n");
    }
    
    const int max_buffer = 1024;
    int buffer_left = max_buffer;
    char buffer[buffer_left];
    memset(buffer, 0, buffer_left*sizeof(char));

    const char *separator1 = "\r\n\r\n";
    const char *separator2 = "\n\n";
    const char *newline = "\n";

    int returnval = -1;
    
    int wait_time = 5; // wait wait_time seconds for request, then give up
    struct timeval begin, end, diff;
    gettimeofday(&begin, NULL);

    while (TRUE) {
        struct pollfd pfd = {sock, POLLIN};
        int rv = poll(&pfd, 1, (wait_time*1000)/10);
        if (rv < 0) {
            return -1;
        } else if (rv > 0) {
            rv = recv(sock, &buffer[buffer_left - max_buffer], buffer_left, 0);
            if (rv < 0) {
                return -1;
            } else if (rv > 0) {
                buffer_left -= rv;
                if (strstr(buffer, separator1) || strstr(buffer,separator2)) {
                    break;
                }
            }
        }

        // have we waited too long for the request?
        gettimeofday(&end, NULL);
        timersub(&end, &begin, &diff);
        if (diff.tv_sec >= wait_time) {
            return -1;        
        }
    }

    int i = 0;
    for (; i < (max_buffer - buffer_left); i++) {
        if (buffer[i] == '\r')
            buffer[i] = '\n';
    }

    char *t = strtok(buffer, newline);
    while (t != NULL) {
        // does this look like the main request line?
        char *proto = strstr(t, "HTTP/1.");
        if (proto != NULL) {
            // request type should be at the beginning of this line.
            // we only respond to GET requests.  All others are rejected.
            if (!strncmp(t, "GET", 3)) {
                char *first_space = index(t, ' ');
                if (first_space != NULL) {
                    first_space += 1;
                    strncpy(reqbuffer, first_space, MIN(buffsize, (proto - first_space - 1)));
                    returnval = 0;
                }
            }
        } 
        t = strtok(NULL, newline);
    }

    return returnval;
}


/*
 * 
 * Send data on a socket back to a client.  Parameters are the network
 * socket, a buffer containing data the send, and the amount (number of
 * bytes) to send.  Returns the number of bytes written, or -1 on failure.
 *
 */
int senddata(int sock, const char *buffer, int bufflen) {
    if (NULL == buffer) {
        return -1;
    }

    int currpos = 0;
    int tosend = bufflen;
    while (currpos < tosend) {
        int rv = send(sock, buffer+currpos, tosend-currpos, 0);
        if (rv < 0) {
            PRINT_ERROR("send");
            return -1;
        } else if (rv >= 0) {
            currpos += rv;
        }
    }
    return currpos;
}

