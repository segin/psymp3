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

#Ifndef __UI_BI__
#Define __UI_BI__

#Include Once "psymp3.bi"

#ifdef __FB_WIN32__
Declare Function file_getname Alias "file_getname" ( byval hWnd as HWND ) as String
Declare sub MsgBox(hWnd As HWND, msg As String)
#Else 
Declare sub TTY_MsgBox Alias "TTY_MsgBox" (hWnd As Integer, msg As String)
#inclib "ui"
Extern "C++"
	Declare Function getFile Alias "getFile" () As ZString Ptr
	Declare Sub libui_init Alias "libui_init" (ByVal argc As Integer, ByVal argv As Zstring Ptr Ptr)
	Declare Sub MsgBox Alias "messageBox" (ByVal hWnd as Integer, ByVal msg As ZString Ptr) 
End Extern
Declare Function file_getname Alias "file_getname" (ByVal hWnd as integer) As String
#EndIf /' __FB_WIN32__ '/
Declare Sub DrawSpectrum Alias "DrawSpectrum" (spectrum As Single Ptr)
Declare Sub ShowAbout Alias "ShowAbout" (ByVal T As Any Ptr)

#EndIf /' __UI_BI__ '/