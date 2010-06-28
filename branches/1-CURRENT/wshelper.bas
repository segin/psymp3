/'
    $Id $

    This file is part of PsyMP3.

    PsyMP3 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    PsyMP3 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PsyMP3; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
'/


''
'' to compile: fbc wshelper.bas -lib
''


#Include Once "psymp3.bi"

#ifdef __FB_WIN32__
function hStart Alias "hStart" ( ) as integer export
	dim wsaData as WSAData
	dim verhigh as integer = 2, verlow as integer = 0
	
	if( WSAStartup( MAKEWORD( verhigh, verlow ), @wsaData ) <> 0 ) then
		return FALSE
	end if
	
	if( wsaData.wVersion <> MAKEWORD( verhigh, verlow ) ) then
		WSACleanup( )	
		return FALSE
	end if
	
	function = TRUE

end function
function hShutdown Alias "hShutdown" ( ) as integer export

	function = WSACleanup( )
	
end function

#endif
#ifdef __FB_LINUX__
'
' Linux does NOT need socket library inits, unlike Windows...
'
function hStart Alias "hStart" () as integer export
	return TRUE
end function

function hShutdown Alias "hShutdown" () as Integer export
	return hStart()
end function
#endif
function hResolve Alias "hResolve" ( byval hostname as string ) as integer export
	dim ia as in_addr
	dim hostentry as hostent ptr

	'' check if it's an ip address
	ia.S_addr = inet_addr( hostname )
	if ( ia.S_addr = INADDR_NONE ) then
		
		'' if not, assume it's a name, resolve it
		hostentry = gethostbyname( hostname )
		if ( hostentry = 0 ) then
			exit function
		end if
		
		function = *cast( integer ptr, *hostentry->h_addr_list )
		
	else
	
		'' just return the address
		function = ia.S_addr
	
	end if
	
end function

'':::::
function hOpen Alias "hOpen" ( byval proto as integer = IPPROTO_TCP ) as SOCKET export
	dim s as SOCKET
    
    s = opensocket( AF_INET, SOCK_STREAM, proto )
    if( s = NULL ) then
		return NULL
	end if
	
	function = s
	
end function

'':::::
function hConnect Alias "hConnect" ( byval s as SOCKET, byval ip as integer, byval port as integer ) as integer export
	dim sa as sockaddr_in

	sa.sin_port			= htons( port )
	sa.sin_family		= AF_INET
	sa.sin_addr.S_addr	= ip
	
	function = connect( s, cast( PSOCKADDR, @sa ), len( sa ) ) <> SOCKET_ERROR
	
end function

'':::::
function hBind Alias "hBind" ( byval s as SOCKET, byval port as integer ) as integer export
	dim sa as sockaddr_in

	sa.sin_port			= htons( port )
	sa.sin_family		= AF_INET
	sa.sin_addr.S_addr	= INADDR_ANY 
	
	function = bind( s, cast( PSOCKADDR, @sa ), len( sa ) ) <> SOCKET_ERROR
	
end function

'':::::
function hListen Alias "hListen" ( byval s as SOCKET, byval timeout as integer = SOMAXCONN ) as integer export
	
	function = listen( s, timeout ) <> SOCKET_ERROR
	
end function

'':::::
function hAccept Alias "hAccept" ( byval s as SOCKET, byval sa as sockaddr_in ptr ) as SOCKET export
	dim salen as integer 
	
	salen = len( sockaddr_in )
	function = accept( s, cast( PSOCKADDR, sa ), @salen )

end function	

'':::::
function hClose Alias "hClose" ( byval s as SOCKET ) as integer export

	shutdown( s, 2 )
	
	#ifdef __FB_WIN32__
	function = closesocket( s )
	#endif
	#if defined(__FB_LINUX__) or defined(__FB_FREEBSD__)
	function = close(s)
	#endif
	
end function

'':::::
function hSend Alias "hSend" ( byval s as SOCKET, byval buffer as zstring ptr, byval bytes as integer ) as integer export

    function = send( s, buffer, bytes, 0 )
    
end function

'':::::
function hReceive Alias "hReceive" ( byval s as SOCKET, byval buffer as zstring ptr, byval bytes as integer ) as integer export

    function = recv( s, buffer, bytes, 0 )
    
end function

'':::::
function hIp2Addr Alias "hIp2Addr" ( byval ip as integer ) as zstring ptr export
	dim ia as in_addr
	
	ia.S_addr = ip
	
	function = inet_ntoa( ia )

end function

#define CLIENTADDR(c) *hIp2Addr( c.sin_addr.S_addr ) & "(" & c.sin_addr & ")"


