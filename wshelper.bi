
#ifdef __FB_WIN32__
#include once "win/winsock2.bi"
#else
#include once "crt/netinet/in.bi"
#include once "crt/arpa/inet.bi"
#include once "crt/netdb.bi"
#include once "crt/sys/socket.bi"
#include once "crt/errno.bi"
#define TRUE	1
#define FALSE	0
#endif

' #inclib "wshelper"

declare function hStart		( ) as integer
declare function hShutdown	( ) as integer
declare function hResolve	( byval hostname as string ) as integer
declare function hOpen		( byval proto as integer = IPPROTO_TCP ) as SOCKET
declare function hConnect	( byval s as SOCKET, byval ip as integer, byval port as integer ) as integer
declare function hBind		( byval s as SOCKET, byval port as integer ) as integer
declare function hListen	( byval s as SOCKET, byval timeout as integer = SOMAXCONN ) as integer
declare function hAccept	( byval s as SOCKET, byval sa as sockaddr_in ptr ) as SOCKET
declare function hClose		( byval s as SOCKET ) as integer
declare function hSend		( byval s as SOCKET, byval buffer as zstring ptr, byval bytes as integer ) as integer
declare function hReceive	( byval s as SOCKET, byval buffer as zstring ptr, byval bytes as integer ) as integer
declare function hIp2Addr	( byval ip as integer ) as zstring ptr

#define hPrintError(e) print "error calling "; #e; ": "; WSAGetLastError( )
