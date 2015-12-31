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
 *  update: 2011-03-31 - Ported to also compile in MS WIndows
 */

#ifdef _MSC_VER
#include <Winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#endif
#include "sockhelp.h"

#include "sockhelp.c"

int main(int argc, char * argv[])
{
  SOCKET sock;  /* Sock we will talk on */
  struct sockaddr_in client, server; /* Address information for client 
                                        and server */
  int recvd; /* Number of bytes recieved */
  int thisinc; /* How many should we increment this time? */
  int count = 0; /* Our current count */
  int structlength; /* Length of sockaddr structure */
  int port; /* The port we will talk on. */
  int status;

  printf("UDP Server - compiled on %s, at %s\n", __DATE__, __TIME__);
  sock_init();
  if (argc > 2) {
    fprintf(stderr,"Usage: udpserver [port]\n");
    // fprintf(stderr,"Need a port number to listen on...\n");
    // exit(EXIT_FAILURE);
    port = atoport(argv[1],"udp"); /* atoport is from sockhelp.c */
  } 
  else
  {
    fprintf(stderr,"UDP Server - Using default port %d\n", MY_DEF_PORT);
    port = htons(MY_DEF_PORT);

  }

  if (port < 0) {
      fprintf(stderr,"ERROR: Unable to use port %d. (%d)\n",port, ntohs(port));
      exit(EXIT_FAILURE);
  }

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (SERROR(sock)) {
      PERROR("ERROR: socket() creation FAILED!");
      sock_end();
      exit(EXIT_FAILURE);
  }

  /* Really there is no need for both a client and server structure
     here.  Both are initialized to the same values, and the server
     structure is only needed for the bind, so one would have done
     the trick.  Two were used in the hope of making the source code
     clearer to read.  You bind to the server port, accepting
     messages from anywhere.  You recvfrom a client, and get the
     client information in onother structure, so that you know who
     to repond to. */

  memset((char *) &client, 0, sizeof(client));
  client.sin_family = AF_INET;
  client.sin_addr.s_addr = htonl(INADDR_ANY);
  client.sin_port = port;

  memset((char *) &server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = port;

  structlength = sizeof(server);
  status = bind(sock, (struct sockaddr *) &server, structlength);
  if (SERROR(status)) {
      PERROR("ERROR: bind() FAILED!");
      SCLOSE(sock);
      sock_end();
      exit(EXIT_FAILURE);
  }
  printf("UDP Server - On INADDR_ANY, bound to port %d, waiting for UPD client...\n",
    ntohs(port) );

  while (1) {   /* FOREVER */
    /* Get an increment request */
    structlength = sizeof(client);
    recvd = recvfrom(sock, (char *)&thisinc, sizeof(thisinc), 0, 
      (struct sockaddr *) &client, &structlength);
    if (SERROR(recvd)) {
        PERROR("ERROR: recvfrom() FAILED");
        SCLOSE(sock);
        sock_end();
        exit(EXIT_FAILURE);
    }
    if (recvd == sizeof(thisinc)) {
      thisinc = ntohl(thisinc);
      count += thisinc;
      printf("UDP Server - Adding %d.  Count now at %d.\n",thisinc,count);
      count = htonl(count);
      /* Send back the current total */
      status = sendto(sock, (char *)&count, sizeof(count), 0, 
          (struct sockaddr *) &client, structlength);
      if (SERROR(status)) {
          PERROR("ERROR: sendto() FAILED!");
          SCLOSE(sock);
          sock_end();
          exit(EXIT_FAILURE);
      }
      count = ntohl(count);
      printf("UDP Server - Sent %d bytes... Waiting for next UDP client...\n",status);
    }
  }
}

/* eof - udpserver2.c */
