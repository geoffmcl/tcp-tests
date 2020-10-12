/* --------------------------
   Simple unix server
   from : http://publib.boulder.ibm.com/infocenter/iseries/v5r3/index.jsp?topic=%2Frzab6%2Frzab6xnonblock.htm

   -------------------------- */
#ifdef _MSC_VER
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h> /* open(), close() */
#include <string.h> /* memset() */
#include <sys/time.h> /* struct timeval */
#endif

#define SPRTF printf
/* some definitions used */
#ifdef _MSC_VER
/* ========== for windows ========= */
#define SERROR(a) (a == SOCKET_ERROR)
#define SCLOSE closesocket
#define PERROR(a) win_wsa_perror(a)
#define SREAD recv
#define SWRITE send
#ifndef EINTR
#define EINTR WSAEINTR
#endif
#define ERRWLDBLK (WSAGetLastError() == WSAEWOULDBLOCK)

#define PRId64 "I64d"
#define PRIu64 "I64u"
// get a message from the system for this error value
char *get_errmsg_text( int err )
{
    LPSTR ptr = 0;
    DWORD fm = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&ptr, 0, NULL );
    if (ptr) {
        size_t len = strlen(ptr);
        while(len--) {
            if (ptr[len] > ' ') break;
            ptr[len] = 0;
        }
        if (len) return ptr;
        LocalFree(ptr);
    }
    return NULL;
}

void win_wsa_perror( char *msg )
{
    int err = WSAGetLastError();
    LPSTR ptr = get_errmsg_text(err);
    if (ptr) {
        SPRTF("%s = %s (%d)\n", msg, ptr, err);
        LocalFree(ptr);
    } else {
        SPRTF("%s %d\n", msg, err);
    }
}

/* ================================ */
#else /* !_MSC_VER */
/* =========== unix/linux ========= */
#define SERROR(a) (a < 0)
typedef int SOCKET;
#define SCLOSE close
#define PERROR(a) perror(a)
#define SREAD read
#define SWRITE write
#define ERRWLDBLK (errno == EWOULDBLOCK)

/* ================================ */
#endif /* _MSC_VER y/n */

#define DEF_SERVER_PORT  3333
#define DEF_SERVER_TO  6

#define TRUE             1
#define FALSE            0

static const char* module = "unix_server";

static unsigned short server_port = DEF_SERVER_PORT;
static unsigned int server_timeout = DEF_SERVER_TO;

void give_help(char* name)
{
    printf("%s: usage: [options]\n", module);
    printf("Options:\n");
    printf(" --help  (-h or -?) = This help and exit(0)\n");
    //printf(" --ip <address> (i) = Set server ip address. (def=%s)\n", server_ip);
    printf(" --port <port)  (p) = Set server port. (def=%u)\n", server_port);
    printf(" --time <mins>  (t) = Set listen timeout. (def=%u mins)\n", server_timeout);
    printf("\n");
    printf(" Set up a server listen on INADDR_ANY:port, and accept incoming\n");
    printf(" connections, read, and echo messages, for the timeout... simple...\n");
}

int parse_args(int argc, char** argv)
{
    int i, i2, c;
    char* arg, * sarg;
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        i2 = i + 1;
        if (*arg == '-') {
            sarg = &arg[1];
            while (*sarg == '-')
                sarg++;
            c = *sarg;
            switch (c) {
            case 'h':
            case '?':
                give_help(argv[0]);
                return 2;
                break;
            case 't':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    server_timeout = atoi(sarg);
                }
                else {
                    printf("%s: Error: Expect 'timeout' minutes to follow '%s'! Aborting...\n", module, arg);
                    return 1;
                }
                break;
            case 'p':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    server_port = atoi(sarg);
                }
                else {
                    printf("%s: Error: Expect 'port' value to follow '%s'! Aborting...\n", module, arg);
                    return 1;
                }
                break;
            default:
                printf("%s: Unknown argument '%s'. Try -? for help...\n", module, arg);
                return 1;
            }
        }
        else {
            // bear argument
            printf("%s: Error: What is this '%s'? Use -? for help.\n", module, arg);
            return 1;
        }
    }
    return 0;
}




static void net_exit ( void )
{
#ifdef _MSC_VER
	/* Clean up windows networking */
	if ( WSACleanup() == SOCKET_ERROR ) {
		if ( WSAGetLastError() == WSAEINPROGRESS ) {
			WSACancelBlockingCall();
			WSACleanup();
		}
	}
#endif
}


int net_init ()
{
#ifdef _MSC_VER
	/* Start up the windows networking */
	WORD version_wanted = MAKEWORD(2,2);
	WSADATA wsaData;

	if ( WSAStartup(version_wanted, &wsaData) != 0 ) {
		SPRTF("Couldn't initialize Winsock 2.2\n");
		return 1;
	}
#endif
    atexit( net_exit ) ;
	return 0;
}


int main (int argc, char *argv[])
{
   int    i, len, rc;
   long   on = 1;
   int    listen_sd, max_sd, new_sd;
   int    desc_ready, end_server = FALSE;
   int    close_conn;
   char   buffer[80];
   struct sockaddr_in   addr;
   struct timeval       timeout;
   //struct fd_set        master_set, working_set;
   fd_set master_set, working_set;

   rc = parse_args(argc, argv);
   if (rc) {
       if (rc == 2)
           rc = 0;
       return rc;
   }

   if (net_init())
       return 1;

   /*************************************************************/
   /* Create an AF_INET stream socket to receive incoming       */
   /* connections on                                            */
   /*************************************************************/
   listen_sd = socket(AF_INET, SOCK_STREAM, 0);
   if (listen_sd < 0)
   {
      PERROR("socket() failed");
      exit(-1);
   }

   /*************************************************************/
   /* Allow socket descriptor to be reuseable                   */
   /*************************************************************/
   rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
                   (char *)&on, sizeof(on));
   if (SERROR(rc))
   {
      PERROR("setsockopt() failed");
      SCLOSE(listen_sd);
      exit(-1);
   }

   /*************************************************************/
   /* Set socket to be non-blocking.  All of the sockets for    */
   /* the incoming connections will also be non-blocking since  */
   /* they will inherit that state from the listening socket.   */
   /*************************************************************/
#ifdef _MSC_VER
   //rc = setsockopt( listen_sd, SOL_SOCKET, SocketOption, (char*)&on, sizeof(on) );
   rc = ioctlsocket(listen_sd, FIONBIO, (u_long *)&on);
#else // !_MSC_VER
   rc = ioctl(listen_sd, FIONBIO, (char *)&on);
#endif
   if (SERROR(rc))
   {
      PERROR("ioctl() failed");
      SCLOSE(listen_sd);
      exit(-1);
   }

   /*************************************************************/
   /* Bind the socket                                           */
   /*************************************************************/
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_port        = htons(server_port);
   rc = bind(listen_sd,
             (struct sockaddr *)&addr, sizeof(addr));
   if (SERROR(rc))
   {
      PERROR("bind() failed");
      SCLOSE(listen_sd);
      exit(-1);
   }

   /*************************************************************/
   /* Set the listen back log                                   */
   /*************************************************************/
   rc = listen(listen_sd, 32);
   if (SERROR(rc))
   {
      PERROR("listen() failed");
      SCLOSE(listen_sd);
      exit(-1);
   }

   /*************************************************************/
   /* Initialize the master fd_set                              */
   /*************************************************************/
   FD_ZERO(&master_set);
   max_sd = listen_sd;
   FD_SET(listen_sd, &master_set);

   /*************************************************************/
   /* Initialize the timeval struct to 6? minutes.  If no        */
   /* activity after this program will end.                     */
   /*************************************************************/
   timeout.tv_sec  = server_timeout * 60;
   timeout.tv_usec = 0;

   // Show the server info...
   struct in_addr* pin = (struct in_addr*)&addr.sin_addr;
   char* IP = inet_ntoa(*pin);
   printf("Server: Listening on addr %s:%u, timeout %u mins...\n",
       (IP ? IP : "unk"),
       server_port,
       (timeout.tv_sec / 60));

   /*************************************************************/
   /* Loop waiting for incoming connects or for incoming data   */
   /* on any of the connected sockets.                          */
   /*************************************************************/
   do
   {
      /**********************************************************/
      /* Copy the master fd_set over to the working fd_set.     */
      /**********************************************************/
      memcpy(&working_set, &master_set, sizeof(master_set));

      /**********************************************************/
      /* Call select() and wait 5 minutes for it to complete.   */
      /**********************************************************/
      printf("Waiting on select()...\n");
      rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);

      /**********************************************************/
      /* Check to see if the select call failed.                */
      /**********************************************************/
      if (SERROR(rc))
      {
         PERROR("  select() failed");
         break;
      }

      /**********************************************************/
      /* Check to see if the 5 minute time out expired.         */
      /**********************************************************/
      if (rc == 0)
      {
         printf("  select() time out, after %u mins. End program.\n", (timeout.tv_sec / 60));
         break;
      }
      /**********************************************************/
      /* One or more descriptors are readable.  Need to         */
      /* determine which ones they are.                         */
      /**********************************************************/
      desc_ready = rc;
      for (i=0; i <= max_sd  &&  desc_ready > 0; ++i)
      {
         /*******************************************************/
         /* Check to see if this descriptor is ready            */
         /*******************************************************/
         if (FD_ISSET(i, &working_set))
         {
            /****************************************************/
            /* A descriptor was found that was readable - one   */
            /* less has to be looked for.  This is being done   */
            /* so that we can stop looking at the working set   */
            /* once we have found all of the descriptors that   */
            /* were ready.                                      */
            /****************************************************/
            desc_ready -= 1;

            /****************************************************/
            /* Check to see if this is the listening socket     */
            /****************************************************/
            if (i == listen_sd)
            {
               printf("  Listening socket is readable\n");
               /*************************************************/
               /* Accept all incoming connections that are      */
               /* queued up on the listening socket before we   */
               /* loop back and call select again.              */
               /*************************************************/
               do
               {
                  /**********************************************/
                  /* Accept each incoming connection.  If       */
                  /* accept fails with EWOULDBLOCK, then we     */
                  /* have accepted all of them.  Any other      */
                  /* failure on accept will cause us to end the */
                  /* server.                                    */
                  /**********************************************/
                  new_sd = accept(listen_sd, NULL, NULL);
                  if (SERROR(new_sd))
                  {
                     if ( !ERRWLDBLK )
                     {
                        PERROR("  accept() failed");
                        end_server = TRUE;
                     }
                     break;
                  }

                  /**********************************************/
                  /* Add the new incoming connection to the     */
                  /* master read set                            */
                  /**********************************************/
                  printf("  New incoming connection - %d\n", new_sd);
                  FD_SET(new_sd, &master_set);
                  if (new_sd > max_sd)
                     max_sd = new_sd;

                  /**********************************************/
                  /* Loop back up and accept another incoming   */
                  /* connection                                 */
                  /**********************************************/
               } while (new_sd != -1);
            }

            /****************************************************/
            /* This is not the listening socket, therefore an   */
            /* existing connection must be readable             */
            /****************************************************/
            else
            {
               printf("  Descriptor %d is readable\n", i);
               close_conn = FALSE;
               /*************************************************/
               /* Receive all incoming data on this socket      */
               /* before we loop back and call select again.    */
               /*************************************************/
               do
               {
                  /**********************************************/
                  /* Receive data on this connection until the  */
                  /* recv fails with EWOULDBLOCK.  If any other */
                  /* failure occurs, we will close the          */
                  /* connection.                                */
                  /**********************************************/
                  rc = recv(i, buffer, sizeof(buffer), 0);
                  if (SERROR(rc))
                  {
                     if ( !ERRWLDBLK )
                     {
                        PERROR("  recv() failed");
                        close_conn = TRUE;
                     }
                     break;
                  }

                  /**********************************************/
                  /* Check to see if the connection has been    */
                  /* closed by the client                       */
                  /**********************************************/
                  if (rc == 0)
                  {
                     printf("  Connection closed\n");
                     close_conn = TRUE;
                     break;
                  }

                  /**********************************************/
                  /* Data was received                          */
                  /**********************************************/
                  len = rc;
                  printf("  %d bytes received\n", len);

                  /**********************************************/
                  /* Echo the data back to the client           */
                  /**********************************************/
                  rc = send(i, buffer, len, 0);
                  if (SERROR(rc))
                  {
                     PERROR("  send() failed");
                     close_conn = TRUE;
                     break;
                  }

               } while (TRUE);

               /*************************************************/
               /* If the close_conn flag was turned on, we need */
               /* to clean up this active connection.  This     */
               /* clean up process includes removing the        */
               /* descriptor from the master set and            */
               /* determining the new maximum descriptor value  */
               /* based on the bits that are still turned on in */
               /* the master set.                               */
               /*************************************************/
               if (close_conn)
               {
                  SCLOSE(i);
                  FD_CLR(i, &master_set);
                  if (i == max_sd)
                  {
                     while (FD_ISSET(max_sd, &master_set) == FALSE)
                        max_sd -= 1;
                  }
               }
            } /* End of existing connection is readable */
         } /* End of if (FD_ISSET(i, &working_set)) */
      } /* End of loop through selectable descriptors */

   } while (end_server == FALSE);

   /*************************************************************/
   /* Cleanup all of the sockets that are open                  */
   /*************************************************************/
   for (i=0; i <= max_sd; ++i)
   {
      if (FD_ISSET(i, &master_set))
         SCLOSE(i);
   }
   return 0;
}

/* eof - unix_server.cxx */
