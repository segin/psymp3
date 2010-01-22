
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

declare function hStart		Alias "hStart"    ( ) as integer
declare function hShutdown	Alias "hShutdown" ( ) as integer
declare function hResolve	Alias "hResolve"  ( byval hostname as string ) as integer
declare function hOpen		Alias "hOpen"     ( byval proto as integer = IPPROTO_TCP ) as SOCKET
declare function hConnect	Alias "hConnect"  ( byval s as SOCKET, byval ip as integer, byval port as integer ) as integer
declare function hBind		Alias "hBind"     ( byval s as SOCKET, byval port as integer ) as integer
declare function hListen	Alias "hListen"   ( byval s as SOCKET, byval timeout as integer = SOMAXCONN ) as integer
declare function hAccept	Alias "hAccept"   ( byval s as SOCKET, byval sa as sockaddr_in ptr ) as SOCKET
declare function hClose		Alias "hClose"    ( byval s as SOCKET ) as integer
declare function hSend		Alias "hSend"     ( byval s as SOCKET, byval buffer as zstring ptr, byval bytes as integer ) as integer
declare function hReceive	Alias "hReceive"  ( byval s as SOCKET, byval buffer as zstring ptr, byval bytes as integer ) as integer
declare function hIp2Addr	Alias "hIp2Addr"  ( byval ip as integer ) as zstring ptr

#define hPrintError(e) print "error calling "; #e; ": "; WSAGetLastError( )
