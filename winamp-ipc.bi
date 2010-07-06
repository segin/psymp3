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

#Ifndef __WINAMP_IPC_BI__
#Define __WINAMP_IPC_BI__

'#Include Once "psymp3.bi"

#Ifdef __FB_WIN32__
#define IPC_PLAYFILE	100
#Define IPC_GETLISTPOS 125
#Define IPC_GETLISTLENGTH 124
#Define IPC_SETVOLUME 122 
#Define IPC_GETOUTPUTTIME 105
#Define IPC_GET_REPEAT 251
#Define IPC_SET_REPEAT 253
#Define IPC_GETWND 260
#Define IPC_ISPLAYING 104
#Define IPC_GETPLAYLISTFILE 211
#Define IPC_GETPLAYLISTFILEW 214
#Define IPC_GETPLAYLISTTITLE 212
#Define IPC_IS_PLAYING_VIDEO 501

#define KVIRC_WM_USER 63112
#define KVIRC_WM_USER_CHECK 13123
#define KVIRC_WM_USER_CHECK_REPLY 13124
#define KVIRC_WM_USER_GETTITLE 5000
#define KVIRC_WM_USER_GETFILE 10000
#define KVIRC_WM_USER_TRANSFER 15000

Declare Function WAIntProc StdCall Alias "WAIntProc" (hWnd As HWND, uMsg As UINT, wParam As WPARAM, lParam As LPARAM) As LRESULT
Declare Sub InitKVIrcWinampInterface Alias "InitKVIrcWinampInterface" ()

#EndIf /' __FB_WIN32__ '/

#EndIf /' __WINAMP_IPC_BI__ '/