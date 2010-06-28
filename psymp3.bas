'
' PsyMP3 Player
' Copyright (C) 2007-2010 Kirn Gill <segin2005@gmail.com>
'
' Parts of this code are from OpenGH, copyright as above.
'
' FMOD code based upon the "playmp3.bas" example, and the 
' FMOD player example provided with the FMOD distribution.
' Portions Copyright (C) 1999-2004 Firelight Technologies Pty, Ltd.
'
' FreeType interface code from the Freetype2 library test, 
' by jofers (spam[at]betterwebber.com) 
'
' vim: ts=4 sw=4 syntax=freebasic ff=dos encoding=utf-8
'
' $Id $
'

#Include Once "psymp3.bi"

LIBXML_TEST_VERSION()

Dim Shared As Playlist Songlist
Dim Shared As LastFM Scrobbler

Sub EndPlayer Alias "EndPlayer" () Export
	#ifdef __FB_LINUX__
		' kill_(getpid(),SIGINT)
	#ElseIf Defined(__FB_WIN32__)
		DestroyWindow(WAWindow)
	#Else 
		' TODO: For future use
	#EndIf

	End
End Sub

Function plGetPosition Alias "plGetPosition" () As Integer Export
	Return Songlist.getPosition()
End Function

Function plGetEntries Alias "plGetEntries" () As Integer Export
	Return Songlist.getEntries()
End Function

Sub InitFMOD Alias "InitFMOD" () Export 
	if( FSOUND_GetVersion < FMOD_VERSION ) then 
		MsgBox hWnd, "FMOD version " + str(FMOD_VERSION) + " or greater required" 
		end 1
	end If 
#Ifdef __FB_LINUX__
#ifndef __PREFER_OSS__
	FSOUND_SetOutput(FSOUND_OUTPUT_ALSA)
#else
	FSOUND_SetOutput(FSOUND_OUTPUT_OSS)
#endif
#endif
	if( FSOUND_Init( 44100, 4, 0 ) = 0 ) then 
		' Try two channel output...
		if( FSOUND_Init( 44100, 2, 0 ) = 0 ) Then
			If( FSOUND_Init( 11025, 1, 0) = 0 ) Then
				MsgBox hWnd, "Can't initialize FMOD" 
				end 1   
			End If
		End If
	end if 
	FSOUND_Stream_SetBufferSize( 1024 )
End Sub

#endif


''
'' End of functions
''
Dim Nkey As String
dim stream2 as FSOUND_STREAM ptr
dim songs as Integer, currsong as Integer, PathSep as String
dim spectrum as Single Ptr
dim doexit as Integer
dim streamended as Integer
Dim Sock As SOCKET
Dim DoFPS As Integer
Dim As Integer sFont, SpectrumOn = 1
Dim As String olddir, userfontdir
Dim I As Integer
Dim X As Integer
Dim Y As Integer
Dim T As Integer
Dim dectBeat As Integer
Dim Action as String
Dim As Integer posx, posy, buttons, mousex, mousey, _
	   oldposx, oldposy, oldmousex, oldmousey
Dim FPS(100) As Double
Dim Sum As Double
Dim As String cpuvendor, cpuname
Dim As Integer doPrev
olddir = CurDir

#Ifdef USE_ASM
cpuvendor = GetCPUVendor()
cpuname = GetCPUName()
#EndIf

If cpuvendor = "GenuineIntel" Then
	' Intel fucked their own CPUID instruction. No fault of my own, ugh...
	For x = 0 To 47 
		If cpuname[x] <> 32 Then
			cpuname = Mid(cpuname, x+1)
			Exit For
		EndIf
	Next x
EndIf

#Ifndef __FB_WIN32__
libui_init(__FB_ARGC__,__FB_ARGV__)
#endif

doCommand = PSYMP3_PLAY

' Announce ourselves via the CONSOLE!
printf(!"PsyMP3 version %s, built with FreeBASIC %d.%d.%d\n", _
       PSYMP3_VERSION,__FB_VER_MAJOR__,__FB_VER_MINOR__,__FB_VER_PATCH__)
printf(!"CPU is %s %s.\n", cpuvendor, cpuname)
#Ifdef __FB_SSE__
	#Ifdef USE_ASM 
		printf(!"Compiled with SSE support.\n")
		If (IsSSE2() = False) Then 
			Dim Feats As String
			Dim retvalue As UInteger
			retvalue =  CPUID_EDX(1)
			If (retvalue and &H2000000) Then Feats = "SSE"
			If (retvalue And &H4000000) Then Feats += ", SSE2" ' Should not happen unless testing.
			If(IsSSE() = TRUE) Then
				MsgBox(0,!"WARNING! Your CPU only supports original SSE!\n" _
					!"Your mileage may vary! If PsyMP3 crashes, you know why!\n" _
					!"Your CPU is probably a Pentium III or similar.\n\n" _
					!"Your CPU supports these instruction sets: " + Feats) 
			Else
				MsgBox(0,!"This build of PsyMP3 was compiled with SSE support and your\n" _
					!"CPU lacks support for SSE and SSE2. Please rebuild without\n" _
					!"SSE FPU support (compile with -fpu 387.)\n\n" _
					!"Your CPU supports these instruction sets: " + Feats)
				End 0
			End If
		End If
	#EndIf
#Else
	printf(!"Compiled without SSE support.\n")
#EndIf

If Command(1) = "--version" Then
	ShowAbout(0)
	End
End If

Dim IsSilent As Integer

If Command(1) = "--silent" Then
	IsSilent = 1 
EndIf

If Command(1) = "--largo" Then
	MsgBox hWnd, !"piro: OH MY GOD!! WHAT ARE YOU DOING?!?!\n\n" & _
	!"largo: Relax, I got a copy of \"Meth for Dummies\"!\n\n" & _
	!"piro: No! No! No! You *moron*!! It's illegal,\nit's dangerous, " & _
	!"and the whole place could blow..."
	End
End If

#Ifndef IGNORE_FBVER
	#if (__FB_VER_MAJOR__ = 0) And (__FB_VER_MINOR__ >= 20)
      ' Not known if this is needed because gfxlib2 is using more
      ' advanced DirectDraw features than is supported on my laptop
      ' or if the DirectX backend is just plain broken. This is clear:
      ' On my laptop, using the DirectX gfxlib2 backend = crash
      '
      ' FreeBASIC 0.18.5 works just fine...
      ' 
      ' I have verified it to be the backend. GDB doesn't lie.
		#ifdef __FB_WIN32__
			SetEnviron("fbgfx=GDI")
		#endif
	#EndIf
#EndIf

If IsSilent <> 1 Then
	ScreenRes 640, 400, 32, 2
Endif

ScreenControl 2, hWnd

#ifdef __FB_LINUX__
	#ifdef __PERFER_OSS__
		printf(!"Sound output: OSS\n")
		FSOUND_SetOutput(FSOUND_OUTPUT_OSS)
	#else
		#ifdef __PERFER_ESD__
			printf(!"Sound output: ESound (ESD)\n")
			FSOUND_SetOutput(FSOUND_OUTPUT_ESD)
		#else
			printf(!"Sound output: ALSA\n") 
			FSOUND_SetOutput(FSOUND_OUTPUT_ALSA)
		#endif
	#endif
#EndIf

WindowTitle "PsyMP3 " & PSYMP3_VERSION & " - Not playing"
InitFMOD()        
#Ifdef CLASSIC
If Command(1) <> "" Then
	mp3file = Command(1)
Else
  
	mp3file = file_getname(hWnd)
	If mp3file = "" Then
		MsgBox hWnd, "No mp3 selected, exiting..."
		End
	End If
	If mp3file = "--version" Then
		MsgBox hWnd, "This is PsyMP3 version " & PSYMP3_VERSION
		End
	End If
End If 
#else

#If 1
Do
	mp3file = file_getname(hWnd)
	If mp3file = "" Then
		If Songlist.getEntries() = 0 Then 
			MsgBox hWnd, "No mp3 selected, exiting..."
			End
		Else
			Exit Do
		End If   
	End If
	If LCase(Right(mp3file,4)) = ".m3u" Then
		Songlist.addPlaylist(mp3file)
	Else
		Songlist.addFile(mp3file)
	End If
Loop 
#endif

InitKVIrcWinampInterface()

#If Defined(__FB_WIN32__) And Defined(LIBSEVEN)
printf("Attempting to initialize ITaskbarList3... ")
Var ITB = InitializeTaskbar()
If ITB = -1 Then
	printf(!"failed!\n")
Else
	printf(!"done.\n")
EndIf
AssociateHwnd(hWnd)
#EndIf

#endif
mp3file = Songlist.getNextFile()
        
printf(!"File to open: \"%s\"\n", mp3file)
stream = FSOUND_Stream_Open( mp3file, FSOUND_MPEGACCURATE, 0, 0 )
songstart = Time_(NULL)

if( stream = 0 ) then 
	MsgBox hWnd, !"Can't load music file \"" + mp3file + !"\""
	End 1
end if

If IsSilent <> 1 Then
FSOUND_DSP_SetActive(FSOUND_DSP_GetFFTUnit, 1)
End If 

InitFT()

#ifdef __FB_WIN32__
	Dim As ZString * 150 WindowsDir
	GetWindowsDirectory(WindowsDir, 150)
	PathSep = !"\\"
	userfontdir = WindowsDir + PathSep + "Fonts"
#else
	Dim As String username 
	PathSep = !"/"
	username = *getenv("USER")
	userfontdir = "/home/" + username + "/.fonts"
#endif

sFont = GetFont(olddir + PathSep + "vera.ttf")
if sFont = 0 Then
	sFont = GetFont(userfontdir + PathSep + "vera.ttf")
#ifdef __FB_WIN32__
    ' If BitStream Vera Sans is not available, try Tahoma.
	If sFont = 0 Then
		sFont = GetFont(userfontdir + PathSep + "segoeui.ttf")
			If sFont = 0 Then
				sFont = GetFont(userfontdir + PathSep + "tahoma.ttf")
			End If
	End If
#Else
	' Look in more places. Vera.ttf must be found!
	If sFont = 0 Then
		' On modern Linux systems, check here
		userfontdir = "/usr/share/fonts/TTF"
		sFont = GetFont(userfontdir + PathSep + "vera.ttf")
	End If
#endif
End If

mp3name = getmp3name(stream)
mp3artist = getmp3artist(stream)
mp3album = getmp3album(stream)
mp3nameW = *getmp3nameW(stream)
mp3artistW = *getmp3artistW(stream)
mp3albumW = *getmp3albumW(stream)

getmp3albumart(stream)
var blank = ImageCreate(640, 350, rgba(0, 0, 0, 64), 32)
If isSilent <> 1 Then
	WindowTitle "PsyMP3 " + PSYMP3_VERSION + " - Playing - -:[ " & mp3artistW & " ]:- -- -:[ " + mp3nameW + " ]:-"
	ScreenSet 1
	DoFPS = 0
	spectrum = FSOUND_DSP_GetSpectrum()
End If
FSOUND_Stream_Play( FSOUND_FREE, stream )
songlength = FSOUND_Stream_GetLengthMs(stream)
Dim As Any Ptr TextMap = ImageCreate(640, 50, RGB(0, 0, 0), 32)
Do
	IsPaused = 0
	#Ifdef __FB_WIN32__
		ClearWMP()
		AnnounceWMP(mp3artist, mp3name, mp3album)
	#Ifdef LIBSEVEN
		SetProgressType(TASKBAR_PROGRESS)
	#EndIf
	#EndIf
	If sock <> 0 Then
		hClose(sock)
	End If
	sock = Scrobbler.setNowPlaying()
   
	Line(0,350)-(639,399),0,BF
	PrintFTW(1,366,"Artist: " + mp3artistW,sFont,12,rgb(255,255,255))
	PrintFTW(1,381,"Title: " + mp3nameW,sFont,12)
	PrintFTW(1,396,"Album: " + mp3albumW,sFont,12) 
	PrintFTW(300,366,"Playlist: " & Songlist.getPosition & "/" & Songlist.getEntries, sFont, 12, rgb(255,255,255)) 
	PrintFTW(280,396,"CPU: " + cpuname + ", Vendor: " + cpuvendor,sFont,9)
	Get (0, 350)-(639,399), TextMap
	Do  
		#ifdef __FB_WIN32__     
		SetWindowText(WAWindow, _
		Songlist.getPosition & ". " + mp3artistW + " - " + mp3nameW + " - Winamp")
		#Ifdef LIBSEVEN
		If IsPaused = 1 Then 
			SetProgressType(TASKBAR_PAUSED)
		Else
			SetProgressType(TASKBAR_PROGRESS)
			UpdateProgressBar(FSOUND_Stream_GetTime(stream),FSOUND_Stream_GetLengthMs(stream))
		EndIf
		#EndIf
		#EndIf
		Sleep 12 ' Timer delay

		If IsSilent <> 1 Then
			If SpectrumOn = 1 Then      
			DrawSpectrum(spectrum)       
			End If

			'ScreenUnlock
			'Window title, is program name/version, artist and songname, and 
			'time elapsed/time remaining
	
			For i = 1 To 99
				fps(i) = fps(i+1)
			Next
			FPS(100) = Timer
			Dim avg As Double
			avg = 0   
			For i = 100 To 2 Step -1
				avg += fps(i) - fps(i - 1)
			Next i
			avg /= 99.0
	
			If DoFPS = 1 Then
				Var szFPS = Format(avg * 1000, "#.00 FPS")
				Locate 1, 70 : Print szFPS
				Locate 2, 70 
				Dim As UInteger status, bufferused, bitrate, flags
				FSOUND_Stream_Net_GetStatus(stream, @status, @bufferused, @bitrate, @flags)
				Print bitrate
			End If
			FPS(1) = Timer
			If IsPaused = 0 Then
				Action = "Playing"
			Else
				Action = "Paused"
			End If
			WindowTitle Int((FSOUND_Stream_GetTime(stream) / FSOUND_Stream_GetLengthMs(stream)) _ 
				* 100) & "% PsyMP3 " & PSYMP3_VERSION & " - " + Action + " - -:[ " _
				+ mp3artist + " ]:- -- -:[ " + mp3name + " ]:- [" & _ 
				Int(FSOUND_Stream_GetTime(stream)/1000)\60 & ":" & _
				Format(Int(FSOUND_Stream_GetTime(stream)/1000) Mod 60,"00") _ 
				& "/" & Int(FSOUND_Stream_GetLengthMs(stream)/1000)\60 & ":" & _
				Format(Int(FSOUND_Stream_GetLengthMs(stream)/1000) Mod 60,"00") & "]"
			' Lock the screen for flicker-free recomposition
			' of display. Disable if you want to see if you
			' will experience photosensitive elliptic seizures.
			PCopy 1,0
			#Ifndef VOICEPRINT	
				Put (0, 0), blank, Alpha
				Line (0,351)-(640,400), 0, BF
			#Else 
				Dim Image As Any Ptr
			#EndIf
			Put (0, 350), TextMap, PSet
			'Put (600, 350), wmctl
			'Time elapsed and time remaining
			PrintFT((400+620)/2-40,365, _
				Int(FSOUND_Stream_GetTime(stream)/1000)\60 & ":" & _
				Format(Int(FSOUND_Stream_GetTime(stream)/1000) Mod 60,"00") & "." & _
				Left(Right(Format(Int(FSOUND_Stream_GetTime(stream)),"000"),3),2) & _
				"/" & Int(FSOUND_Stream_GetLengthMs(stream)/1000)\60 & ":" & _
				Format(Int(FSOUND_Stream_GetLengthMs(stream)/1000) Mod 60,"00"), _
				sFont, 12)

   
			' The brackets on the ends of the progress bar
			Line(399,370)-(399,385), rgb(255,255,255)
			Line(621,370)-(621,385), rgb(255,255,255)
			Line(399,370)-(402,370), rgb(255,255,255)
			Line(399,385)-(402,385), rgb(255,255,255)
			Line(618,370)-(621,370), rgb(255,255,255)
			Line(618,385)-(621,385), rgb(255,255,255)
			' The progress bar
			T = (FSOUND_Stream_GetTime(stream) / FSOUND_Stream_GetLengthMs(stream)) * 220
			For x = 0 to T
				'' The Logic: 
				'' The color code is broken into 3 sections. The first section
				'' starts with 50% red and 100% green, with blue ranging from 0%
				'' to 100% at the section end.
				'' The second section has a constant 100% blue content, with red 
				'' declining from 50% to 0%, and green declining from 100% to 0%
				'' The third section has a constant 0% green and 100% blue content,
				'' with a incline in red from 0% to 100%

				Line(400+x,373)-Step(0,9), _
					RGB( _
						-128*(x<=146)+(x-73)*1.75*(x>=73 And x<=146)-(x-146)*3.5*(x>146), _
						-255*(x<=146)+(x-73)*3.5*(x>=73 And x<=146), _
					-255*(x>=73)-x*3.5*(x<73) _
				)
			Next x
			If doRepeat = 1 Then
				PrintFT(540,30,"Repeat Track",sFont,12,rgb(242,242,242))
			End If
			'Keyboard check
			Nkey = InKey
			If nkey = Chr(27) Or nkey = Chr(255) + "k" Or lcase(nkey) = "q" Then 
				#ifdef __FB_WIN32__
					ClearWMP()
				#EndIf
					Scrobbler.scrobbleTrack()
					EndPlayer()
			End If
			If nkey = Chr(255) + "K" Then ' Left key pressed, go back 1.5sec
				Line(380,377)-(390,377), rgb(255,0,0)
				Line(380,377)-(383,374), rgb(255,0,0)
				Line(380,377)-(383,380), rgb(255,0,0)
				FSOUND_Stream_SetTime(stream, iif(FSOUND_Stream_GetTime(stream) - 1500 < 0, _
					0, FSOUND_Stream_GetTime(stream) - 1500))
			End If
			If nkey = Chr(255) + "M" Then ' Right key pressed, forward 1.5sec
				Line(628,377)-(638,377), rgb(0,255,0)
				Line(638,377)-(635,374), rgb(0,255,0)
				Line(638,377)-(635,380), rgb(0,255,0)
				FSOUND_Stream_SetTime(stream, FSOUND_Stream_GetTime(stream) + 1500)
			End If
			If nkey = " " Then
				If IsPaused = 1 Then
					IsPaused = 0
					#If Defined(__FB_WIN32__) And Defined(LIBSEVEN)
						SetProgressType(TASKBAR_PROGRESS)
					#EndIf 
				Else
					#If Defined(__FB_WIN32__) And Defined(LIBSEVEN)
						SetProgressType(TASKBAR_PAUSED)
					#EndIf
				IsPaused = 1
				End If
				FSOUND_SetPaused(FSOUND_ALL, IsPaused)
			End If
			If doCommand = PSYMP3_PAUSE And IsPaused = 1 Then
				IsPaused = 0
				doCommand = PSYMP3_PLAY
			End If
			If doCommand = PSYMP3_PAUSE Then
				doCommand = PSYMP3_PLAY
				If IsPaused = 1 Then
					IsPaused = 0 
				Else
					IsPaused = 1
				End If
				FSOUND_SetPaused(FSOUND_ALL, IsPaused)
			End If
			If LCase(nkey) = "e" Then
				var effect = FSOUND_GetSurround(0)
				if effect = TRUE Then
					FSOUND_SetSurround(FSOUND_ALL, FALSE)
					printf(!"FSOUND_SetSurround(FSOUND_ALL, FALSE);\n")
				Else 
					FSOUND_SetSurround(FSOUND_ALL, TRUE)
					printf(!"FSOUND_SetSurround(FSOUND_ALL, TRUE);\n")
				End If
			End If
			If LCase(nkey) = "s" Then
				If SpectrumOn = 1 Then
					FSOUND_DSP_SetActive(FSOUND_DSP_GetFFTUnit, 0)
					printf(!"FSOUND_DSP_SetActive(FSOUND_DSP_GetFFTUnit, 0);\n")
					SpectrumOn = 0
				Else
					FSOUND_DSP_SetActive(FSOUND_DSP_GetFFTUnit, 1)
					printf(!"FSOUND_DSP_SetActive(FSOUND_DSP_GetFFTUnit, 1);\n")
					SpectrumOn = 1
				End If
			End If
			If LCase(nkey) = "a" Then
			stream2 = ThreadCreate(@ShowAbout)
			End If 
			If LCase(nkey) = "b" Then
				If doRepeat = 1 Then
					doRepeat = 0 
				Else 
					doRepeat = 1
				End If
			End If
			' Restart playing song.
			If lcase(nkey) = "r" Then
				FSound_Stream_SetTime(stream, 0)
			End If
			' Move the window if dragged
			GetMouse(mousex, mousey, , buttons)
			If (buttons And 1) And ((mousex <> -1) And (oldmousex <> -1)) Then
				ScreenControl GET_WINDOW_POS, posx, posy
				If (oldmousex <> mousex) Or (oldmousey <> mousey) Then
					ScreenControl(SET_WINDOW_POS, posx + (mousex - oldmousex), _
						posy + (mousey - oldmousey))
				End If
			End If
			oldmousex = mousex : oldmousey = mousey
			' End move window if dragged
			' Experimental support for loading a new file
			If LCase(nkey) = "l" Then
				mp3file = file_getname(hWnd)
				If mp3file <> "" Then
					FSOUND_Stream_Stop(stream)
					FSOUND_Stream_Close(stream)
					Scrobbler.scrobbleTrack()
					printf(!"File to open: \"%s\"\n", mp3file)
					stream = FSOUND_Stream_Open( mp3file, FSOUND_MPEGACCURATE, 0, 0 )
					songstart = Time_(NULL)
					if( stream = 0 ) then 
						MsgBox hWnd, !"Can't load music file \"" + mp3file + !"\""
						end 1
					end if
					mp3name = getmp3name(stream)
					mp3artist = getmp3artist(stream)
					mp3album = getmp3album(stream)
					mp3nameW = *getmp3nameW(stream)
					mp3artistW = *getmp3artistW(stream)
					mp3albumW = *getmp3albumW(stream)
					'getmp3albumart(stream)
					FSOUND_Stream_Play( FSOUND_FREE, stream )
					songlength = FSOUND_Stream_GetLengthMs(stream)
					IsPaused = 0
#ifdef __FB_WIN32__
					ClearWMP()
					AnnounceWMP(mp3artist, mp3name, mp3album)
#EndIf
					hClose(sock)
					Line(0,350)-(639,399),0,BF
					PrintFTW(1,366,"Artist: " + mp3artistW,sFont,12,rgb(255,255,255))
					PrintFTW(1,381,"Title: " + mp3nameW,sFont,12)
					PrintFTW(1,396,"Album: " + mp3albumW,sFont,12) 
					PrintFTW(300,366,"Playlist: " & Songlist.getPosition & "/" & Songlist.getEntries, sFont, 12, rgb(255,255,255)) 
					PrintFTW(280,396,"CPU: " + cpuname + ", Vendor: " + cpuvendor,sFont,9)
					Get (0, 350)-(639,399), TextMap
					sock = Scrobbler.setNowPlaying()
				End If
			End If
			' Append song or m3u to playlist.
			If LCase(nkey) = "i" Then
				mp3file = file_getname(hWnd)
				If mp3file <> "" Then
					If LCase(Right(mp3file,4)) = ".m3u" Then
						Songlist.addPlaylist(mp3file)
					ElseIf LCase(Right(mp3file,5)) = ".m3u8" Then
						' parse_m3u8_playlist(mp3file)
					Else
						Songlist.addFile(mp3file)
						Line(0,350)-(639,399),0,BF
						PrintFTW(1,366,"Artist: " + mp3artistW,sFont,12,rgb(255,255,255))
						PrintFTW(1,381,"Title: " + mp3nameW,sFont,12)
						PrintFTW(1,396,"Album: " + mp3albumW,sFont,12) 
						PrintFTW(300,366,"Playlist: " & Songlist.getPosition & "/" & Songlist.getEntries, sFont, 12, rgb(255,255,255)) 
						PrintFTW(280,396,"CPU: " + cpuname + ", Vendor: " + cpuvendor,sFont,9)
						Get (0, 350)-(639,399), TextMap
					End If
				End If
			End If
			' Dump playlist. DISABLED for 1.1-RELEASE due to buggyiness.
/'	
			If LCase(nkey) = "o" Then
				PrintFT(30,130, "Saving playlist to " & CurDir() & "PsyMP3-" & time_(NULL) & ".m3u", sFont, 32, RGB(255,255,255))
				PCopy 1,0
			Songlist.savePlaylist("PsyMP3-" & time_(NULL) & ".m3u")
			End If
'/
			If LCase(nkey) = "z" Then
				If DoFPS = 0 Then
					DoFPS = 1
				Else
					DoFPS = 0
				End If
			End If
			If LCase(nkey) = "n" Then
				Exit Do
			End If
			If doCommand = PSYMP3_PLAY_NEXT Then
				doCommand = PSYMP3_PLAY
				Exit Do
			End If
			If doCommand = PSYMP3_PLAY_PREV Then
				doCommand = PSYMP3_PLAY
				If Songlist.getPosition() > 1 Then
					doPrev = 1
					Exit Do
				End If
			End If 
			If LCase(nkey) = "p" Then
				If Songlist.getPosition() > 1 Then
					doPrev = 1
					Exit Do
				End If
			End If
			' The previous will probably break and do so horribly
			' Play and pause symbols to show status in a graphical and iconic fashion.
			If IsPaused = 1 Then
				Line (580,354)-(583,366), rgb(255,255,255), BF
				Line (586,354)-(589,366), rgb(255,255,255), BF
			Else
				Line (580,354)-(586,360), rgb(32,255,32)
				Line (580,354)-(580,366), rgb(32,255,32)
				Line (586,360)-(580,366), rgb(32,255,32)
				Paint (582, 360), rgb(32,255,32), rgb(32,255,32)
			End If
			'Beat detection and gfx
			Draw "BM630,360"
			dectBeat = 0
			For x = 0 to 5
				If spectrum[x] >= 0.8425 Then
					dectBeat = 1
				End If
			Next x
			If dectBeat = 0 Then
				Draw "BU3 C" & rgb(128,0,32) & " R2 D1 R1 D1 R1 D2 L1 D1 L1"
			Else
				Draw "BU3 C" & rgb(255,0,0) & " R2 D1 R1 D1 R1 D2 L1 D1 L1"
			End If
			Draw "D1 L2 U1 L1 U1 L1 U2 R1 U1 R1 U1"
			Draw "BM605,358 C" & rgb(255,255,255) & "D5 R2 BU1 BR1 U2 BU1 BL1 L2 BR3 BU1 U1"
			Draw "BU1 BL1 L2 D1 U1 BR5 R3 L3 D7 R3 L3 U4 R2 L2 U3 BR5 BD1 D6 U6 BU1 BR1 R1"
			Draw "BR1 BD1 D6 U3 L2 BU4 BR4 R2 L1 D7"

			If (FSOUND_Stream_GetTime(stream) = FSOUND_Stream_GetLengthMs(stream)) And (doRepeat = 1) Then 
				FSOUND_Stream_SetTime(stream, 0)
				' Since this is legitimatly a new song, like any real player's track loop.
				' You track loop on Winamp, AIMP2, or foobar2000, you flood your last.fm scrobble list
				' with the same song over and over.
			#ifdef __FB_WIN32__
				ClearWMP()
				AnnounceWMP(mp3artist, mp3name, mp3album)
			#EndIf
				hClose(sock)
				sock = Scrobbler.setNowPlaying()
			End If
		End IF
	Loop While (FSOUND_Stream_GetTime(stream) <> FSOUND_Stream_GetLengthMs(stream)) Or (doRepeat = 1)
	' I don't want to quit at end-of-song anymore.
	FSOUND_Stream_Stop(stream)
	FSOUND_Stream_Close(stream)
	Scrobbler.scrobbleTrack()
	#ifdef __FB_WIN32__
		ClearWMP()
		#Ifdef LIBSEVEN
			SetProgressType(TASKBAR_INDETERMINATE)
		#EndIf
	#EndIf
	isPaused = 2
	
	If doPrev = 1 Then
		doPrev = 0
		mp3file = Songlist.getPrevFile()
	Else
		mp3file = Songlist.getNextFile()
	End If
	
	If mp3file = "" Then 
		isPaused = 2
		mp3file = file_getname(hWnd)
	End If

	If mp3file <> "" Then
		If LCase(Right(mp3file,4)) = ".m3u" Then
			mp3name = Songlist.getPrevFile()
			Songlist.addPlaylist(mp3file)
			mp3file = Songlist.getNextFile()
			printf(!"File to open: \"%s\"\n", mp3file)
			stream = FSOUND_Stream_Open( StrPtr(mp3file), FSOUND_MPEGACCURATE, 0, 0 )
			songstart = Time_(NULL)
			If( stream = 0 ) then 
				MsgBox hWnd, !"Can't load music file \"" + mp3file + !"\""
				end 1
			end If
			mp3name = getmp3name(stream)
			mp3artist = getmp3artist(stream)
			mp3album = getmp3album(stream)
			mp3nameW = *getmp3nameW(stream)
			mp3artistW = *getmp3artistW(stream)
			mp3albumW = *getmp3albumW(stream)
			'getmp3albumart(stream)
			FSOUND_Stream_Play( FSOUND_FREE, stream )
			songlength = FSOUND_Stream_GetLengthMs(stream)
			IsPaused = 0
		'ElseIf LCase(Right(mp3file,5)) = ".m3u8" Then
			'parse_m3u_playlist(mp3file)
		Else
		printf(!"File to open: \"%s\"\n", mp3file)   	
		stream = FSOUND_Stream_Open( StrPtr(mp3file), FSOUND_MPEGACCURATE, 0, 0 )
		songstart = Time_(NULL)
		If( stream = 0 ) then 
			MsgBox hWnd, !"Can't load music file \"" + mp3file + !"\""
			EndPlayer()
		end If
		mp3name = getmp3name(stream)
		mp3artist = getmp3artist(stream)
		mp3album = getmp3album(stream)
		mp3nameW = *getmp3nameW(stream)
		mp3artistW = *getmp3artistW(stream)
		mp3albumW = *getmp3albumW(stream)
		'getmp3albumart(stream)
		FSOUND_Stream_Play( FSOUND_FREE, stream )
		songlength = FSOUND_Stream_GetLengthMs(stream)
		IsPaused = 0
	End If
	Else
		Exit Do
	End If
Loop
#ifdef __FB_WIN32__
	ClearWMP()
#EndIf
EndPlayer()
End
