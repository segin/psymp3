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

#Include "ui.bi"

#ifdef __FB_WIN32__
function file_getname Alias "file_getname" ( byval hWnd as HWND ) as String Export

	dim ofn as OPENFILENAME
	dim filename as zstring * MAX_PATH+1
	
	with ofn
		.lStructSize 		= sizeof( OPENFILENAME )
		.hwndOwner	 		= hWnd
		.hInstance	 		= GetModuleHandle( NULL )
		.lpstrFilter 		= strptr( _ 
				!"MPEG Layer-3 Audio/MP3 File, (*.mp3)\0*.mp3\0" _
				!"Ogg Vorbis (*.ogg)\0*.ogg\0" _
				!"Windows Media Audio (*.asf; *.wma)\0*.wma;*.asf\0" _
				!"Free Lossless Audio Codec/FLAC (*.flac)\0*.flac\0" _
				!"M3U Playlist (*.m3u; *.m3u8)\0*.m3u;*.m3u8\0" _
				!"All Files, (*.*)\0*.*\0\0" _
			)
		.lpstrCustomFilter 	= NULL
		.nMaxCustFilter 	= 0
		.nFilterIndex 		= 1
		.lpstrFile			= @filename
		.nMaxFile			= sizeof( filename )
		.lpstrFileTitle		= NULL
		.nMaxFileTitle		= 0
		.lpstrInitialDir	= NULL
		.lpstrTitle			= @"PsyMP3 - Select an mp3, Windows Media Audio, FLAC, or Ogg Vorbis file..."
		.Flags				= OFN_EXPLORER or OFN_FILEMUSTEXIST or OFN_PATHMUSTEXIST
		.nFileOffset		= 0
		.nFileExtension		= 0
		.lpstrDefExt		= NULL
		.lCustData			= 0
		'.lpfnHook		= NULL
		.lpTemplateName		= NULL
	end With
	
	
	If( GetOpenFileName( @ofn ) = FALSE ) then
		return ""
	else
		return filename
	end If
	
end function

sub MsgBox Alias "MsgBox" (hWnd As HWND, msg As String) Export
	MessageBox(hWnd, msg, "PsyMP3 Player (pid: " & getpid() & ")", 0)
End Sub
#else 
sub TTY_MsgBox Alias "TTY_MsgBox" (hWnd As Integer, msg As String) Export
	' simply print to the console until I find a method of creating 
	' a X11 window which is modal to it's parent.
	Dim console As Integer
	console = FreeFile()
	Open Err As #console
	Print #console, "PsyMP3 [" & getpid() & "] (XID: 0x" + Hex(hWnd, 8) + "): " + msg
End Sub

Function file_getname Alias "file_getname" (ByVal hWnd as integer) As String Export
	Dim ret As String
	Dim path As String
	ret = *getFile()
	Function = ret
	path = *dirname(ret)
	chdir(path)
End Function

#EndIf

Sub DrawSpectrum Alias "DrawSpectrum" (spectrum As Single Ptr) Export
	Dim X As Integer
	For X = 0 to 320
		line(x*2,350-(spectrum[x]*350))-(x*2+1,350), _
			iif(x>200,rgb((x-200)*2.15,0,255),iif(x<100, _
			rgb(128,255,x*2.55),rgb(128-((x-100)*1.28),255 _
			 - ((x-100)*2.55),255))), bf
	Next X
End Sub

Sub ShowAbout Alias "ShowAbout" (ByVal T As Any Ptr) Export
	MsgBox(hWnd, !"This is PsyMP3 version " & PSYMP3_VERSION & !"\n\n" _
		!"PsyMP3 is free software.  You may redistribute and/or modify it under the terms of\n" & _
		!"the GNU General Public License <http://www.gnu.org/licenses/gpl-2.0.html>,\n" & _
		!"either version 2, or at your option, any later version.\n\n" & _
		!"PsyMP3 is distributed in the hope that it will be useful, but WITHOUT\n" & _
		!"ANY WARRANTY, not even the implied warranty of MERCHANTABILITY or\n" & _
		!"FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License" & _
		!" for details.\n\n" & _
		!"You should have recieved a copy of the GNU General Public License\n" & _
		!"along with PsyMP3; if not, write to the Free Software Foundation, Inc.,\n" & _
		!"51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.\n\n" & _
		!"For those that do not understand, PsyMP3 is free as in speech;\n" & _
		!"in most commercial programs, you can only use the software for certain\n" & _
		!"uses which the author approves of, and any other use instantly makes it\n" & _
		!"illegal to use the software, and it is also illegal to try to take\n" & _ 
		!"the software to see how it works or change it. In contrast, PsyMP3\n" & _
		!"has no limitations on use, and full source code is provided for anyone\n" & _
		!"to study, copy, and modify, and in return distribute the modified versions\n" & _
		!"(only, of course, if these same rules also apply to their versions)\n\n" & _ 
		!"Written by Kirn Gill <segin2005@gmail.com>\n\n" & _ 
		!"Dedicated to my beautiful girlfriend Virginia Wood.")
End Sub
