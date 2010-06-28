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
