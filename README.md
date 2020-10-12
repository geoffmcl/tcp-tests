# tcp-tests projects - 20170122 20151231

This is just a collection of sockets code, tcp and udp, most cross-platform, with no particular purpose other than serve as a tcp/ip example code source, gathered from many sources over the years.

## Build

This is a [CMake](https://cmake.org/) project to generate cross-port build environments like `Unix Makefiles`, `MinGW Makefiles`, `MSVC`, etc...

#### Unix
```
$ cd build
$ cmake .. [Options]
$ make
```

#### Windows

```
$ cd build
$ cmake .. [Options]
$ cmake --build . --config Release
```

Of course the options include setting the generator, if you do not want the default environment... And there is a CMake GUI that can be used instead... and like wise various GUI IDE type builders...

## Projects

They are more or less in alphabetic order... and most work to show something...

#### Crossported WIN32/UNIX

    - ebsocket.c ebsocket.h
    - nbclient.cxx
    - nbserver.cxx
    - simp_client.cxx
    - simp_server.cxx simp_common.hxx
    - udpclient.cxx
    - udpclient2.cxx
    - udp-recv.cxx
    - udp-send.cxx
    - udpserver.cxx
    - udpserver2.cxx
    - unix_client.cxx
    - unix_server.cxx
    
#### Windows Only

    - getlocalip.cpp
    - udp-wrecv.cxx
    - udp-wsend.cxx
    - recvfrom.cxx
    - sendto.cxx
    - WSAenum.cxx

#### Not working

    - httpget.cxx - not working!
    
### More Details
    
##### nbclient/nbserver

A tcp server/client pair, using 'select' in a non-blocking mode

A -? command for help.

##### recvfrom/sendto

A udp receive from and send to pair. Default recvfrom is INADDR_ANY on port 3333, 
and sendto is localhost on port 3333

A -? command for help.

##### simp_client/simp_server

A simple tcp server/client pair.

```
server def is none, port 5050
	sockfd = socket(AF_INET,socket_type,0); // SOCK_STREAM = tcp
	if (SERROR(sockfd)) {
        	PERROR("ERROR: socket() FAILED!");
	...
	status = bind(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	if (SERROR(status)) {
        	PERROR("ERROR: Can't bind address!");
	...
	status = listen(sockfd,DEF_BACKLOG);
    	if (SERROR(status)) {
        	PERROR("ERROR: listen() FAILED!");
	...
        status = select( (int)highest+1, &ListenSocket, NULL, NULL, &timeout );
        if (SERROR(status)) {
            PERROR("ERROR: 'select' FAILED!");
	... If ListenSocket ...
		newsockfd = accept(sockfd,(struct sockaddr *)&cli_addr,&clilen);
		int res = recv(newsockfd,rd_buf,MAXSTR,0);
    		if (SERROR(res)) {
	... done for a list of tcp client

client def is IP 127.0.0.1, port 5050
	sockfd = (int)socket(AF_INET,socket_type,0);
	if (SERROR(sockfd)) {
	...
        status = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
        if (SERROR(status)) {
	... if connected ...
        wlen = send(sockfd,m_data,slen,0);
        if (SERROR(wlen)) {
```

Note use of MACROS to switch between UNIX/WIN32 implmentations.

##### udpclient udpclient2 udpserver udpserver2

Experiments in udp server and client

##### unix_client unix_server

Some unix code scraped from www, but ported it to windows

2020/10/12: Add port and timeout to server...

```
$ ./unix_server -?
unix_server: usage: [options]
Options:
 --help  (-h or -?) = This help and exit(0)
 --port <port)  (p) = Set server port. (def=3333)
 --time <mins>  (t) = Set listen timeout. (def=6 mins)

 Set up a server listen on INADDR_ANY:port, and accept incoming
 connections, read, and echo messages, for the timeout... simple...
```

and IP and port to client...

```
D:\UTILS\tcp-tests\build.x64>release\unix_client -?
unix_client: usage: [options]
Options:
 --help  (-h or -?) = This help and exit(0)
 --ip <address> (i) = Set server ip address. (def=127.0.0.1)
 --port <port) (-p) = Set coomuniation port. (def=3333)

 Create a 'socket', connect to server on IP:port, and offer to send
 messages to the server... simple...
```

##### httpget.exe

This was to do a simple GET to http://crossfeed.fgx.ch, and shows the json received, but FAILS.

## History

20201012 - small enhancements to unix server, and client.

20151231 - re-named tcp-tests - all SG links removed!

20141118 - 20131015 - 20120705 - fgio root CMakeLists.txt - last fgio_11.zip...

Have FUN!

Geoff.  
20201012 - 20170122

; eof
