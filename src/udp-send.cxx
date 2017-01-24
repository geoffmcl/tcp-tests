/*\
 * udp-send.cxx
 *
 * Copyright (c) 2015 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
\*/

#include <stdio.h>
#include <string.h> // for strdup(), ...
// other includes

#include "winsockerr.cxx"

static const char *module = "udp-send";

static const char *usr_input = 0;

void give_help( char *name )
{
    printf("%s: usage: [options] usr_input\n", module);
    printf("Options:\n");
    printf(" --help  (-h or -?) = This help and exit(2)\n");
    // TODO: More help
}

int parse_args( int argc, char **argv )
{
    int i,i2,c;
    char *arg, *sarg;
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
            // TODO: Other arguments
            default:
                printf("%s: Unknown argument '%s'. Try -? for help...\n", module, arg);
                return 1;
            }
        } else {
            // bear argument
            if (usr_input) {
                printf("%s: Already have input '%s'! What is this '%s'?\n", module, usr_input, arg );
                return 1;
            }
            usr_input = strdup(arg);
        }
    }
    //if (!usr_input) {
    //    printf("%s: No user input found in command!\n", module);
    //    return 1;
    //}
    printf("%s: App not completed! Is a WIP only!\n", module);
    return 1;
}

int do_test()
{
    int iret = 1;
    SOCKET sd = 0;

    /* The socket() function returns a socket */
    /* get a socket descriptor */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (SERROR(sd)) {
        PERROR("udp-send: socket() error");
        iret = 1;
        goto Exit;
    }
    printf("%s: Opened socket %d - Begin %s\n", module, (int)sd, get_datetime_str());


Exit:
    if (sd && !SERROR(sd)) {
        printf("%s: Close socket %d - End %s\n", module, (int)sd, get_datetime_str());
        SCLOSE(sd);
    }
    return iret;
}

// main() OS entry
int main( int argc, char **argv )
{
    int iret = 0;
    iret = parse_args(argc,argv);
    if (iret) {
        if (iret == 2)
            iret = 0;
        return iret;
    }
    
    sock_init();
    iret = do_test(); // actions of app
    sock_end();

    return iret;
}


// eof = udp-send.cxx
