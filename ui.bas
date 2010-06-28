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

#Include "psymp3.bi"

#Ifdef __FB_WIN32__
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

Dim Shared As Double max_amp(0 To 2047)

sub draw_scope _
	( _
		byval n        as integer, _        /' number of samples '/
		byval in_data  as complex_t ptr, _  /' sample data '/
		byval col      as uinteger _        /' colour '/
	) Export

	dim as integer sw        = yetiplay.screen_width
	dim as integer sh        = yetiplay.screen_height
	dim as double  wid_scale = sw / n
	dim as integer sh2       = sh \ 2
	dim as integer sh4       = sh \ 4

	pset( 0, sh2 )
	for x as integer = 0 to sw - 1
		dim as integer i = int(x / wid_scale)
		line -( x, sh2 - (in_data[i].re * sh4) ), col
	next x

end sub

sub draw_fft _
	( _
		byval n        as integer, _        /' number of samples '/
		byval out_data as complex_t ptr, _  /' sample data after fft '/
		byval max_freq as integer, _        /' display 0 - max_freq '/
		byval col      as uinteger, _       /' colour '/
		byval sp       as any ptr, _        /' screen ptr '/
		byval pitch    as integer _         /' screen pitch '/
	) Export 

	if sp = 0 then exit sub

	dim as integer sw         = yetiplay.screen_width
	dim as integer sh         = yetiplay.screen_height
	dim as double  wid_scale  = sw / n
	dim as integer sh2        = sh \ 2
	dim as double  freq_scale = 44100 / max_freq
	dim as integer end_y      = sh - 10

	for x as integer = 0 to sw - 1
		dim as integer i           = int((x / wid_scale) / freq_scale)
		dim as double  dist        = sqrt( sqrt( (out_data[i].re^2) + (out_data[i].im^2) ) )
		dim as integer line_height = 0

		line_height = (dist / max_amp(i)) * sh2

		if (line_height <= sh2) and (line_height > 0) then
			dim as integer      start_y = end_y - line_height
			dim as any ptr      p       = sp + (pitch * start_y)

			for y as integer = 0 to line_height - 1
				cast( uinteger ptr, p )[x] = col
				p += pitch
			next y

		end if

	next x

end sub

sub draw_notes _
	( _
		byval n        as integer, _        /' number of samples '/
		byval out_data as complex_t ptr, _  /' sample data after fft '/
		byval col      as uinteger _        /' colour '/
	) Export

	dim as integer sw        = yetiplay.screen_width
	dim as integer sh        = yetiplay.screen_height
	dim as integer sh4       = sh \ 4

	#define NUMNOTES 48

	dim as single hzz(0 to NUMNOTES - 1) = { _
		65.41, _
		69.30, _
		73.42, _
		77.78, _
		82.41, _
		87.31, _
		92.50, _
		98.00, _
		103.83, _
		110.00, _
		116.54, _
		123.47, _
		130.81, _
		138.59, _
		146.83, _
		155.56, _
		164.81, _
		174.61, _
		185.00, _
		196.00, _
		207.65, _
		220.00, _
		233.08, _
		246.94, _
		261.63, _
		277.18, _
		293.66, _
		311.13, _
		329.63, _
		349.23, _
		369.99, _
		392.00, _
		415.30, _
		440.00, _
		466.16, _
		493.88, _
		523.25, _
		554.37, _
		587.33, _
		622.25, _
		659.26, _
		698.46, _
		739.99, _
		783.99, _
		830.61, _
		880.00, _
		932.33, _
		987.77 _
	}

	for qq as integer = 0 to NUMNOTES - 1
		dim as integer hz = hzz(qq)
		dim as integer z = int((n / 44100) * hz)
		dim as double  dist        = sqrt(sqrt( out_data[z].re^2 + out_data[z].im^2 ))
		dim as integer line_height = 0

		line_height = (dist / max_amp(z)) * ((sw / NUMNOTES) / 2)
		if line_height >= ((sw / NUMNOTES) / 8) then
			dim as integer circ_x = ((sw / NUMNOTES) * qq) + ((sw / NUMNOTES) / 2)
			dim as integer circ_y = sh4
			circle( circ_x, circ_y ), line_height, col
			if line_height >= ((sw / NUMNOTES) / 4) then
				circle( circ_x, (circ_y - (sw / NUMNOTES) * 2) ), line_height, col
			end if
		end if
	Next qq

End Sub

sub fade_screen Alias "fade_screen" _
	( _
		byval sp       as any ptr, _        /' screen ptr '/
		byval pitch    as integer _         /' screen pitch '/
	) Export

	if sp = 0 then exit sub

	dim as integer sw         = yetiplay.screen_width
	dim as integer sh         = yetiplay.screen_height

	#define FADE_AMOUNT 32

	' perform a fade out effect on the old imagep
	for y as integer = 0 to sh - 1
		dim as uinteger ptr p = sp
		for x as integer = 0 to sw - 1
			dim as ubyte ptr pp = cast( ubyte ptr, @p[x] )
			dim as integer   b  = pp[0]
			dim as integer   g  = pp[1]
			dim as integer   r  = pp[2]

			' Fade with clip at zero
			pp[0] = ((b - FADE_AMOUNT) >= 0) and (b - FADE_AMOUNT)
			pp[1] = ((g - FADE_AMOUNT) >= 0) and (g - FADE_AMOUNT)
			pp[2] = ((r - FADE_AMOUNT) >= 0) and (r - FADE_AMOUNT)
		next x
		sp += pitch
	next y

end sub

'::::::::
sub draw_screen Alias "draw_screen" _
	( _
		byval n        as integer, _        /' number of samples '/
		byval in_data  as complex_t ptr, _  /' sample data '/
		byval out_data as complex_t ptr, _  /' sample data after fft '/
		byval max_freq as integer _         /' display 0 - max_freq '/
	) Export

	dim as integer sw = yetiplay.screen_width
	dim as integer sh = yetiplay.screen_height

	static as any ptr sp
	static as integer pitch

	if sp = NULL then
		screeninfo , , , , pitch
		sp = screenptr( )
	end if

	' adjust the max_amp table if needed
	for i as integer = 0 to n - 1
		dim as double dist = sqrt( sqrt( (out_data[i].re^2) + (out_data[i].im^2) ) )
		if dist > max_amp(i) then max_amp(i) = dist
	next i

	fade_screen( sp, pitch )

	dim as double wid_scale = sw / n

	dim as uinteger c
	dim as integer green
	dim as integer blue

	scope
		dim as integer c3i   = int((n / 44100) * 130.81) ' index for c3
		dim as double  dist  = sqrt(sqrt( out_data[c3i].re^2 + out_data[c3i].im^2 ))
		green = (dist / max_amp(c3i)) * 255
		if green < 0   then green = 0
		if green > 255 then green = 255
		c or= green shl 8
	end scope

	scope
		dim as integer c2i   = int((n / 44100) * 65.41) ' index for c2
		dim as double  dist  = sqrt(sqrt( out_data[c2i].re^2 + out_data[c2i].im^2 ))
		blue = (dist / max_amp(c2i)) * 255
		if blue < 0   then blue = 0
		if blue > 255 then blue = 255
		c or= blue shl 0
	end scope

	draw_scope( n, in_data, &HFF0000 or c )

	draw_fft( n, out_data, max_freq, RGB( 191 + (blue \ 4), 191 + (blue \ 4), 191 + (blue \ 4) ), sp, pitch )

	draw_notes( n, out_data, &H00FFFF )

end sub
