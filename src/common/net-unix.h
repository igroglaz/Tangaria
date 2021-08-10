#ifdef UNIX
/*
 * File: net-unix.h
 * Purpose: Network module (Linux)
 */

#ifndef _SOCKLIB_INCLUDED
#define _SOCKLIB_INCLUDED

/* Error values and their meanings */
#define SL_ESOCKET		0	/* socket system call error */
#define SL_EBIND		1	/* bind system call error */
#define SL_ELISTEN		2	/* listen system call error */
#define SL_EHOSTNAME		3	/* Invalid host name format */
#define SL_ECONNECT		5	/* connect system call error */
#define SL_ESHUTD		6	/* shutdown system call error */
#define SL_ECLOSE		7	/* close system call error */
#define SL_EWRONGHOST		8	/* message arrived from unspec. host */
#define SL_ENORESP		9	/* No response */
#define SL_ERECEIVE		10	/* Receive error */

#ifndef _SOCKLIB_LIBSOURCE
#ifdef VMS
#include <in.h>			/* for sockaddr_in */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN		64
#endif
#else
#include <netinet/in.h>			/* for sockaddr_in */
#include <arpa/inet.h>			/* for in_addr */
#define h_addr h_addr_list[0] /* for backward compatibility */
#ifdef UNIX_SOCKETS
#include <sys/un.h>			/* for sockaddr_un */
#endif
#include <sys/select.h>
#endif
extern int
    sl_errno,
    sl_timeout_s,
    sl_timeout_us,
    sl_default_retries,
    sl_broadcast_enabled;
#ifdef UNIX_SOCKETS
extern struct sockaddr_un
#else
extern struct sockaddr_in
#endif
    sl_dgram_lastaddr;

#endif /* _SOCKLIB_LIBSOURCE */

#ifdef __cplusplus
#ifndef __STDC__
#define __STDC__	1
#endif
#endif

#ifdef __STDC__
extern void	SetTimeout(int, int);
extern int	CreateServerSocket(int);
extern int	GetPortNum(int);
extern char	*GetSockAddr(int);
extern int	GetPeerName(int, char *, int);
extern int	CreateClientSocket(char *, int);
extern int	SocketAccept(int);
extern int	SocketLinger(int);
extern int	SetSocketReceiveBufferSize(int, int);
extern int	SetSocketSendBufferSize(int, int);
extern int	SetSocketNoDelay(int, int);
extern int	SetSocketNonBlocking(int, int);
extern int	SetSocketBroadcast(int, int);
extern int	GetSocketError(int);
extern int	SocketReadable(int);
extern int	SocketRead(int, char *, int);
extern int	SocketWrite(int, char *, int);
extern int	SocketClose(int);
extern void	SocketCloseAll(void);
extern int	CreateDgramSocket(int);
extern int	CreateDgramAddrSocket(char *, int);
extern int	DgramBind(int fd, char *dotaddr, int port);
extern int	DgramConnect(int, char *, int);
extern int	DgramSend(int, char *, int, char *, int);
extern int	DgramReceiveAny(int, char *, int);
extern int	DgramReceive(int, char *, char *, int);
extern int	DgramReply(int, char *, int);
extern int	DgramRead(int fd, char *rbuf, int size);
extern int	DgramWrite(int fd, char *wbuf, int size);
extern int	DgramSendRec(int, char *, int, char *, int, char *, int);
extern char	*DgramLastaddr(void);
extern char	*DgramLastname(void);
extern int	DgramLastport(void);
extern void	DgramClose(int);
extern void	GetLocalHostName(char *, unsigned);
extern const char *GetSocketErrorMessageAux(int error);
extern void Sleep(int time);
#else /* __STDC__ */
extern void	SetTimeout();
extern int	CreateServerSocket();
extern int	GetPortNum();
extern char	*GetSockAddr();
extern int	GetPeerName();
extern int	CreateClientSocket();
extern int	SocketAccept();
extern int	SocketLinger();
extern int	SetSocketReceiveBufferSize();
extern int	SetSocketSendBufferSize();
extern int	SetSocketNoDelay();
extern int	SetSocketNonBlocking();
extern int	SetSocketBroadcast();
extern int	GetSocketError();
extern int	SocketReadable();
extern int	SocketRead();
extern int	SocketWrite();
extern int	SocketClose();
extern int	CreateDgramSocket();
extern int	CreateDgramAddrSocket();
extern int	DgramBind();
extern int	DgramConnect();
extern int	DgramSend();
extern int	DgramReceiveAny();
extern int	DgramReceive();
extern int	DgramReply();
extern int	DgramRead();
extern int	DgramWrite();
extern int	DgramSendRec();
extern char	*DgramLastaddr();
extern char	*DgramLastname();
extern int	DgramLastport();
extern void	DgramClose();
extern void	GetLocalHostName();
extern const char *GetSocketErrorMessageAux();
extern void Sleep();
#endif /* __STDC__ */
/*
#if !defined(select) && defined(__linux__)
#define select(N, R, W, E, T)	select((N),		\
	(fd_set*)(R), (fd_set*)(W), (fd_set*)(E), (T))
#endif
*/
#endif /* _SOCKLIB_INCLUDED */
#endif /* UNIX */


