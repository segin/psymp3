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

#Ifndef __TAGUTIL_BI__
#Define __TAGUTIL_BI__

'#Include Once "psymp3.bi"

Declare Function getmp3artist Alias "getmp3artist" (stream As FSOUND_STREAM Ptr) As String
Declare Function getmp3name Alias "getmp3name" (stream As FSOUND_STREAM Ptr) As String
Declare Function getmp3artistW Alias "getmp3artistW" (stream As FSOUND_STREAM Ptr) As WString Ptr
Declare Function getmp3nameW Alias "getmp3nameW" (stream As FSOUND_STREAM Ptr) As WString Ptr
Declare Function getmp3album Alias "getmp3album" ( byval stream as FSOUND_STREAM ptr ) as String
Declare Function getmp3albumW Alias "getmp3albumW" ( byval stream as FSOUND_STREAM ptr ) as WString Ptr
Declare Function getmp3albumart Alias "getmp3albumart" ( byval stream as FSOUND_STREAM ptr ) as Any Ptr

#EndIf /' __TAGUTIL_BI__ '/