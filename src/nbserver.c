/* 
   Non blocking server demo 
   By Vic Metcalfe (vic@acm.org)
   For the unix-socket-faq

   2011-04-01: As ongoing 'discovery' of Windows (WIN32) sockets,
   port this source to windows OS, using the Winsock2 API
   Some additional items/info added through winsockerr.cxx to fill out
   Windows socket ERRORS through SERROR(a), PERROR(a), etc macros, so
   needs some other source files to be complete.

   This example was a good exercise in using NON-BLOCKING, and select()
   to handle multiple sockets...
   By Geoff R. McLane - reports@geoffair.info

*/
#ifdef _MSC_VER
#include <Winsock2.h>
#endif
#include "sockhelp.h"
#include <ctype.h>
#ifndef _MSC_VER
#include <sys/time.h>
#endif
#include <fcntl.h>

#define MY_MX_CONNS 5
#define MY_TIMEOUT 1    /* in seconds */
#include "sockhelp.c" /* add helper services */

#define GET_CLIENT_INFO /* get and keep client info on 'accept()' */
#undef KEEP_CLIENT_DATA /* keep the incoming data - NOT COMPLETED!!! */

/* structure to hold the client connection information */
struct connect_info {
	SOCKET fd;			/* Socket file descriptor */
#ifdef GET_CLIENT_INFO
    int cli_len, port;
    struct sockaddr cli_addr;
    char ip[32]; /* '.' form of IP address - from inet_ntoa() */
#endif /* GET_CLIENT_INFO */
#ifdef KEEP_CLIENT_DATA
#error This was NOT completed
	char *input_buf;	/* Input buffer for reads from fd */
	char *output_buf;	/* Output buffer for writes to fd */
	size_t input_buf_sz;	/* Current size of input buffer */
	size_t output_buf_sz;	/* Current size of output buffer */
#endif /* KEEP_CLIENT_DATA */
};

static SOCKET sock = 0; /* The socket file descriptor for our "listening" socket */
static struct connect_info connectlist[MY_MX_CONNS];  /* Array of connected socket data
			so we know who we are talking to, and the state
			of our communication.  (File descriptor,
			input and output buffers) */
static fd_set socks;  /* Socket file descriptors we want to wake up for, using select() */
static SOCKET highsock = 0; /* Highest #'d file descriptor, needed for select() */
static int shown_maximum = 0; /* warning when server gets to maximum clients */

static time_t last_time = 0;
static time_t nxt_msg_time = 20; /* show working dot each 20 seconds */
static int dot_out_count = 0; /* keep track of '.' output, and a macro to clear */
#define CLEAR_DOTS if (dot_out_count) { dot_out_count = 0; printf("\n"); }

#ifdef KEEP_CLIENT_DATA
#define MY_RDB_SZ 100
/* NOT YET IMPLEMENTED!!!
 * Does read into sock_info structure.  This is like the sock_gets in
 * sockhelp.c, but it uses the buffers in the connect_info struct so that
 * the whole string up to the LF is returned all at once, even though it
 * may have actually come in from multiple reads.  If the full string can
 * not be returned into buf, then it returns -1 and errno is set to
 * EAGAIN, so you should be prepared to call this function more than
 * once to retrieve a single line of text. */
int nb_sock_gets_NOT_USED(struct connect_info *sock_info, char *buf, size_t count)
{
	char read_buffer[MY_RDB_SZ];
	int bytes_read;
	char *buffer_target;
    char *endofline;
#ifdef _MSC_VER
	bytes_read = SREAD(sock_info->fd,read_buffer,MY_RDB_SZ,0);
#else
	bytes_read = SREAD(sock_info->fd,read_buffer,read_buffer,MY_RDB_SZ);
#endif
	if (SERROR(bytes_read))
		return -1; /* Pass error up to caller */
	else if (bytes_read > 0) {
		/* Update sock_info buffer with new data */
#ifdef _MSC_VER
        if (sock_info->input_buf) {
            sock_info->input_buf = (char *)realloc(sock_info->input_buf,
                sock_info->input_buf_sz + bytes_read);
        } else {
            sock_info->input_buf = (char *)malloc(sock_info->input_buf_sz + bytes_read);
        }
#else
		remalloc(sock_info->input_buf, sock_info->input_buf_sz +
			bytes_read); /* Expand/create input buffer */
#endif
		buffer_target = sock_info->input_buf;
		buffer_target += sock_info->input_buf_sz;
		memcpy(buffer_target,read_buffer,bytes_read);
		sock_info->input_buf_sz += bytes_read;
#ifndef _MSC_VER
		endofline = (char *)memchr(buffer_target,10,bytes_read);
		if (endofline != NULL) {
			/* We have read to end of line.  Return the
			   string, and adjust the input buffer. */
			endofline++; /* Move past LF character */
			memmove(sock_info->input_buf,endofline,???);
		}
#endif // _MSC_VER
	}
	return 0; /* Not an error, no data read, so must be closed. */
}
#endif // #ifdef KEEP_CLIENT_DATA

void setnonblocking(SOCKET sock)
{
#ifdef _MSC_VER
    u_long opts;
    int status;
    opts = 1;
    status = ioctlsocket(sock, FIONBIO, (u_long *) &opts);
    if (SERROR(status)) {
        PERROR("ERROR: ioctlsocket() FAILED!");
    }
#else // !_MSC_VER
	int opts;
	opts = fcntl(sock,F_GETFL);
	if (opts < 0) {
		perror("fcntl(F_GETFL)");
		exit(EXIT_FAILURE);
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(sock,F_SETFL,opts) < 0) {
		perror("fcntl(F_SETFL)");
		exit(EXIT_FAILURE);
	}
	return;
#endif // _MSC_VER
}

void build_select_list(void)
{
	int listnum;	     /* Current item in connectlist for for loops */
	/* First put together fd_set for select(), which will
	   consist of the sock variable in case a new connection
	   is coming in, plus all the sockets we have already
	   accepted. */
	FD_ZERO(&socks);
	FD_SET(sock,&socks); /* set our listening port */
	for (listnum = 0; listnum < MY_MX_CONNS; listnum++)
    {
		if ((connectlist[listnum].fd != 0)&&(connectlist[listnum].fd != (SOCKET)-1)) {
            /* is an active client - set in the 'select()' list */
			FD_SET(connectlist[listnum].fd,&socks);
			if (connectlist[listnum].fd > highsock)
				highsock = connectlist[listnum].fd;
		}
	}
}

void handle_new_connection(void) {
	int listnum;	     /* Current item in connectlist for for loops */
	SOCKET connection; /* Socket file descriptor for incoming connections */
	/* We have a new connection coming in!  We'll
	try to find a spot for it in connectlist. */
#ifdef GET_CLIENT_INFO
    struct connect_info CLI_CI;
    struct sockaddr_in *cli_addr_in = (struct sockaddr_in *)&CLI_CI.cli_addr;
    char * IP;
    memset(&CLI_CI,0,sizeof(CLI_CI)); /* clear ALL to zero */
    CLI_CI.cli_len = sizeof(CLI_CI.cli_addr); /* set client addr size */
	connection = accept(sock, &CLI_CI.cli_addr, &CLI_CI.cli_len);
#else /* !GET_CLIENT_INFO */
	connection = accept(sock, NULL, NULL);
#endif /* GET_CLIENT_INFO y/n */
    CLEAR_DOTS;
	if (SERROR(connection)) {
        PERROR("ERROR: accept() FAILED!");
        SCLOSE(sock);
        sock_end();
		exit(EXIT_FAILURE);
	}
	setnonblocking(connection); /* set the NEW client NON-BLOCKING */
#ifdef GET_CLIENT_INFO
    IP = inet_ntoa(cli_addr_in->sin_addr);
    if (IP) {
        strcpy(&CLI_CI.ip[0],IP);
        CLI_CI.port = ntohs(cli_addr_in->sin_port);
        printf("NB Server(tcp) - Connection from IP %s, on port %d\n",
            CLI_CI.ip, CLI_CI.port );
    }
#endif /* #ifdef GET_CLIENT_INFO */

	for (listnum = 0; (listnum < MY_MX_CONNS) && (connection != (SOCKET)-1); listnum ++)
    {
		if (connectlist[listnum].fd == 0) {
			printf("NB Server(tcp) - Connection accepted: FD=%d; Slot=%d\n",
				connection,listnum);
#ifdef GET_CLIENT_INFO
            memcpy(&connectlist[listnum],&CLI_CI,sizeof(CLI_CI));
#endif /* GET_CLIENT_INFO */
			connectlist[listnum].fd = connection;
			connection = (SOCKET)-1; /* set to EXIT loop */
		}
    }
	if (connection != -1) {
		/* No room left in the queue! */
		printf("NB Server(tcp) - No room left for new client.\n");
		sock_puts(connection,"Sorry, this server is too busy. Try again later!\r\n");
        sleep(1);
		SCLOSE(connection);
    } else if ( shown_maximum == 0 ) {
    	for (listnum = 0; listnum < MY_MX_CONNS; listnum++) {
    		if (connectlist[listnum].fd == 0)
                break;
        }
        if (listnum == MY_MX_CONNS) {
            shown_maximum++;
    		printf("\nNB Server(tcp) - Have MAXIMUM %d clients. NO ROOM FOR MORE!\n", MY_MX_CONNS);
            printf("Increase the value of MY_MX_CONNS, and re-compile to support more.\n\n");
            sleep(2);
        }
    }
}

#define MY_MX_RCV 128
void deal_with_data(int listnum) /* Current item in connectlist for for loops */
{
	char buffer[MY_MX_RCV+16]; /* Buffer for socket reads, plus a bit... */
	char *cur_char;            /* Used in processing buffer to UPPER */
    memset(buffer,0,sizeof(buffer)); /* start with all zeros */
    CLEAR_DOTS; /* ensure a nwe line, if any 'working' dots have been output... */
    /* sock_gets() reads data up until a CR, or at max buffer size
       and NOTE WELL any additional data will be discarded */
	if (sock_gets(connectlist[listnum].fd,buffer,MY_MX_RCV) < 0) {
		/* Connection closed, close this end
		   and free up entry in connectlist */
		printf("NB Server - Connection lost: FD=%d; Slot=%d\n",
			connectlist[listnum],listnum);
		SCLOSE(connectlist[listnum].fd);
		connectlist[listnum].fd = 0; /* make slot available for the next */
	} else {
		/* We got some data, so upper case it and send it back. */
		printf("NB Server - Received : [%s] Slot %d",buffer,listnum);
#ifdef GET_CLIENT_INFO
        printf(", port %d", connectlist[listnum].port);
#endif /* GET_CLIENT_INFO */
        printf("\n");
		cur_char = buffer;
		while (cur_char[0] != 0) {
			cur_char[0] = toupper(cur_char[0]);
			cur_char++;
		}
		sock_puts( connectlist[listnum].fd, buffer );
		sock_puts( connectlist[listnum].fd, "\n"   );
		printf("NB Server - Responded: [%s]\n",buffer);
	}
}

void read_socks(void) {
	int listnum;	     /* Current item in connectlist for for loops */
	/* OK, now socks will be set with whatever socket(s)
	   are ready for reading.  Lets first check our
	   "listening" socket, and then check the sockets
	   in connectlist. */
	if (FD_ISSET(sock,&socks))
		handle_new_connection(); /* got a NEW client */

	/* Now check connectlist for available data */
	for (listnum = 0; listnum < MY_MX_CONNS; listnum++) {
        if (connectlist[listnum].fd && (connectlist[listnum].fd != (SOCKET)-1)) {
            if (FD_ISSET(connectlist[listnum].fd,&socks)) {
                deal_with_data(listnum);
            }
        }
	} /* for (all entries in queue) */
}

void give_nb_help(void)
{

}


int main (int argc, char *argv[]) 
{
    int iret = 0;
	char *ascport;  /* ASCII version of the server port */
	int port = 0;   /* The port number after conversion from ascport */
    int i, status;
	struct sockaddr_in server_address; /* bind info structure */
	int reuse_addr = 1;  /* Used so we can re-bind to our port
				while a previous connection is still
				in TIME_WAIT state. */
	struct timeval timeout;  /* Timeout for select */
	int readsocks;	     /* Number of sockets ready for reading */
#ifdef GOT_KEYBOARD_TEST
    int chr;
#endif // #ifdef GOT_KEYBOARD_TEST

    printf("NB Server(tcp) - compiled on %s, at %s\n", __DATE__, __TIME__);

    sock_init();
	/* Obtain a file descriptor for our "listening" socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (SERROR(sock)) {
        PERROR("ERROR: socket() FAILED!");
        sock_end();
		exit(EXIT_FAILURE);
	}

	/* So that we can re-bind to it without TIME_WAIT problems */
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr,
		sizeof(reuse_addr));

	/* Set socket to non-blocking with our setnonblocking routine */
	setnonblocking(sock);

	/* See if we were given a host address, or a port number to use */
	/* Get the address information, and bind it to the socket */
    for (i = 1; i < argc; i++)
    {
        char * pa = argv[i];
        if (*pa == '-') {
            while(*pa == '-') pa++;
            switch (*pa)
            {
            case '?':
                give_nb_help();
                goto NB_Server_End;
                break;
            case 'h':
                i++;
                if (i < argc) {
                    // get the users HOST
                    pa = argv[i];
                } else {
                    i--;
                    fprintf(stderr,"ERROR: 'h' option must be followed by host name!\n");
                    goto Bad_Option;
                }
                break;
            case 'p':
                i++;
                if (i < argc) {
                    // get the users HOST
                    ascport = argv[i];
                    /* Use function from sockhelp to convert to an int nw order */
                    port = atoport(ascport,"tcp");
                } else {
                    i--;
                    fprintf(stderr,"ERROR: 'p' option must be followed by port number!\n");
                    goto Bad_Option;
                }
                break;
            default:
                goto Bad_Option;
                break;
            }
        } else {
Bad_Option:
            fprintf(stderr,"ERROR: Unknown option! [%s] Try -?\n", argv[i]);
            iret = 1;
            goto NB_Server_End;
        }
    }
    if (port == 0)
        port = htons(PORTNUM);
    if (port < 0) {
        printf("ERROR: Could not convert to a valid PORT!\n");
        iret = 1;
        goto NB_Server_End;
    }

	memset((char *) &server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = port;

    printf("NB Server(tcp) - Using IP 0.0.0.0 (ADDR_ANY), port %d\n", ntohs(port) );
	status = bind(sock, (struct sockaddr *) &server_address,
	  sizeof(server_address));
    if (SERROR(status)) {
        PERROR("ERROR: bind() FAILED!");
		SCLOSE(sock);
        sock_end();
		exit(EXIT_FAILURE);
	}

	/* Set up queue for incoming connections. */
	status = listen(sock,5);
    if (SERROR(status)) {
        PERROR("ERROR: listen() FAILED!");
        SCLOSE(sock);
        sock_end();
        exit(EXIT_FAILURE);
    }

	/* Since we start with only one socket, the listening socket,
	   it is the highest socket so far. */
	highsock = sock;
	memset((char *) &connectlist, 0, sizeof(connectlist));

    printf("NB Server(tcp) - entering forever loop... waiting for clients...\n" );
#ifdef GOT_KEYBOARD_TEST
    printf(" ESC key to exit loop...\n");
#endif // #ifdef GOT_KEYBOARD_TEST
	while (1) { /* Main server loop - forever */
		build_select_list();
		timeout.tv_sec = MY_TIMEOUT;
		timeout.tv_usec = 0;
		readsocks = select((int)highsock+1, &socks, (fd_set *) 0, 
		  (fd_set *) 0, &timeout);
        if (SERROR(readsocks)) {
            CLEAR_DOTS;
            PERROR("ERROR: select() FAILED!");
            SCLOSE(sock);
            sock_end();
			exit(EXIT_FAILURE);
		}
		if (readsocks == 0) {
			/* Nothing ready to read, just show that
			   we're alive */
            if ((last_time == 0) || ((time(0) - last_time) > nxt_msg_time)) {
                dot_out_count++;
                printf(".");
                fflush(stdout);
                last_time = time(0); /* set for NEXT message time */
            }
        } else {
			read_socks();
        }
#ifdef GOT_KEYBOARD_TEST
        chr = test_for_input();
        if (chr) {
            CLEAR_DOTS;
            if (chr == 0x1b) {
                printf("NB Server(tcp) - ESC exit key...\n");
                break;
            }
        }
#endif // #ifdef GOT_KEYBOARD_TEST
	} /* while(1) */

NB_Server_End:

    SCLOSE(sock);
    sock_end();
    return iret;
} /* main */

/* eof - nbserver.c */
