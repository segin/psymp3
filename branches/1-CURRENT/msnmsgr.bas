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

#Include Once "psymp3.bi"

#Ifdef __FB_WIN32__
Sub ClearWMP Alias "ClearWMP" () Export
	Dim cpd As CopyData
	Dim wmsg As WString * 500
	Dim msgr As HWND
	Dim Found As Integer
	wmsg = "PsyMP3\0Music\00\0PsyMP3: Not playing\0\0\0\0\0"
	cpd.dwData = 1351
	cpd.cbData = (Len(wmsg) * 2) + 2 
	cpd.lpData = @wmsg
	Do
		msgr = FindWindowEx(HWND_MESSAGE, msgr,"MsnMsgrUIManager", 0)
		If msgr = 0 Then
			Do
				msgr = FindWindowEx(0, msgr,"MsnMsgrUIManager", 0)
				If msgr = 0 Then
					If Found = 0 Then Printf(!"Cannot find messenger for announce!\n")
					Exit Do, Do
				EndIf
				SendMessage(msgr, WM_COPYDATA, 0, @cpd)
				Found = 1
			Loop
		EndIf
		SendMessage(msgr, WM_COPYDATA, 0, @cpd)
		Found = 1
	Loop
End Sub

Sub AnnounceWMP Alias "AnnounceWMP" (artist As String, Title As String, Album As String) Export
	Dim cpd As CopyData
	Dim msgr As HWND
	Dim tmp As String
	Dim Found As Integer
	Dim wmsg As WString * 500
	Dim As WString * 30 WMContentID = "WMContentID"
	Dim As WString * 500 MSNMusicString = !"PsyMP3\\0Music\\0%d\\0%s\\0%s\\0%s\\0%s\\0%s\\0"
	Dim As WString * 100 FormatStr = "PsyMP3: {1} - {0}"
	Dim As WString * 500 WTitle, WArtist, WAlbum
	wsprintfW(wmsg, MSNMusicString, 1, FormatStr, mp3nameW, mp3artistW, mp3albumW, WMContentID) 
	cpd.dwData = 1351
	cpd.cbData = (Len(wmsg) * 2) + 2 
	cpd.lpData = @wmsg
	Do
		msgr = FindWindowEx(HWND_MESSAGE, msgr, "MsnMsgrUIManager", 0)
		If msgr = 0 Then
			Do
				msgr = FindWindowEx(0, msgr,"MsnMsgrUIManager", 0)
				If msgr = 0 Then
					If Found = 0 Then Printf(!"Cannot find messenger for announce!\n")
					Exit Do, Do
				EndIf
				SendMessage(msgr, WM_COPYDATA, 0, @cpd)
				Found = 1
			Loop
		EndIf
		SendMessage(msgr, WM_COPYDATA, 0, @cpd)
		Found = 1
	Loop
End Sub
#EndIf /' __FB_WIN32__ '/
