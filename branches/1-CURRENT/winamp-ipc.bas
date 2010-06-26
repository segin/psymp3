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

#Include Once "winamp-ipc.bi"

#Ifdef __FB_WIN32__

'
' This emulates the interface of Winamp
'

Function WAIntProc StdCall Alias "WAIntProc" (hWnd As HWND, uMsg As UINT, wParam As WPARAM, lParam As LPARAM) As LRESULT Export
	Static As Integer msgptr, lastcall
	Static As ZString * 4096 buf
	Static As String wintitle
	Static As WString * 4096 wmp3file
	Static As WString * 4096 wret 
	wmp3file = mp3file
	Select Case uMsg
		Case WM_COPYDATA
			' Oh, fuck me running...
			Dim cpd As CopyData Ptr = Cast(CopyData Ptr, lParam)
			If cpd->dwData <> 3031 Then
				printf(!"WAIntProc(): cpd->dwData = %d\n",cpd->dwData)
				printf(!"WAIntProc(): cpd->cbData = %d\n",cpd->cbData)
				printf(!"WAIntProc(): cpd->lpData = %d\n",cpd->lpData)
			End If
			Return DefWindowProc(hWnd, uMsg, wParam, lParam)
		Case 273 ' Remote control functions
			Select Case As Const wParam
				Case &h9c70
					doCommand = PSYMP3_PLAY_NEXT
				Case &h9c6d
					If isPaused = 1 Then doCommand = PSYMP3_PAUSE
				Case &h9c6e
					doCommand = PSYMP3_PAUSE
				Case &h9c6f
					doCommand = PSYMP3_STOP
				Case &h9c6c
					doCommand = PSYMP3_PLAY_PREV
			End Select
		Case WM_USER
			If wParam = KVIRC_WM_USER Then
				' Note: The KVIrc interface here might seem retarded and buggy.
				' It's not. The GETTITLE and GETFILE thing just sets up the buffer.
				' TRANSFER transfers the buffer (and it's up to KVIrc to pass the right
				' lParam.
				Select Case lParam 
					Case KVIRC_WM_USER_CHECK
						Return KVIRC_WM_USER_CHECK_REPLY
					Case KVIRC_WM_USER_GETTITLE
						lastcall = KVIRC_WM_USER_GETTITLE
						buf = mp3artistW + " - " + mp3nameW
						Return Len(buf)
					Case KVIRC_WM_USER_GETFILE
						lastcall = KVIRC_WM_USER_GETTITLE
						' TODO: Sanitize output
						Dim As Integer i
						buf = percent_encodeW(mp3fileW)
						Return Len(buf)
					Case KVIRC_WM_USER_TRANSFER To KVIRC_WM_USER_TRANSFER + 4096 
						msgptr = lParam - KVIRC_WM_USER_TRANSFER
						Return buf[msgptr]
				End Select
			EndIf
			Select Case lParam
				Case 0
					Return &h2000 ' Claim we are Winamp 2.000
				Case 102
					doCommand = PSYMP3_PLAY
				Case 106
					Return FSOUND_Stream_SetTime(stream, wParam)
				Case IPC_GETLISTPOS
					Return plGetPosition()
				Case IPC_GETLISTLENGTH
					Return plGetEntries()
				Case IPC_SETVOLUME
					' Do nothing for now.
					Return 255
				Case 105 ' IPC_GETOUTPUTTIME
					If IsPaused = 2 Then 
						Return 0
					End If
					Select Case wParam
						Case 0 ' Current playing time in msec.
							Return FSOUND_Stream_GetTime(stream)
						Case 1 ' Length of song in seconds.
							Return Int(FSOUND_Stream_GetLengthMs(stream)/1000)
					End Select
				Case IPC_GET_REPEAT
					Return doRepeat
				Case IPC_SET_REPEAT
					If wParam = 0 Or wParam = 1 Then doRepeat = wParam
				Case IPC_GETWND
					' We don't support this for obvious reasons.
					Return 0
				Case IPC_GETPLAYLISTFILE
					Return StrPtr(mp3file)
				Case IPC_GETPLAYLISTFILEW
					Return StrPtr(wmp3file)
				Case IPC_GETPLAYLISTTITLE
					' I don't like this, it requires I return a pointer inside
					' PsyMP3's memory space.
					wintitle = mp3artist + " - " + mp3name
					Return StrPtr(wintitle)
				Case IPC_ISPLAYING
					Select Case As Const isPaused
						Case 1
							Return 3
						Case 0
							Return 1
						Case 2
							Return 0
					End Select
				Case IPC_IS_PLAYING_VIDEO
					Return 0 ' 0 means not playing video.
				Case 2000 To 3000 ' Freeform message. We aren't really Winamp, return 0
					Return 0
				Case 3026 ' WARNING: Satanic rituals at work here.
					Dim As extendedFileInfoStructW Ptr efis = wParam
					If *efis->filename <> wmp3file Then Return 0 
					Select Case LCase(efis->metadata)
						Case "artist"
							*efis->ret = mp3artistW
						Case "title"
							*efis->ret = mp3nameW
						Case "album"
							*efis->ret = mp3albumW
					End Select
					Return efis
				Case 290
					Dim As extendedFileInfoStruct Ptr efis = wParam
					If *efis->filename <> mp3file Then Return 0 
					Select Case LCase(*efis->metadata)
						Case "artist"
							*efis->ret = mp3artist
						Case "title"
							*efis->ret = mp3name
						Case "album"
							*efis->ret = mp3album
					End Select
					Return efis
				Case Else
					Printf(!"hwnd = %#x, umsg = %d, wParam = %#x, lParam = %#x\n",hWnd, uMsg, wParam, lParam)
					Return 0
			End Select
		Case WM_GETMINMAXINFO
			' printf(!"WM_GETMINMAXINFO caught.\n")
			Return DefWindowProc(hWnd, uMsg, wParam, lParam)
		Case 12 To 14' I dunno what this is, but it's used by SetWindowText()
			Return DefWindowProc(hWnd, uMsg, wParam, lParam)
		Case Else
			Printf(!"hwnd = %#x, umsg = %d, wParam = %#x, lParam = %#x\n",hWnd, uMsg, wParam, lParam)
			Return DefWindowProc(hWnd, uMsg, wParam, lParam)
	End Select
End Function

Sub InitKVIrcWinampInterface Alias "InitKVIrcWinampInterface" () Export
	Dim As WNDCLASSEX WAClass 
	Dim As ATOM a
	Dim As Dword WINERR
   
	With WAClass 
		.cbSize = SizeOf(WNDCLASSEX)
		.style = 0
		.lpfnWndProc = @WAIntProc
		.cbClsExtra = 0
		.cbWndExtra = 0
	      .hInstance = GetModuleHandle(NULL)
	      .hIcon = 0
	      .hCursor = 0 
	      .hbrBackground = 0
	      .lpszMenuName = NULL
	      .lpszClassName = @"Winamp v1.x"
	      .hIconSm = 0
	End With
	
	a = RegisterClassEx(@WAClass)
	
	printf(!"RegisterClassEx(&WAClass) returned %#x.\n", a) 
	
	If a = 0 Then
	      WINERR = GetLastError()
	      Dim As ZString Ptr ErrMsg
	      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER Or FORMAT_MESSAGE_FROM_SYSTEM, 0, WINERR, 0, @ErrMsg, 256, 0)
	      printf(!"Error %#x: %s\n", WINERR, ErrMsg)
	EndIf
	
	WAWindow = CreateWindowEx(0, "Winamp v1.x","PsyMP3 Winamp Interface Window - Winamp", _
				0, 0, 0, 0, 0, 0, 0, GetModuleHandle(NULL), 0)
				
	printf(!"CreateWindowEx(...) returned %#x.\n", WAWindow)
	printf(!"FindWindow(\"Winamp v1.x\",NULL) returned %#x.\n", FindWindow("Winamp v1.x",NULL))
	
End Sub

#EndIf /' __FB_WIN32__ '/
