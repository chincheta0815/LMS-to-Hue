/*
 *  libhuec - philips hue library for C
 *
 *  (c) Rouven Weiler 2017
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define _GNU_SOURCE

#define HUE_MSG_CHUNK 1024

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#if __sun__
#include <sys/ddi.h>
#endif

#include "hue_interface.h"

/*----------------------------------------------------------------------------*/
char *hue_request_method2string(int method_value) {
    switch(method_value) {
            case GET:
                return "GET";
            case DELETE:
                return "DELETE";
            case POST:
                return "POST";
            case PUT:
                return "PUT";
    }
    return "";
}


/*----------------------------------------------------------------------------*/
#if OSX
static void set_nosigpipe(sockfd s) {
	int set = 1;
	setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
}
#else
#define set_nosigpipe(s)
#endif


/*----------------------------------------------------------------------------*/
static void set_nonblock(sockfd s) {
#if WIN
	u_long iMode = 1;
	ioctlsocket(s, FIONBIO, &iMode);
#else
	int flags = fcntl(s, F_GETFL,0);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}


/*----------------------------------------------------------------------------*/
static void set_block(sockfd s) {
#if WIN
	u_long iMode = 0;
	ioctlsocket(s, FIONBIO, &iMode);
#else
	int flags = fcntl(s, F_GETFL,0);
	fcntl(s, F_SETFL, flags & (~O_NONBLOCK));
#endif
}


/*----------------------------------------------------------------------------*/
static int connect_timeout(sockfd sock, const struct sockaddr *addr, socklen_t addrlen, int timeout) {
	fd_set w, e;
	struct timeval tval;

	if (connect(sock, addr, addrlen) < 0) {
#if !WIN
		if (errno != EINPROGRESS) {
#else
		if (errno != WSAEWOULDBLOCK) {
#endif
			return -1;
		}
	}

	FD_ZERO(&w);
	FD_SET(sock, &w);
	e = w;
	tval.tv_sec = timeout;
	tval.tv_usec = 0;

	// only return 0 if w set and sock error is zero, otherwise return error code
	if (select(sock + 1, NULL, &w, &e, timeout ? &tval : NULL) == 1 && FD_ISSET(sock, &w)) {
		int	error = 0;
		socklen_t len = sizeof(error);
		getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *)&error, &len);
		return error;
	}

	return -1;
}

/**
 * @desc Creates a socket used for communicating with a hue bridge
 *
 * @param pointer to a variable of type hue_bridge_t
 * @return 0 for success or 1 for error
 */
extern int hue_connect(hue_bridge_t *bridge) {
	int err;
	struct sockaddr_in addr;

#ifdef _LIBHUEC_USE_PTHREADS
	pthread_mutex_lock(&bridge->mutex);
#endif

	// do nothing if we are already connected
	bridge->sock = socket(AF_INET, SOCK_STREAM, 0);
	set_nonblock(bridge->sock);
	set_nosigpipe(bridge->sock);

	addr.sin_family = AF_INET;
#if WIN
	addr.sin_addr.s_addr = bridge->ip.S_un.S_addr;
#else
	addr.sin_addr.s_addr = bridge->ipAddress.s_addr;
#endif
	addr.sin_port = htons(80);

	err = connect_timeout(bridge->sock, (struct sockaddr *) &addr, sizeof(addr), 2);

	if (err) {
		close(bridge->sock);
		printf("Cannot open socket connection (%d)", err);
#ifdef _LIBHUEC_USE_PTHREADS
		pthread_mutex_unlock(&bridge->mutex);
#endif
		return -1;
	}

	set_block(bridge->sock);

#ifdef _LIBHUEC_USE_PTHREADS
	pthread_mutex_unlock(&bridge->mutex);
#endif

    printf("Socket opened!\n");

	return 0;
}

/**
 * @desc Closes a socket used for communicating with a hue bridge
 *
 * @param pointer to a variable of type hue_bridge_t
 * @return 0 for success or 1 for error
 */
extern int hue_disconnect(hue_bridge_t *bridge) {
    int rv;

#ifdef _LIBHUEC_USE_PTHREADS
    pthread_mutex_lock(&bridge->mutex);
#endif

    rv = close(bridge->sock);

#ifdef _LIBHUEC_USE_PTHREADS
    pthread_mutex_unlock(&bridge->mutex);        
#endif

    return rv;
}


/**
 * @desc Creates and sends a request to a hue bridge
 *
 * @param pointer to a variable of type hue_bridge_t
 * @param pointer to a variable of type hue_request_t
 * @return 0 for success or 1 for error
 */
extern int hue_send_request(hue_bridge_t *bridge, hue_request_t *request) {
    char *message_header;

#ifdef _LIBHUEC_USE_PTHREADS
    pthread_mutex_lock(&bridge->mutex);
#endif


#ifdef _LIBHUEC_USE_PTHREADS
    pthread_mutex_unlock(&request->mutex);
#endif

    asprintf(&message_header, "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s%s\r\nContent-Type: application/json\r\nContent-Length: %d",
            hue_request_method2string(request->method), request->uri, inet_ntoa(bridge->ipAddress), "libhuec/", LIBHUEC_VERSION, strlen(request->body));
    free(request->uri);
    
    char *complete_message;
    asprintf(&complete_message,"%s\r\n\r\n%s", message_header, request->body);
    free(message_header);
    free(request->body);    

#ifdef _LIBHUEC_USE_PTHREADS
    pthread_mutex_unlock(&request->mutex);
#endif

    printf("Sending to Hue:\n%s\n", complete_message);
    send(bridge->sock, complete_message, strlen(complete_message), 0);
    free(complete_message);

#ifdef _LIBHUEC_USE_PTHREADS
    pthread_mutex_unlock(&bridge->mutex);
#endif
   
    return 0;
}


/**
 * @desc Recevies a response from a hue bridge
 *
 * @param pointer to a variable of type hue_bridge_t
 * @param pointer to a variable of type hue_response_t
 * @return 0 for success or 1 for error
 */
extern int hue_receive_response(hue_bridge_t *bridge, hue_response_t *response) {
    int len = 0;
    int wait = 100;
    int size = HUE_MSG_CHUNK;
    int remainingBodyLength = 0;
    int ret = 1;
    char *response_message = malloc(size);

    if (!response_message) {
        return 1;
    }

    while (wait--) {
        fd_set rfds;
        struct timeval timeout = {0, 10000};

#ifdef _LIBHUEC_USE_PTHREADS
        pthread_mutex_lock(&bridge->mutex);
#endif

        FD_ZERO(&rfds);
        FD_SET(bridge->sock, &rfds);

        if (select(bridge->sock + 1, &rfds, NULL, NULL, &timeout) < 0) {
            // socket has closed, which seems to be what you are expecting with Hue bridge
            // yes according to the hue devs that is exactly what I have to wait for
            // maybe the header 'Connection: close' can help here
            break;
        }

        if (FD_ISSET(bridge->sock, &rfds)) {
            int bytesReceived;
			char *p;

            // something usefull received
			bytesReceived = recv(bridge->sock, response_message + len, size - len - 1, 0);

            if (bytesReceived < 0) {
                // oops ... should not happen
                break;
            }

            len += bytesReceived;

            if (len == size - 1) {
                size += HUE_MSG_CHUNK;
                response_message = realloc(response_message, size);
                if(!response_message) {
                    break;
                }
            }

            if (remainingBodyLength) {
                remainingBodyLength -= bytesReceived;
				if (remainingBodyLength <= 0) {
                    break;
                }
            }
            else if ((p = strnstr(response_message, "Content-Length", len)) != NULL) {
                if (!strstr(p, "\r\n")) {
                    continue;	// EoL not in buffer, next time then
                }
                sscanf(p, "Content-Length: %d", &remainingBodyLength);
                remainingBodyLength -=  len - (strstr(p, "\r\n") + 2 - response_message);
            }
            
            // Maybe better to take that if-statement out of the while-loop?
#ifdef _LIBHUEC_USE_PTHREADS
            pthread_mutex_lock(&response->mutex);
#endif

            if ((p = strnstr(response_message, "HTTP", len)) != NULL) {
                if (!strstr(p, "\r\n")) {
                    continue;	// EoL not in buffer, next time then
                }
                sscanf(p, "HTTP/1.1 %d ", &response->status_code);
            }

#ifdef _LIBHUEC_USE_PTHREADS
            pthread_mutex_unlock(&response->mutex);
#endif

        }

#ifdef _LIBHUEC_USE_PTHREADS       
        pthread_mutex_unlock(&bridge->mutex);
#endif

    }

    if (!wait) {
        // timeout occured
        // I would rather log a warning here as timeout does not mean failure
        printf("Timeout occured!");
        ret = 0;
    }

    if (len && response_message) {
        char *p;

        response_message[len] = '\0';

#ifdef _LIBHUEC_USE_PTHREADS
        pthread_mutex_lock(&response->mutex);
#endif

        if ((p = strstr(response_message, "\r\n\r\n")) != NULL) {
            response->body = strdup(p) + 4;
            ret = 0;
        }
                
        free(response_message);

#ifdef _LIBHUEC_USE_PTHREADS
        pthread_mutex_unlock(&response->mutex);
#endif

    }

    return ret;

}
