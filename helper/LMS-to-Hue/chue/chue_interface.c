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

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CHUE_MSG_CHUNK 1024

#include "chue_interface.h"
#include "chue_log.h"

static chue_loglevel_t *clp = &chue_loglevel;

#if SUNOS
#include <sys/ddi.h>
#endif

/*----------------------------------------------------------------------------*/
char *chue_request_method2string(int method_value) {
    switch(method_value) {
			case _GET:
                return "GET";
			case _DELETE:
				return "DELETE";
			case _POST:
				return "POST";
            case _PUT:
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

/*
 * @desc Creates a socket used for communicating with a hue bridge
 *
 * @param pointer to a variable of type chue_bridge_t
 * @return 0 for success or 1 for error
 */
extern int chue_connect(chue_bridge_t *bridge) {
	int err;
	struct sockaddr_in addr;

	// do nothing if we are already connected
	bridge->sock = socket(AF_INET, SOCK_STREAM, 0);
	set_nosigpipe(bridge->sock);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = bridge->ip_address.s_addr;
	addr.sin_port = htons(80);

	err = connect_timeout(bridge->sock, (struct sockaddr *) &addr, sizeof(addr), 2);

	if (err) {
		chue_disconnect(bridge);
		CHUE_DEBUG("Cannot open socket connection (%d)", err);
		return 1;
	}

	CHUE_SDEBUG("Socket opened!\n");

	return 0;
}

/*
 * @desc Closes a socket used for communicating with a hue bridge
 *
 * @param pointer to a variable of type chue_bridge_t
 * @return 0 for success or 1 for error
 */
extern int chue_disconnect(chue_bridge_t *bridge) {
	int rv = 0;

	if (bridge->sock != -1) {
		rv = close(bridge->sock);
		bridge->sock = -1;
	}

	return rv;
}


/*
 * @desc Creates and sends a request to a hue bridge
 *
 * @param pointer to a variable of type chue_bridge_t
 * @param pointer to a variable of type chue_request_t
 * @return 0 for success or 1 for error
 */
extern int chue_send_request(chue_bridge_t *bridge, chue_request_t *request) {
	char *message_header;
    char *complete_message;

	asprintf(&message_header, "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s%s\r\nContent-Type: application/json\r\nContent-Length: %d",
			chue_request_method2string(request->method), request->uri, inet_ntoa(bridge->ip_address), "chue/", CHUE_VERSION, strlen(request->body));

	asprintf(&complete_message,"%s\r\n\r\n%s", message_header, request->body);
    free(message_header);

	CHUE_SDEBUG("Sending to Hue:\n%s\n", complete_message);
	send(bridge->sock, complete_message, strlen(complete_message), 0);
	free(complete_message);

    return 0;
}


/*
 * @desc Recevies a response from a hue bridge
 *
 * @param pointer to a variable of type chue_bridge_t
 * @param pointer to a variable of type chue_response_t
 * @return 0 for success or 1 for error
 */
extern int chue_receive_response(chue_bridge_t *bridge, chue_response_t *response) {
    int len = 0;
    int wait = 100;
    int size = CHUE_MSG_CHUNK;
    int remainingBodyLength = 0;
    int ret = 1;
    char *response_message = malloc(size);

    if (!response_message) {
        return 1;
	}

	response->body = NULL;

    while (wait--) {
        fd_set rfds;
        struct timeval timeout = {0, 10000};

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
				size += CHUE_MSG_CHUNK;
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
			if ((p = strnstr(response_message, "HTTP", len)) != NULL) {
				if (!strstr(p, "\r\n")) {
					continue;	// EoL not in buffer, next time then
				}
				sscanf(p, "HTTP/1.1 %d ", &response->status_code);
			}
		}
	}

	if (!wait) {
		// timeout occured
		// I would rather log a warning here as timeout does not mean failure
		CHUE_WARN("Timeout occured!");
		ret = 0;
	}

	if (len && response_message) {
		char *p;

		response_message[len] = '\0';

		if ((p = strstr(response_message, "\r\n\r\n")) != NULL) {
			response->body = strdup(p + 4);
			ret = 0;
		}

		free(response_message);
	}

	return ret;

}
