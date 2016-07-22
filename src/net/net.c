/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <errno.h>

#if defined(_WIN32)
#define ioctl ioctlsocket
#else
#include <netdb.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#endif

#include "net.h"

in_addr_t net_lo;

int32_t Net_GetError(void) {
#if defined(_WIN32)
	return WSAGetLastError();
#else
	return errno;
#endif
}

/*
 * @return A printable error string for the most recent OS-level network error.
 */
const char *Net_GetErrorString(void) {
	return strerror(Net_GetError());
}

/*
 * @brief Initializes the specified sockaddr according to the net_addr_t.
 */
void Net_NetAddrToSockaddr(const net_addr_t *a, struct sockaddr *s) {

	memset(s, 0, sizeof(*s));

	if (a->type == NA_BROADCAST) {
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_port = a->port;
                ((struct sockaddr_in *)s)->sin_addr.s_addr = INADDR_BROADCAST;
        }
        else if( a->type == NA_IP ) {
                ((struct sockaddr_in *)s)->sin_family = AF_INET;
                ((struct sockaddr_in *)s)->sin_addr.s_addr = *(int *)&a->ip4;
                ((struct sockaddr_in *)s)->sin_port = a->port;
        }
        else if( a->type == NA_IP6 ) {
                ((struct sockaddr_in6 *)s)->sin6_family = AF_INET6;
                ((struct sockaddr_in6 *)s)->sin6_addr = * ((struct in6_addr *) &a->ip6);
                ((struct sockaddr_in6 *)s)->sin6_port = a->port;
                ((struct sockaddr_in6 *)s)->sin6_scope_id = a->scope_id;
        }		
}

/*
 * @return True if the addresses share the same base and port.
 */
_Bool Net_CompareNetaddr(const net_addr_t *a, const net_addr_t *b) {
	if (a->type == b->type) {
		return a->ip4 == b->ip4 && a->ip6 == b->ip6 && a->port == b->port;
	}
	return false;
}

/*
 * @return True if the addresses share the same type and base.
 */
_Bool Net_CompareClientNetaddr(const net_addr_t *a, const net_addr_t *b) {
	return a->type == b->type && a->ip4 == b->ip4 && a->ip6 == b->ip6;
}

/*
 * @brief
 */
const char *Net_NetaddrToString(const net_addr_t *a) {
	static char s[64];

	g_snprintf(s, sizeof(s), "%s:%i", inet_ntoa(*(struct in_addr *) &a->addr), ntohs(a->port));

	return s;
}

// look for a specific address type (v6 or v4)
static struct addrinfo *Net_SearchAddrInfo(struct addrinfo *hints, sa_family_t family) {
	while (hints) {
		if (hints->ai_family == family)
			return hints;

		hints = hints->ai_next;
	}

        return NULL;
}

/*
 * @brief Resolve internet hostnames to sockaddr. Examples:
 *
 * localhost
 * idnewt
 * idnewt:28000
 * 192.246.40.70
 * 192.246.40.70:28000
 */
_Bool Net_StringToSockaddr(const char *s, struct sockaddr *sa, size_t sa_len, sa_family_t family) {

	memset(saddr, 0, sizeof(*sa));

	char *node = g_strdup(s);

	char *service = strchr(node, ':');
	if (service) {
		*service++ = '\0';
	}

	const struct addrinfo hints = {
		.ai_family = family,
		.ai_socktype = SOCK_DGRAM,
	};

	struct addrinfo *info;
	if (getaddrinfo(node, service, &hints, &info) == 0) {

		struct addrinfo *search = NULL;

		if (family == AF_UNSPEC) {

			// prioritize IPv6
			search = Net_SearchAddrInfo(res, AF_INET6);
			if (!search) {
				search = Net_SearchAddrInfo(res, AF_INET);
			}
		} else {
			search = Net_SearchAddrInfo(res, family);
		}

		if (search) {
			if (search->ai_addrlen > sa_len)
				search->ai_addrlen = sa_len;
			
			memcpy(sa, search->ai_addr, search->ai_addrlen);
			freeaddrinfo(res);

			return true;
		} else {
			Com_Printf("Net_StringToSockaddr: Error resolving %s: No address of required type found.\n", s);
		}	
	} else {
		Com_Printf("Net_StringToSockaddr: Error resolving '%s'\n", s);	
	}

	g_free(node);

	return true;
}

// fill up the network address from the socket
static void Net_SockaddrToNetAddr(struct sockaddr *s, net_addr_t *a) {

	if (s->sa_family == AF_INET) {
                a->type = NA_IPV4;
                *(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
                a->port = ((struct sockaddr_in *)s)->sin_port;
        }
        else if(s->sa_family == AF_INET6)
        {
                a->type = NA_IPV6;
                memcpy(a->ip6, &((struct sockaddr_in6 *)s)->sin6_addr, sizeof(a->ip6));
                a->port = ((struct sockaddr_in6 *)s)->sin6_port;
                a->scope_id = ((struct sockaddr_in6 *)s)->sin6_scope_id;
        }
}

/*
 * @brief Parses the hostname and port into the specified net_addr_t.
 */
_Bool Net_StringToNetaddr(const char *s, net_addr_t *a, net_addr_type_t type) {
	//struct sockaddr_in saddr;
	struct sockaddr_storage saddr;
	sa_family_t family;

	switch (type) {
		case NA_IPV4:
			family = AF_INET;
			break;
		case NA_IPV6:
			family = AF_INET6;
			break;
		default:
			family = AF_UNSPEC;
	}

	if (!Net_StringToSockaddr(s, (struct sockaddr *)&saddr), sizeof(saddr), family)
		return false;

	Net_SockaddrToNetAddr((struct sockaddr *)&saddr, a);

	return true;
}

/*
 * @brief Creates and binds a new network socket for the specified protocol.
 */
int32_t Net_Socket(net_addr_type_t type, const char *iface, in_port_t port) {
	int32_t sock, i = 1;

	switch (type) {
		case NA_BROADCAST:
		case NA_DATAGRAM:
			if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
				Com_Error(ERR_DROP, "socket: %s\n", Net_GetErrorString());
			}

			if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const void *) &i, sizeof(i)) == -1) {
				Com_Error(ERR_DROP, "setsockopt: %s\n", Net_GetErrorString());
			}
			break;

		case NA_STREAM:
			if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
				Com_Error(ERR_DROP, "socket: %s\n", Net_GetErrorString());
			}

			if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const void *) &i, sizeof(i)) == -1) {
				Com_Error(ERR_DROP, "setsockopt: %s\n", Net_GetErrorString());
			}
			break;

		default:
			Com_Error(ERR_DROP, "Invalid socket type: %d", type);
	}

	// make all sockets non-blocking
	if (ioctl(sock, FIONBIO, (void *) &i) == -1) {
		Com_Error(ERR_DROP, "ioctl: %s\n", Net_GetErrorString());
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	if (iface) {
		Net_StringToSockaddr(iface, &addr);
	} else {
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
	}

	addr.sin_port = htons(port);

	if (bind(sock, (void *) &addr, sizeof(addr)) == -1) {
		Com_Error(ERR_DROP, "bind: %s\n", Net_GetErrorString());
	}

	return sock;
}

/*
 * @brief
 */
void Net_CloseSocket(int32_t sock) {
#if defined(_WIN32)
	closesocket(sock);
#else
	close(sock);
#endif
}

/*
 * @brief
 */
void Net_Init(void) {

#if defined(_WIN32)
	WORD v;
	WSADATA d;

	v = MAKEWORD(2, 2);
	WSAStartup(v, &d);
#endif

	net_lo = inet_addr("127.0.0.1");
}

/*
 * @brief
 */
void Net_Shutdown(void) {

#if defined(_WIN32)
	WSACleanup();
#endif

}
