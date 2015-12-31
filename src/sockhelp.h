/*
 *  This file is provided for use with the unix-socket-faq.  It is public
 *  domain, and may be copied freely.  There is no copyright on it.  The
 *  original work was by Vic Metcalfe (vic@brutus.tlug.org), and any
 *  modifications made to that work were made with the understanding that
 *  the finished work would be in the public domain.
 *
 *  If you have found a bug, please pass it on to me at the above address
 *  acknowledging that there will be no copyright on your work.
 *
 *  The most recent version of this file, and the unix-socket-faq can be
 *  found at http://www.interlog.com/~vic/sock-faq/.
 */

#ifndef _SOCKHELP_H_
#define _SOCKHELP_H_

#ifdef _MSC_VER
#include <Winsock2.h>
#include <stdio.h>
#else
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <limits.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

extern int atoport( char *service, char *proto );
extern struct in_addr *atoaddr( char *address );
#ifndef _MSC_VER
extern SOCKET get_connection( int socket_type, u_short port, SOCKET *listener );
#endif
extern SOCKET make_connection( char *service, int type, char *netaddress );
extern int sock_read(SOCKET sockfd, char *buf, size_t count);
extern int sock_write(SOCKET sockfd, const char *buf, size_t count);
extern int sock_gets(SOCKET sockfd, char *str, size_t count );
extern int sock_puts(SOCKET sockfd, char *str );
void ignore_pipe(void);

#define MY_DEF_PORT 5050

#endif
// eof - sockhelp.h
