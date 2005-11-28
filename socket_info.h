/* $Id$
 *
 * find & manage listen addresses 
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to" the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * This file contains code that initializes and handles ser listen addresses
 * lists (struct socket_info). It is used mainly on startup.
 * 
 * History:
 * --------
 *  2003-10-22  created by andrei
 */


#ifndef socket_info_h
#define socket_info_h

#include <stdlib.h>

#include "ip_addr.h" 
#include "dprint.h"
#include "globals.h"
/* struct socket_info is defined in ip_addr.h */

struct socket_info* udp_listen;
#ifdef USE_TCP
struct socket_info* tcp_listen;
#endif
#ifdef USE_TLS
struct socket_info* tls_listen;
#endif


int add_listen_iface(char* name, unsigned short port, unsigned short proto,
							enum si_flags flags);
int fix_all_socket_lists();
void print_all_socket_lists();
void print_aliases();

struct socket_info* grep_sock_info(str* host, unsigned short port,
										unsigned short proto);
struct socket_info* find_si(struct ip_addr* ip, unsigned short port,
												unsigned short proto);

/* helper function:
 * returns next protocol, if the last one is reached return 0
 * useful for cycling on the supported protocols */
static inline int next_proto(unsigned short proto)
{
	switch(proto){
		case PROTO_NONE:
			return PROTO_UDP;
		case PROTO_UDP:
#ifdef	USE_TCP
			return (tcp_disable)?0:PROTO_TCP;
#else
			return 0;
#endif
#ifdef USE_TCP
		case PROTO_TCP:
#ifdef USE_TLS
			return (tls_disable)?0:PROTO_TLS;
#else
			return 0;
#endif
#endif
#ifdef USE_TLS
		case PROTO_TLS:
			return 0;
#endif
		default:
			LOG(L_ERR, "ERROR: next_proto: unknown proto %d\n", proto);
	}
	return 0;
}



/* gets first non-null socket_info structure
 * (useful if for. e.g we are not listening on any udp sockets )
 */
inline static struct socket_info* get_first_socket()
{
	if (udp_listen) return udp_listen;
#ifdef USE_TCP
	else if (tcp_listen) return tcp_listen;
#ifdef USE_TLS
	else if (tls_listen) return tls_listen;
#endif
#endif
	return 0;
}


/* returns -1 on error, 0 on success
 * sets proto */
inline static int parse_proto(unsigned char* s, long len, int* proto)
{
#define PROTO2UINT(a, b, c) ((	(((unsigned int)(a))<<16)+ \
								(((unsigned int)(b))<<8)+  \
								((unsigned int)(c)) ) | 0x20202020)
	unsigned int i;
	if (len!=3) return -1;
	i=PROTO2UINT(s[0], s[1], s[2]);
	switch(i){
		case PROTO2UINT('u', 'd', 'p'):
			*proto=PROTO_UDP;
			break;
#ifdef USE_TCP
		case PROTO2UINT('t', 'c', 'p'):
			*proto=PROTO_TCP;
			break;
#ifdef USE_TLS
		case PROTO2UINT('t', 'l', 's'):
			*proto=PROTO_TLS;
			break;
#endif
#endif
		default:
			return -1;
	}
	return 0;
}



/*
 * parses [proto:]host[:port]
 * where proto= udp|tcp|tls
 * returns 0 on success and -1 on failure
 */
inline static int parse_phostport(char* s, char** host, int* hlen, 
													int* port, int* proto)
{
	char* first; /* first ':' occurrence */
	char* second; /* second ':' occurrence */
	char* p;
	int bracket;
	char* tmp;
	
	first=second=0;
	bracket=0;
	
	/* find the first 2 ':', ignoring possible ipv6 addresses
	 * (substrings between [])
	 */
	for(p=s; *p; p++){
		switch(*p){
			case '[':
				bracket++;
				if (bracket>1) goto error_brackets;
				break;
			case ']':
				bracket--;
				if (bracket<0) goto error_brackets;
				break;
			case ':':
				if (bracket==0){
					if (first==0) first=p;
					else if( second==0) second=p;
					else goto error_colons;
				}
				break;
		}
	}
	if (p==s) return -1;
	if (*(p-1)==':') goto error_colons;
	
	if (first==0){ /* no ':' => only host */
		*host=s;
		*hlen=(int)(p-s);
		*port=0;
		*proto=0;
		return 0;
	}
	if (second){ /* 2 ':' found => check if valid */
		if (parse_proto((unsigned char*)s, first-s, proto)<0)
			goto error_proto;
		*port=strtol(second+1, &tmp, 10);
		if ((tmp==0)||(*tmp)||(tmp==second+1)) goto error_port;
		*host=first+1;
		*hlen=(int)(second-*host);
		return 0;
	}
	/* only 1 ':' found => it's either proto:host or host:port */
	*port=strtol(first+1, &tmp, 10);
	if ((tmp==0)||(*tmp)||(tmp==first+1)){
		/* invalid port => it's proto:host */
		if (parse_proto((unsigned char*)s, first-s, proto)<0) goto error_proto;
		*port=0;
		*host=first+1;
		*hlen=(int)(p-*host);
	}else{
		/* valid port => its host:port */
		*proto=0;
		*host=s;
		*hlen=(int)(first-*host);
	}
	return 0;
error_brackets:
	LOG(L_ERR, "ERROR: parse_phostport: too many brackets in %s\n", s);
	return -1;
error_colons:
	LOG(L_ERR, "ERROR: parse_phostport: too many colons in %s\n", s);
	return -1;
error_proto:
	LOG(L_ERR, "ERROR: parse_phostport: bad protocol in %s\n", s);
	return -1;
error_port:
	LOG(L_ERR, "ERROR: parse_phostport: bad port number in %s\n", s);
	return -1;
}





#endif
