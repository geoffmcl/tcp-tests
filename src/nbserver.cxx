// nbserver.cxx
// 2011-03-31 - Some (messy) experiments with SOCKETS
// In this case setting NON-BLOCKING, and using select()
// ==================================================
//
// Written by Geoff R. McLane, started March 2011.
//
// Copyright (C) 2011 - ????  Geoff R. McLane  - http://geoffair.org
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id$

/* BASED ON -
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

#include "config.h"
#if (defined(_MSC_VER) && !defined(_CONFIG_H_MSVC_))
#error "ERROR: Copy config.h-msvc to config.h for Windows compile!"
#endif

#ifdef _MSC_VER
#include <stdio.h> // stderr, ...
#include <time.h>  // time()
#endif /* #ifdef _MSC_VER */

#include <ctype.h>
#ifndef _MSC_VER
#include <sys/time.h>
#endif
#include <fcntl.h>

#include "winsockerr.cxx"
#include "sockhelp.h"

#define MY_MX_CONNS 5
#define MY_TIMEOUT 0.5    /* in seconds */

#include "sockhelp.c" /* add helper services */

#define GET_CLIENT_INFO /* get and keep client info on 'accept()' */
#undef KEEP_CLIENT_DATA /* keep the incoming data - NOT COMPLETED!!! */

/* structure to hold the client connection information */
struct connect_info {
	SOCKET fd;			/* Socket file descriptor */
#ifdef GET_CLIENT_INFO
    socklen_t cli_len;
    int port;
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
static double time_out = (double)MY_TIMEOUT;
static struct timeval timeout;  /* Timeout for select */
static time_t last_time = 0;
static time_t nxt_msg_time = 20; /* show working dot each 20 seconds */
static int dot_out_count = 0; /* keep track of '.' output, and a macro to clear */
#define CLEAR_DOTS if (dot_out_count) { dot_out_count = 0; printf("\n"); last_time = 0; }
static struct in_addr * in_addr_ptr = NULL;

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
#if 0      
#ifndef _MSC_VER
		endofline = (char *)memchr(buffer_target,10,bytes_read);
		if (endofline != NULL) {
			/* We have read to end of line.  Return the
			   string, and adjust the input buffer. */
			endofline++; /* Move past LF character */
			memmove(sock_info->input_buf,endofline,bytes_read);
		}
#endif // _MSC_VER
#endif
	}
	return 0; /* Not an error, no data read, so must be closed. */
}
#endif // #ifdef KEEP_CLIENT_DATA
///////////////////////
//// dummy - seems gcc adds static functions, whether they are used or not,
//// and this is needed for static check_key_sleep(...) not used here.
int check_key() { return 0; }
////////////////////////

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
			printf("NB Server(tcp) - Connection accepted: FD=%d; Slot=%d of %d\n",
				connection,(listnum+1),MY_MX_CONNS);
#ifdef GET_CLIENT_INFO
            memcpy(&connectlist[listnum],&CLI_CI,sizeof(CLI_CI));
#endif /* GET_CLIENT_INFO */
			connectlist[listnum].fd = connection;
			connection = (SOCKET)-1; /* mark done, and set to EXIT loop */
		}
    }
	if (connection != -1) {
		/* No room left in the queue! */
		printf("NB Server(tcp) - No room left for new client.\n");
		sock_puts(connection,(char *)"Sorry, this server is too busy. Try again later!\r\n");
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

void clean_data(char * dest, char * src, int len)
{
    int i, off, c;
    off = 0;
    for (i = 0; i < len; i++) {
        c = src[i];
        if (c & 0x80) { // got high bit on
            dest[off++] = '@';
            c &= ~0x80;
        }
        if (c < ' ') {
            dest[off++] = '^';
            c += '@';
        }
        dest[off++] = c;
    }
    dest[off] = 0;
}

void deal_with_data(int listnum) /* Current item in connectlist for for loops */
{
	char buffer[MY_MX_RCV+16]; /* Buffer for socket reads, plus a bit... */
    char cleaned[MY_MX_RCV+16]; /* Buffer for socket reads, plus a bit... */
	char *cur_char;            /* Used in processing buffer to UPPER */
    int rlen, slen, i;   
    memset(buffer,0,sizeof(buffer)); /* start with all zeros */
    CLEAR_DOTS; /* ensure a new line, if any 'working' dots have been output... */
    /* sock_gets() reads data up until a CR, or at max buffer size
       and NOTE WELL any additional data will be discarded */
    rlen = sock_gets(connectlist[listnum].fd,buffer,MY_MX_RCV);
	if (rlen < 0) {
		/* Connection closed, close this end and free up entry in connectlist */
		printf("NB Server(tcp) - Connection lost: FD=%d; Slot=%d\n",
			connectlist[listnum].fd,(listnum+1));
		SCLOSE(connectlist[listnum].fd);
		connectlist[listnum].fd = 0; /* make slot available for the next */
	} else if (rlen > 0) {
		/* We got some data, so upper case it and send it back. */
        clean_data(cleaned,buffer,rlen);      
		printf("NB Server(tcp) - Received : [%s] Len %d, Slot %d",
            cleaned, rlen, (listnum+1));
#ifdef GET_CLIENT_INFO
        // printf(", port %d", (ntohs(connectlist[listnum].port) & 0xffff));
        // port is already converted to local...
        printf(", port %d", connectlist[listnum].port);
#endif /* GET_CLIENT_INFO */
        printf("\n");
		cur_char = buffer;
        i = rlen;      
		while (i--) {
			cur_char[0] = toupper(cur_char[0]);
			cur_char++;
		}
        slen = rlen;
        if (slen > 0) {
            if (buffer[slen -1] != '\n') {
                buffer[slen++] = '\n';
            }
            sock_write(connectlist[listnum].fd, buffer, slen);
            // slen = (int)strlen(buffer);
    		// sock_puts( connectlist[listnum].fd, buffer );
	       	// sock_puts( connectlist[listnum].fd, (char *)"\n");
            clean_data(cleaned,buffer,slen);
		    printf("NB Server(tcp) - Responded: [%s] Len %d\n", cleaned, slen );
        }      
	} else {
        /* ASSUME Connection closed, close this end and free up entry in connectlist */
        printf("NB Server(tcp) - Received : 0! Assume connection lost: FD=%d; Slot=%d\n",
            connectlist[listnum].fd,(listnum+1));
        SCLOSE(connectlist[listnum].fd);
        connectlist[listnum].fd = 0; /* make slot available for the next */
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

void set_time_out(void)
{
    int usecs = (int)(time_out * 1000000.0);
    timeout.tv_sec = usecs / 1000000;
    timeout.tv_usec = usecs % 1000000;
}


void show_comp(void)
{
    printf("NB Server(tcp) - compiled on %s, at %s\n", __DATE__, __TIME__);
}
void give_nb_help(char * name)
{
    set_time_out();
    printf("%s [Options]\n", name);
    printf("Options:\n");
    printf(" -?         = This help and exit 0.\n");
    printf(" -h <host>  = Host IP to use. (Def=localhost)\n");
    printf(" -p <port>  = Host port to use, (Def=%d)\n", PORTNUM);
    printf("Set up a non-blocking listen on the port, and\n");
    printf(" wait for clients sending a LF terminated string. Echo that\n");
    printf(" string, in upper case, back. Compiled for max. %d clients.\n", MY_MX_CONNS);
    printf("Uses 'select()' to wait for NEW connections on the listening\n");
    printf(" port, and service client traffic, with a timeout of %u.%06u seconds.\n",
        timeout.tv_sec,
        timeout.tv_usec);
#ifdef GOT_KEYBOARD_TEST
    printf("ESC key to exit forever loop...\n");
#else
    printf("Use Ctrl+c to abort forever loop...\n");
#endif
}

int check_keys(void)
{
#ifdef GOT_KEYBOARD_TEST
    int chr = test_for_input();
    char disp[4];
    int off = 0;
    if (chr) {
        CLEAR_DOTS;
        if (chr == 0x1b) {
            printf("NB Server(tcp) - ESC exit key...\n");
            return 1;
        } else {
            /* show an UNUSED key input */
            if (chr & 0x80) {
                disp[off++] = '@';
                chr &= ~0x80;
            }
            if (chr < ' ') {
                disp[off++] = '^';
                chr += '@';
            }
            disp[off++] = (char)chr;
            disp[off] = 0;
            printf("NB Server(tcp) - Unused keyboard [%s]...\n", disp);
        }
    }
#endif // #ifdef GOT_KEYBOARD_TEST
    return 0;
}

int run_nbserver(void)
{
    int iret = 0;
    int readsocks;
    set_time_out();
    printf("NB Server(tcp) - enter loop... select() to=%u.%06u secs... waiting...\n",
        timeout.tv_sec,
        timeout.tv_usec);
#ifdef GOT_KEYBOARD_TEST
    printf(" ESC key to exit loop...\n");
#else // !#ifdef GOT_KEYBOARD_TEST
    printf(" No keyboard. Use Ctrl+c to abort...\n");
#endif // #ifdef GOT_KEYBOARD_TEST

    /* ======================================================== */
	while (1) { /* Main server loop - forever */
		build_select_list(); /* re-build list, in case new client(s) added */
        set_time_out(); /* select() may change 'timeout' to show remaining time? */
		readsocks = select((int)highsock+1, &socks, (fd_set *) 0, (fd_set *) 0,
            &timeout);
        if (SERROR(readsocks)) {
            CLEAR_DOTS;
            PERROR("ERROR: select() FAILED!");
            iret = 1;
            break;
		}
		if (readsocks == 0) {
			/* Nothing ready to read, just show that we're alive - if it is time to do so... */
            if ((last_time == 0) || ((time(0) - last_time) > nxt_msg_time)) {
                dot_out_count++;
                printf(".");
                fflush(stdout);
                last_time = time(0); /* set for NEXT message time */
            }
        } else {
            last_time = 0; /* clear last time to get first DOT message */
            /* with a busy server - with clients - indication of select count */
			read_socks();
        }
#ifdef GOT_KEYBOARD_TEST
        if (check_keys())
            break;
#endif // #ifdef GOT_KEYBOARD_TEST
	} /* while(1) */
    /* ======================================================== */
    return iret;
}


int main (int argc, char *argv[]) 
{
    int iret = 0;
	char *ascport;  /* ASCII version of the server port */
	int port = 0;   /* The port number after conversion from ascport */
    int i, status;
	struct sockaddr_in server_address; /* bind info structure */
	int readsocks;	     /* Number of sockets ready for reading */

    show_comp();
    sock_init(); /* any socket init - needed for Windows */

	/* Obtain a file descriptor for our "listening" socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (SERROR(sock)) {
        PERROR("ERROR: socket() FAILED!");
        iret = 1;
        goto NB_Server_End;
	}
    printf("NB Server(tcp) - Got listen SOCKET 0x%X (%d)\n", sock, sock );

	/* So that we can re-bind to it without TIME_WAIT problems */
    setreuseaddr(sock);

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
                give_nb_help(argv[0]);
                goto NB_Server_End;
                break;
            case 'h':
                i++;
                if (i < argc) {
                    // get the users HOST
                    pa = argv[i];
                    in_addr_ptr = atoaddr(pa);
                    if (in_addr_ptr == NULL) {
                        i--;
                        printf("ERROR: 'h' failed to get host address from [%s]\n", pa);
                        goto Bad_Option2;
                    }
                } else {
                    i--;
                    printf("ERROR: 'h' option must be followed by host name!\n");
                    goto Bad_Option2;
                }
                break;
            case 'p':
                i++;
                if (i < argc) {
                    // get the users HOST
                    pa = argv[i];
                    /* Use function from sockhelp to convert to an int nw order */
                    port = atoport(pa,(char *)"tcp");
                    if (port < 0) {
                        printf("ERROR: 'p' failed to get port value from [%s]\n", pa);
                        goto Bad_Option2;
                    }
                } else {
                    i--;
                    printf("ERROR: 'p' option must be followed by port number!\n");
                    goto Bad_Option;
                }
                break;
            default:
                goto Bad_Option;
                break;
            }
        } else {
Bad_Option:
            printf("ERROR: Unknown option! [%s] Try -?\n", argv[i]);
Bad_Option2:
            iret = 1;
            goto NB_Server_End;
        }
    }
    if (port == 0)
        port = htons(PORTNUM);

	memset((char *) &server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
    if (in_addr_ptr) /* do we have a USER host */
    	server_address.sin_addr.s_addr = in_addr_ptr->s_addr;
    else
    	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = port; /* port, already converted to network order */

    printf("NB Server(tcp) - Bind IP %s, port %d\n",
        (in_addr_ptr ? inet_ntoa(*in_addr_ptr) : "0.0.0.0 (INADDR_ANY)"),
        ntohs(server_address.sin_port) );

	status = bind(sock, (struct sockaddr *) &server_address, sizeof(server_address));
    if (SERROR(status)) {
        PERROR("ERROR: bind() FAILED!");
        iret = 1;
        goto NB_Server_End;
	}

	/* Set up queue for incoming connections. */
	status = listen(sock,5);
    if (SERROR(status)) {
        PERROR("ERROR: listen() FAILED!");
        iret = 1;
        goto NB_Server_End;
    }

	/* Since we start with only one socket, the listening socket,
	   it is the highest socket so far. Not used in Windows! */
	highsock = sock;
	memset((char *) &connectlist, 0, sizeof(connectlist));

    iret = run_nbserver();

NB_Server_End:
    if (!SERROR(sock)) {
        printf("NB Server(tcp) - Closing socket %d\n", sock);
        SCLOSE(sock);
    }
#ifdef _MSC_VER
    printf("NB Server(tcp) - Doing WSACleanup()\n");
#endif // _MSC_VER
    sock_end(); /* socket cleanup - perhaps only windows */
    printf("NB Server(tcp) - Exit %d - %s\n", iret, get_datetime_str());
    return iret;
} /* main */

/* eof - nbserver.c */
