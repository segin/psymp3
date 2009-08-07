'
' PsyMP3 Player
' Copyright (C) 2007-2009 Kirn Gill <segin2005@gmail.com>
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
' vim: ts=4 syntax=freebasic
'
 
#include once "crt.bi"
#include once "crt/stdlib.bi"
#include once "crt/sys/types.bi"
#include once "crt/stddef.bi"
#include once "fmod.bi"
#include once "fbgfx.bi"
'#Include Once "id3tag.bi"
'#include once "jpeg.bi"
#include once "freetype2/freetype.bi"
#ifdef __FB_WIN32__
#define WIN_INCLUDEALL
#include once "windows.bi"
#else
#define SIGINT 3
#endif
#include once "vbcompat.bi"

#Include "md5.bi"
#Include "wshelper.bas"

#define PSYMP3_VERSION "1.1-CURRENT"

#If Not Defined(Boolean)
   #Define Boolean integer
#endif

Dim Shared As String mp3artist, mp3name, mp3album, mp3file
Dim Shared stream as FSOUND_STREAM Ptr
Dim Shared IsPaused as Integer
Dim Shared doRepeat As Integer
Dim Shared doCommand As Integer
Dim Shared As String lastfm_username, lastfm_password, lastfm_sessionkey
Dim Shared songstart As Integer ' Song start time in UNIX format.
#ifdef __FB_WIN32__
Dim Shared WAWindow As HWND
#endif
Dim Shared songlength As Integer

Declare Function getmp3artist(stream As FSOUND_STREAM Ptr) As String
Declare Function getmp3name(stream As FSOUND_STREAM Ptr) As String

Type extendedFileInfoStructW
   As WString Ptr filename
   As WString Ptr metadata
   As WString Ptr ret
   As Integer retlen
End Type

Using FB

'' Playlist code. Object-oriented as an experiment

Type Playlist Alias "Playlist"
Private:
   m_entries As Integer
   m_position As Integer
   m_playlist(15000) As String
   m_israw(15000) As Integer
   ' These are ONLY for RAW CD-Audio track files as specified in a WINCDR CUE sheet.
   ' DO NOT USE THESE FOR ANYTHING OTHER FILE TYPE -- THEY WILL BE IGNORED.
   m_artist(15000) As String
   m_album(15000) As String
   m_title(15000) As String
Public:
   Declare Constructor ()
   Declare Sub addFile(file As String)
   Declare Sub addRawFile(file As String)
   Declare Sub savePlaylist(file As String)
   Declare Function getNextFile() As String
   Declare Function getPrevFile() As String
   Declare Function getPosition() As Integer
   Declare Function getEntries() As Integer
   Declare Function isFileRaw() As Integer
   Declare Function getFirstEntry() As String
End Type

Dim Shared As Playlist Songlist

Constructor Playlist ()
   this.m_entries = 0
   this.m_position = 0
   this.m_playlist(1) = ""
End Constructor

Sub Playlist.addFile Alias "addFile" (file As String)
   this.m_entries += 1
   this.m_playlist(this.m_entries) = file
End Sub

Sub Playlist.addRawFile Alias "addRawFile" (file As String)
   this.m_entries += 1
   this.m_playlist(this.m_entries) = file
   this.m_israw(this.m_entries) = 1
End Sub

Function Playlist.getNextFile Alias "getNextFile" () As String
   If this.m_entries = 0 Or this.m_entries <= this.m_position Then
      this.m_position = this.m_entries + 1
      Return ""
   EndIf 
   this.m_position += 1
   Return this.m_playlist(this.m_position)
End Function

Function Playlist.getPrevFile Alias "getPrevFile" () As String
   If this.m_entries = 0 Then Return ""
   this.m_position -= 1
   Return this.m_playlist(this.m_position)
End Function

Function Playlist.getPosition Alias "getPosition" () As Integer
   Return this.m_position
End Function

Function Playlist.getEntries Alias "getEntries" () As Integer
   Return this.m_entries
End Function

Function Playlist.getFirstEntry Alias "getFirstEntry" () As String
   this.m_position = 0
   Return this.getNextFile()
End Function

Sub Playlist.savePlaylist Alias "savePlaylist" (file As String)
   ' Rationale: 
   ' This function iterates through the entire playlist and attempts to write
   ' an M3U that is compatible with Winamp, et. al.
   Dim As Integer ret, i, slen
   Dim As Any Ptr fd
   Dim tmp As FSOUND_STREAM Ptr
   Dim As String Artist, Title
   fd = fopen(file, "wt")
   If fd = 0 Then 
      printf(!"Playlist::savePlayList(): Unable to open %s!\n", file)
      Return
   EndIf
   fprintf(fd, "#EXTM3U\n")
   For i = 1 To this.m_entries
      tmp = FSOUND_Stream_Open( this.m_playlist(i), FSOUND_MPEGACCURATE, 0, 0 )
      slen = Int(FSOUND_Stream_GetLengthMs(tmp)/1000)
      Artist = getmp3artist(tmp)
      Title = getmp3name(tmp)
      FSOUND_Stream_Close(tmp)
      fprintf(fd, !"#EXTINF:%d,%s - %s\n", slen, Artist, Title)
      fprintf(fd, !"%s\n", this.m_playlist(i))
   Next
   fclose(fd)
End Sub

'' End Playlist code.

Enum PSYMP3_COMMANDS
   PSYMP3_PLAY_NEXT
   PSYMP3_PLAY_PREV
   PSYMP3_PAUSE
   PSYMP3_PLAY
   PSYMP3_STOP ' alias for kill
End Enum


extern "c"
declare function getpid () as Integer
#ifndef __FB_WIN32__
Declare Sub _Exit Alias "_Exit" (ByVal errcode As Integer)
declare function kill_ alias "kill" (byval pid as pid_t, byval sig as integer) as Integer
#endif
#Ifdef __FB_WIN32__
Declare Function wsprintfW Alias "wsprintfW" (buf As WString ptr, fmt As WString Ptr, ...) As Integer 
#endif
Declare Function dirname Alias "dirname" (path As ZString Ptr) As ZString Ptr 
End Extern


Declare sub DrawGlyph(ByVal FontFT As FT_Face, ByVal x As Integer, ByVal y As Integer, ByVal Clr As UInteger)
Declare Function PrintFT(ByVal x As Integer, ByVal y As Integer, ByVal Text As String, ByVal Font As Integer, ByVal Size As Integer = 14, ByVal Clr As UInteger = Rgb(255, 255, 255)) as integer
Declare Function GetFont(ByVal FontName As String) As Integer
#Ifdef __FB_WIN32__
Declare sub MsgBox(hWnd As HWND, msg As String)
#Else
Extern "C++"
	Declare Sub MsgBox Alias "messageBox" (ByVal hWnd as Integer, ByVal msg As ZString Ptr)
End Extern
#endif
 
#ifdef __FB_WIN32__
#Inclib "dir"
#endif

' Alpha blending 
#define FT_MASK_RB_32         &h00FF00FF 
#define FT_MASK_G_32          &h0000FF00 

Type FT_Var 
    ErrorMsg   As FT_Error 
    Library    As FT_Library 
    PixelSize  As Integer 
End Type 

Type CopyData
    dwData As Integer
    cbData As Integer
    lpData As Any Ptr
End Type

Dim Shared FT_Var As FT_Var 
Function InitFT() As Integer
	Dim console As Integer
	console = FreeFile
	Open Err As #console
	Print #console, "Initalizing FreeType... ";
	FT_Var.ErrorMsg = FT_Init_FreeType(@FT_Var.Library) 
	If FT_Var.ErrorMsg Then
		Print #console, "failed." 
		Function = 0
	Else
		Print #console, "done."
		Function = 1
	End If
	Close #console
End Function

Function GetFont(ByVal FontName As String) As Integer 
   Dim Face As FT_Face 
   Dim ErrorMsg As FT_Error 
   Dim console As Integer
	console = FreeFile
	Open Err As #console
	Print #console, "Loading font ";FontName;"... ";
   ErrorMsg = FT_New_Face(FT_Var.Library, FontName, 0, @Face ) 
   If ErrorMsg Then
	   Print #console, "failed!" 
	   close #console
	   Return 0 
   else
	   Print #console, "done." 
	   close #console
	   Return CInt(Face) 
   end if 
End Function 

' Print Text 
' ---------- 
Function PrintFT(ByVal x As Integer, ByVal y As Integer, ByVal Text As String, ByVal Font As Integer, ByVal Size As Integer = 14, ByVal Clr As UInteger = Rgb(255, 255, 255)) as integer
    Dim ErrorMsg   As FT_Error 
    Dim FontFT     As FT_Face 
    Dim GlyphIndex As FT_UInt 
    Dim Slot       As FT_GlyphSlot 
    Dim PenX       As Integer 
    Dim PenY       As Integer 
    Dim i          As Integer 
    
    ' Get rid of any alpha channel in AlphaClr 
    Clr = Clr Shl 8 Shr 8 

    ' Convert font handle 
    FontFT = Cast(FT_Face, Font) 
    
    ' Set font size 
    ErrorMsg = FT_Set_Pixel_Sizes(FontFT, Size, Size) 
    FT_Var.PixelSize = Size 
    If ErrorMsg Then Return 0 
    
    ' Draw each character 
    Slot = FontFT->Glyph 
    PenX = x 
    PenY = y 
        
    For i = 0 To Len(Text) - 1 
        ' Load character index 
        GlyphIndex = FT_Get_Char_Index(FontFT, Text[i]) 
        
        ' Load character glyph 
        ErrorMsg = FT_Load_Glyph(FontFT, GlyphIndex, FT_LOAD_DEFAULT) 
        If ErrorMsg Then Return 0 
        
        ' Render glyph 
        ErrorMsg = FT_Render_Glyph(FontFT->Glyph, FT_RENDER_MODE_NORMAL) 
        If ErrorMsg Then Return 0 
        
        ' Check clipping 
        If (PenX + FontFT->Glyph->Bitmap_Left + FontFT->Glyph->Bitmap.Width) > 640 Then Exit For 
        If (PenY - FontFT->Glyph->Bitmap_Top + FontFT->Glyph->Bitmap.Rows) > 400 Then Exit For 
        If (PenX + FontFT->Glyph->Bitmap_Left) < 0 Then Exit For 
        If (PenY - FontFT->Glyph->Bitmap_Top) < 0 Then Exit For 
        
        ' Set pixels 
        DrawGlyph FontFT, PenX + FontFT->Glyph->Bitmap_Left, PenY - FontFT->Glyph->Bitmap_Top, Clr 
        
        PenX += Slot->Advance.x Shr 6 
    Next i 
End Function 

sub DrawGlyph(ByVal FontFT As FT_Face, ByVal x As Integer, ByVal y As Integer, ByVal Clr As UInteger)
    Dim BitmapFT As FT_Bitmap 
    Dim BitmapPtr As UByte Ptr 
    Dim DestPtr As UInteger Ptr 
    
    Dim BitmapHgt As Integer 
    Dim BitmapWid As Integer 
    Dim BitmapPitch As Integer 
    
    Dim Src_RB As UInteger 
    Dim Src_G As UInteger 
    Dim Dst_RB As UInteger 
    Dim Dst_G As UInteger 
    Dim Dst_Color As UInteger 
    Dim Alpha As Integer 

    BitmapFT = FontFT->Glyph->Bitmap 
    BitmapPtr = BitmapFT.Buffer 
    BitmapWid = BitmapFT.Width 
    BitmapHgt = BitmapFT.Rows 
    BitmapPitch = 640 - BitmapFT.Width 
    
    DestPtr = Cast(UInteger Ptr, ScreenPtr) + (y * 640) + x 
    
    Do While BitmapHgt 
        Do While BitmapWid 
            ' Thanks, GfxLib 
            Src_RB = Clr And FT_MASK_RB_32 
            Src_G  = Clr And FT_MASK_G_32 

            Dst_Color = *DestPtr 
            Alpha = *BitmapPtr 
            
            Dst_RB = Dst_Color And FT_MASK_RB_32 
            Dst_G  = Dst_Color And FT_MASK_G_32 
            
            Src_RB = ((Src_RB - Dst_RB) * Alpha) Shr 8 
            Src_G  = ((Src_G - Dst_G) * Alpha) Shr 8 
            
            *DestPtr = ((Dst_RB + Src_RB) And FT_MASK_RB_32) Or ((Dst_G + Src_G) And FT_MASK_G_32) 
            
            DestPtr += 1 
            BitmapPtr += 1 
            BitmapWid -= 1 
        Loop 
        
        BitmapWid = BitmapFT.Width 
        BitmapHgt -= 1 
        DestPtr += BitmapPitch 
    Loop 
    
End sub
''
'' End FreeType2 functions
''

' by D.J.Peters (Joshy)
' an put, scale, rotate hackfor the new ImageHeader format.
' MultiPut [destination],[xmidpos],[ymidpos],source,[xScale],[yScale],[Trans]

#define UseRad 'if not then Rotate are in degres

Sub MultiPut(Byval lpTarget As Any Ptr= 0, _
             Byval xMidPos  As Integer= 0, _
             Byval yMidPos  As Integer= 0, _
             Byval lpSource As Any Ptr   , _
             Byval xScale   As Single = 1, _
             Byval yScale   As Single = 1, _
             Byval Rotate   As Single = 0, _
             Byval Trans    As Integer= 0)

  If (screenptr=0) Or (lpSource=0) Then Exit Sub

  If xScale < 0.001 Then xScale=0.001
  If yScale < 0.001 Then yScale=0.001

  Dim As Integer MustLock,MustRotate

  If lpTarget= 0 Then MustLock  =1
  If Rotate  <>0 Then MustRotate=1

  Dim As Integer  TargetWidth,TargetHeight,TargetBytes,TargetPitch
  If MustLock Then
    ScreenInfo    _
    TargetWidth , _
    TargetHeight, _
    TargetBytes ,,_
    TargetPitch
    TargetBytes Shr=3

    lpTarget=ScreenPtr
  Else
    TargetBytes  = cptr(Uinteger Ptr,lpTarget)[1]
    TargetWidth  = cptr(Uinteger Ptr,lpTarget)[2]
    TargetHeight = cptr(Uinteger Ptr,lpTarget)[3]
    TargetPitch  = cptr(Uinteger Ptr,lpTarget)[4]
    lpTarget    += 32
  End If
  If (TargetWidth<4) Or (TargetHeight<4) Then Exit Sub

  Dim As Integer   SourceWidth,SourceHeight,SourceBytes,SourcePitch
  If cptr(Integer Ptr,lpSource)[0] = 7 Then
    SourceBytes  = cptr(Uinteger Ptr,lpSource)[1]
    SourceWidth  = cptr(Uinteger Ptr,lpSource)[2]
    SourceHeight = cptr(Uinteger Ptr,lpSource)[3]
    SourcePitch  = cptr(Uinteger Ptr,lpSource)[4]
    lpSource    += 32
  Else
    SourceBytes  = 1
    SourceWidth  = cptr(Ushort Ptr,lpSource)[0] Shr 3
    SourceHeight = cptr(Ushort Ptr,lpSource)[1]
    SourcePitch  = SourceWidth
    lpSource    += 4
  End If
#if 0
  ? TargetWidth & "x" & TargetHeight & "x" & TargetBytes,TargetPitch
  ? SourceWidth & "x" & SourceHeight & "x" & SourceBytes,SourcePitch
  ? MustLock,Trans
  Sleep:End 
#endif

  If (SourceWidth<2) Or (SourceHeight<2) Then Exit Sub
  If (TargetBytes<>SourceBytes) Then Exit Sub

#define xs 0 'screen
#define ys 1
#define xt 2 'texture
#define yt 3
  Dim As Single Points(3,3)
  points(0,xs)=-SourceWidth/2 * xScale
  points(1,xs)= SourceWidth/2 * xScale
  points(2,xs)= points(1,xs)
  points(3,xs)= points(0,xs)

  points(0,ys)=-SourceHeight/2 * yScale
  points(1,ys)= points(0,ys)
  points(2,ys)= SourceHeight/2 * yScale
  points(3,ys)= points(2,ys)

  points(1,xt)= SourceWidth-1
  points(2,xt)= points(1,xt)
  points(2,yt)= SourceHeight-1
  points(3,yt)= points(2,yt)

  Dim As Uinteger i
  Dim As Single x,y
  If MustRotate Then
    #ifndef UseRad
    Rotate*=0.017453292 'degre 2 rad
    #endif
    While Rotate< 0        :rotate+=6.2831853:Wend
    While Rotate>=6.2831853:rotate-=6.2831853:Wend
    For i=0 To 3
      x=points(i,xs)*Cos(Rotate) - points(i,ys)*Sin(Rotate)
      y=points(i,xs)*Sin(Rotate) + points(i,ys)*Cos(Rotate)
      points(i,xs)=x:points(i,ys)=y
    Next
  End If

  Dim As Integer yStart,yEnd,xStart,xEnd
  yStart=100000:yEnd=-yStart:xStart=yStart:xEnd=yEnd

#define LI 0   'LeftIndex
#define RI 1   'RightIndex
#define  IND 0 'Index
#define NIND 1 'NextIndex
  Dim As Integer CNS(1,1) 'Counters

  For i=0 To 3
    points(i,xs)=Int(points(i,xs)+xMidPos)
    points(i,ys)=Int(points(i,ys)+yMidPos)
    If points(i,ys)<yStart Then yStart=points(i,ys):CNS(LI,IND)=i
    If points(i,ys)>yEnd   Then yEnd  =points(i,ys)
    If points(i,xs)<xStart Then xStart=points(i,xs)
    If points(i,xs)>xEnd   Then xEnd  =points(i,xs)
  Next
  If yStart =yEnd         Then Exit Sub
  If yStart>=TargetHeight Then Exit Sub
  If yEnd   <0            Then Exit Sub
  If xStart = xEnd        Then Exit Sub
  If xStart>=TargetWidth  Then Exit Sub
  If xEnd   <0            Then Exit Sub

  Dim As Ubyte    Ptr t1,s1
  Dim As Ushort   Ptr t2,s2
  Dim As Uinteger Ptr t4,s4


#define ADD 0
#define CMP 1
#define SET 2
  Dim As Integer ACS(1,2) 'add compare and set
  ACS(LI,ADD)=-1:ACS(LI,CMP)=-1:ACS(LI,SET)=3
  ACS(RI,ADD)= 1:ACS(RI,CMP)= 4:ACS(RI,SET)=0

#define EX  0
#define EU  1
#define EV  2
#define EXS 3
#define EUS 4
#define EVS 5
  Dim As Single E(2,6),S(6),Length,uSlope,vSlope
  Dim As Integer U,UV,UA,UN,V,VV,VA,VN

  ' share the same highest point
  CNS(RI,IND)=CNS(LI,IND)
  If MustLock Then ScreenLock
  ' loop from Top to Bottom
  While yStart<yEnd
    'Scan Left and Right sides together
    For i=LI To RI
      ' bad to read but fast and short ;-)
      If yStart=points(CNS(i,IND),ys) Then
        CNS(i,NIND)=CNS(i,IND)+ACS(i,Add)
        If CNS(i,NIND)=ACS(i,CMP) Then CNS(i,NIND)=ACS(i,SET)
        While points(CNS(i,IND),ys) = points(CNS(i,NIND),ys)
          CNS(i, IND)=CNS(i,NIND)
          CNS(i,NIND)=CNS(i, IND)+ACS(i,Add)
          If CNS(i,NIND)=ACS(i,CMP) Then CNS(i,NIND)=ACS(i,SET)
        Wend
        E(i,EX) = points(CNS(i, IND),xs)
        E(i,EU) = points(CNS(i, IND),xt)
        E(i,EV) = points(CNS(i, IND),yt)
        Length  = points(CNS(i,NIND),ys)
        Length -= points(CNS(i, IND),ys)
        If Length <> 0.0 Then
          E(i,EXS) = points(CNS(i, NIND),xs)-E(i,EX):E(i,EXS)/=Length
          E(i,EUS) = points(CNS(i, NIND),xt)-E(i,EU):E(i,EUS)/=Length
          E(i,EVS) = points(CNS(i, NIND),yt)-E(i,EV):E(i,EVS)/=Length
        End If
        CNS(i,IND)=CNS(i,NIND)
      End If
    Next

    If (yStart<0)                              Then Goto SkipScanLine
    xStart=E(LI,EX)+0.5:If xStart>=TargetWidth Then Goto SkipScanLine
    xEnd  =E(RI,EX)-0.5:If xEnd  < 0           Then Goto SkipScanLine
    If (xStart=xEnd)                           Then Goto SkipScanLine
    'if xEnd  <xStart                           then goto SkipScanLine
    Length=xEnd-xStart
    uSlope=E(RI,EU)-E(LI,EU):uSlope/=Length
    vSlope=E(RI,EV)-E(LI,EV):vSlope/=Length
    If xstart<0 Then
      Length=Abs(xStart)
      U=Int(E(LI,EU)+uSlope*Length)
      V=Int(E(LI,EV)+vSlope*Length)
      xStart = 0
    Else
      U=Int(E(LI,EU)):V=Int(E(LI,EV))
    End If
    If xEnd>=TargetWidth Then xEnd=TargetWidth-1
    UV=Int(uSlope):UA=(uSlope-UV)*100000:UN=0
    VV=Int(vSlope):VA=(vSlope-VV)*100000:VN=0
    xEnd-=xStart
    Select Case TargetBytes
      Case 1
        t1=cptr(Ubyte Ptr,lpTarget)
        t1+=yStart*TargetPitch+xStart:xStart=0
        If Trans=0 Then
          While xStart<xEnd
            s1=lpSource+V*SourcePitch+U
            *t1=*s1
            U+=UV:UN+=UA:If UN>=100000 Then U+=1:UN-=100000
            V+=VV:VN+=VA:If VN>=100000 Then V+=1:VN-=100000
            If u<0 Then u=0
            If v<0 Then v=0
            xStart+=1:t1+=1
          Wend
        Else
          While xStart<xEnd
            s1=lpSource+V*SourcePitch+U
            If *s1 Then *t1=*s1
            U+=UV:UN+=UA:If UN>=100000 Then U+=1:UN-=100000
            V+=VV:VN+=VA:If VN>=100000 Then V+=1:VN-=100000
            If u<0 Then u=0
            If v<0 Then v=0
            xStart+=1:t1+=1
          Wend
        End If
      Case 2
        t2=cptr(Short Ptr,lpTarget)
        t2+=yStart*(TargetPitch Shr 1)+xStart:xStart=0
        If Trans=0 Then
          While xStart<xEnd
            s2=cptr(Short Ptr,lpSource)+V*(SourcePitch Shr 1)+U
            *t2=*s2
            U+=UV:UN+=UA:If UN>=100000 Then U+=1:UN-=100000
            V+=VV:VN+=VA:If VN>=100000 Then V+=1:VN-=100000
            If u<0 Then u=0
            If v<0 Then v=0
            xStart+=1:t2+=1
          Wend
        Else
          While xStart<xEnd
            s2=cptr(Short Ptr,lpSource)+V*(SourcePitch Shr 1)+U
            If *s2<>&HF81F Then *t2=*s2
            U+=UV:UN+=UA:If UN>=100000 Then U+=1:UN-=100000
            V+=VV:VN+=VA:If VN>=100000 Then V+=1:VN-=100000
            If u<0 Then u=0
            If v<0 Then v=0
            xStart+=1:t2+=1
          Wend
        End If
      Case 4
        t4=cptr(Integer Ptr,lpTarget)+yStart*(TargetPitch Shr 2)+xStart:xStart=0
        If Trans=0 Then
          While xStart<xEnd
            s4=cptr(Integer Ptr,lpSource)+V*(SourcePitch Shr 2)+U
            *t4=*s4
            U+=UV:UN+=UA:If UN>=100000 Then U+=1:UN-=100000
            V+=VV:VN+=VA:If VN>=100000 Then V+=1:VN-=100000
            If u<0 Then u=0
            If v<0 Then v=0
            xStart+=1:t4+=1
          Wend
        Else
          While xStart<xEnd
            's4=cptr(Integer Ptr,lpSource):s4+=V*(SourcePitch shr 2):s4+=U
            s4=cptr(Integer Ptr,lpSource)+V*(SourcePitch Shr 2)+U
            If *s4<>&HFFFF00FF Then *t4=*s4
            U+=UV:UN+=UA:If UN>=100000 Then U+=1:UN-=100000
            V+=VV:VN+=VA:If VN>=100000 Then V+=1:VN-=100000
            If u<0 Then u=0
            If v<0 Then v=0
            xStart+=1:t4+=1
          Wend
        End If
    End Select

SkipScanLine:
    E(LI,EX)+=E(LI,EXS):E(LI,EU)+=E(LI,EUS):E(LI,EV)+=E(LI,EVS)
    E(RI,EX)+=E(RI,EXS):E(RI,EU)+=E(RI,EUS):E(RI,EV)+=E(RI,EVS)
    yStart+=1:If yStart=TargetHeight Then yStart=yEnd 'exit loop
  Wend
If MustLock Then ScreenUnlock
End Sub

'' End Multiput code
/'
function imageread_jpg( byval buf as any ptr ) as FB.IMAGE ptr
	
	if( buf = NULL ) then
		return NULL
	end if
	
	dim jinfo as jpeg_decompress_struct
	jpeg_create_decompress( @jinfo )
	
	dim jerr as jpeg_error_mgr
	jinfo.err = jpeg_std_error( @jerr )

	jpeg_stdio_src( @jinfo, fp )
	jpeg_read_header( @jinfo, TRUE )
	jpeg_start_decompress( @jinfo )
	
	dim row as JSAMPARRAY
	row = jinfo.mem->alloc_sarray( cast( j_common_ptr, @jinfo ), _
	                               JPOOL_IMAGE, _
	                               jinfo.output_width * jinfo.output_components, _
	                               1 )
	
	dim img as FB.IMAGE ptr
	img = imagecreate( jinfo.output_width, jinfo.output_height )
	
	dim as byte ptr dst = cast( byte ptr, img + 1 )
	
	do while jinfo.output_scanline < jinfo.output_height
		jpeg_read_scanlines( @jinfo, row, 1 )
		
		'' !!!FIXME!!! no grayscale support
		imageconvertrow( *row, 24, dst, bpp, jinfo.output_width )
		
		dst += img->pitch
	loop
	
	jinfo.mem->free_pool( cast( j_common_ptr, @jinfo ), JPOOL_IMAGE )
	
	jpeg_finish_decompress( @jinfo )
	jpeg_destroy_decompress( @jinfo )
	fclose( fp )
	
	function = img
	
end function
'/
''
'' End IJG JPEG functions
''

''
'' Start CPUID Functions.
''

#define true 1
#define false 0

Function ReadEFLAG() as uinteger
  asm pushfd  'eflag on stack
  asm pop eax 'in eax
  asm mov [function],eax 
End Function

Sub WriteEFLAG(byval value as uinteger)
  asm mov eax,[value]
  asm push eax 'value on stack
  asm popfd    'pop in eflag
End Sub

Function IsCPUID() as boolean 'CPUID command available
  
  dim as UInteger old, _new
  old = readeflag() 
  _new = old xor &H200000 'change bit 21
  writeeflag _new
  _new = readeflag()
  if old<>_new then
    function = true
    writeeflag old 'restore old value
  end if
end function

Function IsFPU486() as boolean 'FPU available
  dim tmp as ushort
  tmp = &HFFFF
  asm fninit 'try FPU init
  asm fnstsw [tmp] 'store statusword
  if tmp=0 then 'is it really 0
    asm fnstcw [tmp] 'store control
    if tmp=&H37F then function = true
  end if
end function

Function CPUCounter() as ulongint
  dim tmp as ulongint
  asm RDTSC
  asm mov [tmp],eax
  asm mov [tmp+4],edx
  function = tmp
end function

'' Inspired by the Linux kernel

Function CPUID_EDX(byval funcnr as uinteger) as uinteger
  asm mov dword ptr eax,[funcnr]
  asm cpuid
  asm mov [function],edx
end Function

Function CPUID_ECX(byval funcnr as uinteger) as uinteger
  asm mov dword ptr eax,[funcnr]
  asm cpuid
  asm mov [function],ecx
end Function

Function CPUID_EBX(byval funcnr as uinteger) as uinteger
  asm mov dword ptr eax,[funcnr]
  asm cpuid
  asm mov [function],ebx
end Function

Function CPUID_EAX(byval funcnr as uinteger) as uinteger
  asm mov dword ptr eax,[funcnr]
  asm cpuid
  asm mov [function],eax
end Function

Sub CPUID(byval funcnr as UInteger, ByVal ra As Any Ptr, ByVal rb As Any Ptr, ByVal rc As Any Ptr, ByVal rd As Any Ptr) 
   Asm 
      mov dword ptr eax,[funcnr]
      cpuid
      mov [ra],eax
      mov [rb],ebx
      mov [rc],ecx
      mov [rd],edx     
   End Asm
End Sub

Function GetCPUVendor() As String
   Dim vendor As ZString * 12 
   Asm
      mov eax, 0x0
      cpuid
      mov [vendor+0], ebx
      mov [vendor+4], edx
      mov [vendor+8], ecx
   End Asm
   Return vendor
End Function

Function GetCPUName() As String
   Dim cpu As ZString * 48
   Asm
      mov eax, 0x80000002
      cpuid
      mov [cpu+0], eax
      mov [cpu+4], ebx
      mov [cpu+8], ecx
      mov [cpu+12], edx
      mov eax, 0x80000003
      cpuid
      mov [cpu+16], eax
      mov [cpu+20], ebx
      mov [cpu+24], ecx
      mov [cpu+28], edx
      mov eax, 0x80000004
      cpuid
      mov [cpu+32], eax
      mov [cpu+36], ebx
      mov [cpu+40], ecx
      mov [cpu+44], edx      
   End Asm
   Return cpu
End Function

Function IsSSE2() As Boolean
   dim retvalue as uinteger
   retvalue = CPUID_EDX(1)
   If (retvalue And &H4000000) Then Return TRUE Else Return False
End Function

Function IsSSE() As Boolean
   dim retvalue as uinteger
   retvalue = CPUID_EDX(1)
   If (retvalue And &H2000000) Then Return TRUE Else Return False
End Function

Function GetMHz() As Integer
   Dim As UInteger retvalue, flag
   Dim As Double a, Start
      
   retvalue=CPUID_EDX(1)
   if (retvalue and &H10) then 
      flag=true
   end If
   
   if flag=true then
      a=CPUCounter():a-=8500000
      Start =timer()
      while (timer()-start)<1.0:wend
      a=CPUCounter()-a
      a\=1000000
      Return a
   end If
   Return 0
End Function

''
'' End CPUID Functions.
''

function getmp3name( byval stream as FSOUND_STREAM ptr ) as string
	dim tagname as zstring ptr, taglen as integer

	FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "TIT2", @tagname, @taglen )
	tagname += 1 
	If( taglen = 0 ) then 
		FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V1, "TITLE", @tagname, @taglen )
	End if
	printf(!"getmp3name(): tagname = \"%s\" taglen = %d\n", tagname, taglen)
	getmp3name = left( *tagname, taglen )
end function

'':::::
function getmp3artist( byval stream as FSOUND_STREAM ptr ) as string
	dim tagname as zstring ptr, taglen as integer
	FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "TPE1", @tagname, @taglen )
	tagname += 1
	If( taglen = 0 ) then 
		FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V1, "ARTIST", @tagname, @taglen )
        	if( taglen = 0 ) then 
			FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ASF, "WM/AlbumArtist", @tagname, @taglen )
        	end if
	End if
	printf(!"getmp3artist(): tagname = \"%s\" taglen = %d\n", tagname, taglen)
	getmp3artist = left( *tagname, taglen )
end function

'':::::
function getmp3album( byval stream as FSOUND_STREAM ptr ) as string
	dim tagname as zstring ptr, taglen as integer
   
   FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "TALB", @tagname, @taglen )
   tagname += 1
	If( taglen = 0 ) then 
		 FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V1, "ALBUM", @tagname, @taglen )
         if( taglen = 0 ) then 
             'Assume at this point we're decoding Windows Media Audio...
             FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ASF, "WM/AlbumTitle", @tagname, @taglen )
         End If
	End if
   printf(!"getmp3album(): tagname = \"%s\" taglen = %d\n", tagname, taglen)
	getmp3album = left( *tagname, taglen ) 
end function

function getmp3albumart( byval stream as FSOUND_STREAM ptr ) as Any Ptr
	dim imgdata As Any Ptr, buflen As Integer
	Dim console As Integer
	Dim ret As Boolean
	console = FreeFile()
	Open Err As #console
   ret = FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "APIC", @imgdata, @buflen )
   
   If ret = FALSE Then
      Print #console, "getmp3albumart(): FindTagField(APIC) FAILED!"
      Close #console
      Return NULL
   Else
      Print #console, "getmp3albumart(): FindTagField(APIC) succeeded."
      Close #console
      If StrnCmp(imgdata+1,"image/jpeg", 10) = 0 Then
         Var imgbuf = imgdata + 20
         Var imglen = buflen - 20 
      EndIf
      Dim fp As FILE Ptr
      'fp = fopen("crack.kills.jpg","w+")
      'fwrite(imgdata,buflen,1,fp)
      'fclose(fp)
      Return NULL 
   EndIf
end Function

#ifdef __FB_WIN32__
function file_getname( byval hWnd as HWND ) as string

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
		.lpfnHook			= NULL
		.lpTemplateName		= NULL
	end With
	
	if( GetOpenFileName( @ofn ) = FALSE ) then
		return ""
	else
		return filename
	end If
	
end function

sub MsgBox(hWnd As HWND, msg As String)
    MessageBox(hWnd, msg, "PsyMP3 Player (pid: " & getpid() & ")", 0)
End Sub
#else 
sub TTY_MsgBox(hWnd As Integer, msg As String)
    ' simply print to the console until I find a method of creating 
    ' a X11 window which is modal to it's parent.
    Dim console As Integer
    console = FreeFile()
    Open Err As #console
    Print #console, "PsyMP3 [" & getpid() & "] (XID: 0x" + Hex(hWnd, 8) + "): " + msg
End Sub

#inclib "ui"
Extern "C++"
	Declare Function getFile Alias "getFile" () As ZString Ptr
	Declare Sub libui_init Alias "libui_init" (ByVal argc As Integer, ByVal argv As Zstring Ptr Ptr) 
End Extern
Function file_getname (ByVal hWnd as integer) As String
	Dim ret As String
	Dim path As String
	ret = *getFile()
	Function = ret
	path = *dirname(ret)
	chdir(path)
End Function

#EndIf

Sub DrawSpectrum(spectrum As Single Ptr)
   Dim X As Integer
	For X = 0 to 320
      line(x*2,350-(spectrum[x]*350))-(x*2+1,350), _
			iif(x>200,rgb((x-200)*2.15,0,255),iif(x<100, _
			rgb(128,255,x*2.55),rgb(128-((x-100)*1.28),255 _
			 - ((x-100)*2.55),255))), bf
	Next X
End Sub

Sub EndPlayer()
	#ifdef __FB_LINUX__
		' kill_(getpid(),SIGINT)
		end
	#ElseIf Defined(__FB_WIN32__)
		DestroyWindow(WAWindow)
		End
	#Else 
		End
	#endif
End Sub

#ifdef __FB_WIN32__
Dim Shared As HWND hWnd
#else 
Dim Shared As Integer hWnd
#EndIf

Sub ShowAbout(ByVal T As Any Ptr)
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
	   !"Dedicated to my friends: Melissa Pegley, Ayana Smith, Justin, Mike, Nick Smith,\n" & _
	   !"Deandre and Warren Ervin, Micheal and Brittany Stearns, Virginia Wood," & _  
	   !"and everyone else I forgot!")
End Sub

Sub InitFMOD() 
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

#Ifdef __FB_WIN32__

'
' This emulates the interface of Winamp
'
#define IPC_PLAYFILE 100

#define KVIRC_WM_USER 63112

#define KVIRC_WM_USER_CHECK 13123
#define KVIRC_WM_USER_CHECK_REPLY 13124
#define KVIRC_WM_USER_GETTITLE 5000
#define KVIRC_WM_USER_GETFILE 10000
#define KVIRC_WM_USER_TRANSFER 15000

Function WAIntProc StdCall(hWnd As HWND, uMsg As UINT, wParam As WPARAM, lParam As LPARAM) As LRESULT
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
						buf = mp3artist + " - " + mp3name
						Return Len(buf)
					Case KVIRC_WM_USER_GETFILE
						lastcall = KVIRC_WM_USER_GETTITLE
						' TODO: Sanitize output
						Dim As Integer i
						buf = ""
						For i = 0 To Len(mp3file) - 1
							Select Case mp3file[i]
								Case 32
									buf &= "%20"
								Case Asc("\")
									buf &= "/"
								Case Asc("[")
									buf &= "%5b"
								Case Asc("]")
									buf &= "%5d"
								Case Else
									buf &= Chr(mp3file[i])
							End Select
						Next
						'buf = mp3file
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
				Case 125 ' IPC_GETLISTPOS
					Return Songlist.getPosition()
				Case 124 ' IPC_GETLISTLENGTH
					Return Songlist.getEntries()
				Case 122 ' IPC_SETVOLUME
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
				Case 251 ' IPC_GET_REPEAT
					Return doRepeat
				Case 253 ' IPC_SET_REPEAT
					If wParam = 0 Or wParam = 1 Then doRepeat = wParam
				Case 260 ' IPC_GETWND
					' We don't support this for obvious reasons.
					Return 0
				Case 211 ' IPC_GETPLAYLISTFILE
					Return StrPtr(mp3file)
				Case 214 ' IPC_GETPLAYLISTFILEW
					Return StrPtr(wmp3file)
				Case 212 ' IPC_GETPLAYLISTTITLE
					' I don't like this, it requires I return a pointer inside
					' PsyMP3's memory space.
					wintitle = mp3artist + " - " + mp3name
					Return StrPtr(wintitle)
				Case 104 ' IPC_ISPLAYING
					Select Case As Const isPaused
						Case 1
							Return 3
						Case 0
							Return 1
						Case 2
							Return 0
					End Select
				Case 501 ' IPC_IS_PLAYING_VIDEO
					Return 0 ' 0 means not playing video.
				Case 2000 To 3000 ' Freeform message. We aren't really Winamp, return 0
					Return 0
				Case 3026 ' WARNING: Satanic rituals at work here.
					Dim As extendedFileInfoStructW Ptr efis = wParam
					If *efis->filename <> wmp3file Then Return 0 
					Select Case LCase(efis->metadata)
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
			Return 0
		Case 12 To 14' I dunno what this is, but it's used by SetWindowText()
			Return DefWindowProc(hWnd, uMsg, wParam, lParam)
		Case Else
			Printf(!"hwnd = %#x, umsg = %d, wParam = %#x, lParam = %#x\n",hWnd, uMsg, wParam, lParam)
			Return DefWindowProc(hWnd, uMsg, wParam, lParam)
	End Select
End Function

Sub InitKVIrcWinampInterface Alias "InitKVIrcWinampInterface" ()
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

Sub ClearWMP()
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

Sub AnnounceWMP(artist As String, Title As String, Album As String)
	Dim cpd As CopyData
	Dim msgr As HWND
	Dim Found As Integer
	Dim wmsg As WString * 500
	Dim As WString * 30 WMContentID = "WMContentID"
	Dim As WString * 500 MSNMusicString = !"PsyMP3\\0Music\\0%d\\0%s\\0%s\\0%s\\0%s\\0%s\\0"
	Dim As WString * 100 FormatStr = "PsyMP3: {1} - {0}"

	Dim As WString * 500 WTitle, WArtist, WAlbum
	MultiByteToWideChar(CP_UTF8, 0, StrPtr(artist), Len(artist), WArtist, Len(artist))
	MultiByteToWideChar(CP_UTF8, 0, StrPtr(Title), Len(Title), WTitle, Len(Title))
	MultiByteToWideChar(CP_UTF8, 0, StrPtr(Album), Len(Album), WAlbum, Len(Album))
	wsprintfW(wmsg, MSNMusicString, 1, FormatStr, WTitle, WArtist, WAlbum, WMContentID) 
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
#endif

#endif

Sub parse_m3u_playlist(m3u_file As String)
	Dim As Integer fd = FreeFile()
	Dim As String text
	Dim As Integer ret
	ret = Open (m3u_file, For Input, As #fd)
	If ret <> 0 Then Return
	Do
		Line Input #fd, text
		If Left(text,1) <> "#" Then
			#Ifdef __FB_WIN32__
			If Mid(text,2,2) = ":\" Then
			#Else
			If text[0] = Asc("/") Then
			#EndIf
				Songlist.addFile(text)
			Else
				Songlist.addFile(*Cast(ZString Ptr,dirname(m3u_file)) + "/" + text)
			End If
		End If
	Loop While Eof(fd) = 0
	Close #fd
End Sub

Function MD5str (chars As String) As String
	Dim md5hex As ZString * 32
	Dim digest As ZString * 16
	Dim state As md5_state_s
	Dim i As Integer

	md5_init(@state)
	md5_append(@state, Cast(md5_byte_t Ptr, StrPtr(chars)), Len(chars))
	md5_finish(@state, @digest)
	
	For i = 0 To 15
		sprintf(@md5hex _ 
		+ (i * 2), _
		"%02x", _
		digest[i])
	Next i
	Return md5hex
End Function

Function percent_encode(message As String) As String
	Dim ret As String
	Dim i As Integer
	For i = 0 to Len(message) - 1
	Select Case(message[i])
		Case &h0 To &h2c, &h2f, &h3a, &h3b, &h3d, &h3f, &h40, &h5b, &h5d, &h80 to &hff
		ret &= "%" & Hex(message[i])
		Case Else
		ret &= Chr(message[i])
	End Select
	Next
	Return ret
End Function

Function lastfm_session() As String
	Dim As Integer curtime = time_(NULL)
	Dim As String authkey = MD5str(MD5str(lastfm_password) & curtime)
	Dim As ZString * 10000 response
	Dim As String response_data, httpdata

   If lastfm_username = "" Or Lastfm_password = "" Then Return ""

   printf(!"Last.fm: Getting session key.\n")

	httpdata = "GET /?hs=true&p=1.2.1&c=psy&v="PSYMP3_VERSION"&u=" & lastfm_username & "&t=" & curtime & "&a=" & authkey & " HTTP/1.1" & Chr(10) & "Host: post.audioscrobbler.com" & Chr(10) & "User-Agent: PsyMP3/1.1beta" & Chr(10) & Chr(10)
	hStart()
	Dim s As SOCKET, addr As Integer
	s = hOpen()
	addr = hResolve("post.audioscrobbler.com")
	hConnect(s, addr, 80)
	hSend(s, strptr(httpdata), len(httpdata))
	hReceive(s, strptr(response), 10000)
	hClose(s)
	
	response_data = Mid(response, InStr(response, !"\r\n\r\n") + 4)
	
	If left(response_data,3) = !"OK\n" Then
		printf(!"Last.fm: Session key retreived: %s.\n", Mid(response_data, 4, 32))
		Function = Mid(response_data, 4, 32)
	Else
		printf(!"Last.fm: Failed to authenticate (bad password?)\n")
		Function = ""
	End If
End Function

Function Lastfm_nowplaying() As SOCKET
	Dim As Integer curtime = time_(NULL)
	Dim As String authkey = MD5str(MD5str(lastfm_password) & curtime)
	Dim As ZString * 10000 response
	Dim As String response_data, httpdata, postdata
   
   Dim As Integer length = Int(FSOUND_Stream_GetLengthMs(stream)/1000)

	httpdata = 	!"POST /np_1.2 HTTP/1.1\n" & _
					!"Host: post.audioscrobbler.com\n" & _ 
					!"User-Agent: PsyMP3/"PSYMP3_VERSION"\n" 

	postdata = 	"s=" & lastfm_sessionkey & "&" & _ 
					"a=" & percent_encode(mp3artist) & "&" & _
					"t=" & percent_encode(mp3name) & "&" & _ 
					"b=" & percent_encode(mp3album) & "&" & _
					"l=" & length & "&" & _
					"n=&m="

	httpdata &= !"Content-Length: " & Len(postdata) & !"\n" & _
					!"Content-Type: application/x-www-form-urlencoded\n\n" & _
					postdata


	Dim s As SOCKET, addr As Integer
	s = hOpen()
	addr = hResolve("post.audioscrobbler.com")
	hConnect(s, addr, 80)
	hSend(s, strptr(httpdata), len(httpdata))
	hReceive(s, strptr(response), 10000)
   response_data = Mid(response, InStr(response, !"\r\n\r\n") + 4)

   printf(!"Lastfm_nowplaying(): Response_data = %s\n", response_data)


   if left(response_data,3) = !"OK\n" Then
	   printf !"Last.fm: Server response OK. Track sucessfully sent as nowplaying!\n"
   Else 
      printf !"Last.fm: Sending track as nowplaying failed!\n"
   End If 
Return(s)
End Function

Function lastfm_scrobble() As Integer
	Dim As Integer curtime = time_(NULL)
	Dim As String authkey = MD5str(MD5str(lastfm_password) & curtime)
	Dim As ZString * 10000 response
	Dim As String response_data, httpdata, postdata

   Dim As Integer qual

   ' IIf keeps giving me "Invalid data types" so go with a more verbose workaround
   If (songlength / 4000) > 240 Then
      qual = 240
   Else
      qual = songlength / 4000
   EndIf
   /'
   qual = IIf( _ 
      Int(songlength/4000) > 240, _ 
      240, _ 
      songlength/4000 _
   )
   '/


   If songlength < 30000 Then Return 0
   
   If (curtime - songstart) < qual Then Return 0
	
	printf(!"Last.fm: Going to scrobble track. Artist: \"%s\", Title: \"%s\", Album: \"%s\".\n", mp3artist, mp3name, mp3album)
	
   httpdata = 	!"POST /protocol_1.2 HTTP/1.1\n" & _
		!"Host: post2.audioscrobbler.com\n" & _ 
		!"User-Agent: PsyMP3/"PSYMP3_VERSION"\n" 
 
postdata = 	"s=" & lastfm_sessionkey & "&" & _ 
				"a[0]=" & percent_encode(mp3artist) & "&" & _
				"t[0]=" & percent_encode(mp3name) & "&" & _ 
				"i[0]=" & time_(NULL) & "&" & _ 
				"o[0]=P&" & _ 
				"r[0]=&" & _ 
				"l[0]=" & Int(FSOUND_Stream_GetLengthMs(stream)/1000) & "&" & _
				"b[0]=" & percent_encode(mp3album) & "&" & _
				"n[0]=&m[0]="

httpdata &= _
		!"Content-Length: " & Len(postdata) & !"\n" & _
		!"Content-Type: application/x-www-form-urlencoded\n\n" & _
		postdata

Dim s As SOCKET
Dim addr As Integer
s = hOpen()
addr = hResolve("post2.audioscrobbler.com")
hConnect(s, addr, 80)

hSend(s, strptr(httpdata), len(httpdata))
hReceive(s, strptr(response), 10000)
response_data = Mid(response, InStr(response, !"\r\n\r\n") + 4)
if left(response_data,3) = !"OK\n" Then
	printf !"Last.fm: Server response OK. Track sucessfully scrobbled!\n"
End If 
End Function

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
Dim X As Integer
Dim Y As Integer
Dim T As Integer
Dim dectBeat As Integer
Dim Action as String
Dim As Integer posx, posy, buttons, mousex, mousey, _
	   oldposx, oldposy, oldmousex, oldmousey
Dim FPS(2) As Double
Dim As String cpuvendor, cpuname
Dim As Integer doPrev
olddir = CurDir

cpuvendor = GetCPUVendor()
cpuname = GetCPUName()

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
      EndIf
   EndIf
#Else
   printf(!"Compiled without SSE support.\n")
#endif

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

Dim fd As Integer = FreeFile()
Open "lastfm_config.txt" For Input As #fd
Line Input #fd, lastfm_username
Line Input #fd, lastfm_password
Close #fd

printf(!"Last.fm username: %s. password: %s.\n", lastfm_username, String(Len(lastfm_password), "*"))
lastfm_sessionkey = lastfm_session()
If lastfm_sessionkey <> "" Then
   printf(!"Last.fm login successful!\n")
EndIf

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
#endif

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

Do
   mp3file = file_getname(hWnd)
   If mp3file = "" Then
      If Songlist.getEntries() = 0 Then 
         MsgBox hWnd, "No mp3 selected, exiting..."
         End
      Else
         Exit Do
      EndIf   
   EndIf
   If LCase(Right(mp3file,4)) = ".m3u" Then
      parse_m3u_playlist(mp3file)
   Else
      Songlist.addFile(mp3file)
   EndIf
Loop 
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
#ifdef __FB_WIN32__
InitKVIrcWinampInterface() 
 
#endif
mp3name = getmp3name(stream)
mp3artist = getmp3artist(stream)
mp3album = getmp3album(stream)
getmp3albumart(stream)
var blank = ImageCreate(640, 350, rgba(0, 0, 0, 64), 32)
If isSilent <> 1 Then
   WindowTitle "PsyMP3 " & PSYMP3_VERSION & " - Playing - -:[ " + mp3artist + " ]:- -- -:[ " + mp3name + " ]:-"
   ScreenSet 1
   DoFPS = 0
   spectrum = FSOUND_DSP_GetSpectrum()
End If
FSOUND_Stream_Play( FSOUND_FREE, stream )
songlength = FSOUND_Stream_GetLengthMs(stream)
Do
   IsPaused = 0
   #Ifdef __FB_WIN32__
		ClearWMP()
		AnnounceWMP(mp3artist, mp3name, mp3album)
   #EndIf
   If sock <> 0 Then
      hClose(sock)
   EndIf
   sock = Lastfm_nowplaying()
Do  
#ifdef __FB_WIN32__     
   SetWindowText(WAWindow, _
		Songlist.getPosition & ". " + mp3artist + " - " + mp3name + " - Winamp")

	Sleep 12 ' Timer delay
#endif

If IsSilent <> 1 Then
	If SpectrumOn = 1 Then      
		DrawSpectrum(spectrum)       
	End If

   'ScreenUnlock
   'Window title, is program name/version, artist and songname, and 
   'time elapsed/time remaining
	FPS(2) = Timer
	If DoFPS = 1 Then
		Var szFPS = Format(1000 / ((FPS(2) - FPS(1)) * 1000), "#.0 FPS")
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
   PrintFT(1,366,"Artist: " + mp3artist,sFont,12,rgb(255,255,255))
   PrintFT(1,381,"Title: " + mp3name,sFont,12)
   PrintFT(1,396,"Album: " + mp3album,sFont,12) 
   PrintFT(300,366,"Playlist: " & Songlist.getPosition & "/" & Songlist.getEntries, sFont, 12, rgb(255,255,255)) 
   PrintFT(280,396,"CPU: " + cpuname + ", Vendor: " + cpuvendor,sFont,9)
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
#ifdef __FBGL__
	win.color = iif(x>146,rgb((x-146)*3.5,0,255), _
					IIf(x<73, rgb(128,255,x*3.5), _
					RGB(128-((x-73)*1.75),255-((x-73)*3.5),255)))
	win.line (400+x,373,400+x,382)
#Else
  'original
  'Line(400+x,173)-(400+x,182), _
  '  Iif(x>146, _
  '    Rgb((x-146)*3.5,0,255), _
  '  Iif(x<73, _
  '    Rgb(128,255,x*3.5), _
  '    Rgb(128-((x-73)*1.75),255-((x-73)*3.5),255))), bf


	Line(400+x,373)-Step(0,9), _
		RGB( _
			-128*(x<=146)+(x-73)*1.75*(x>=73 And x<=146)-(x-146)*3.5*(x>146), _
			-255*(x<=146)+(x-73)*3.5*(x>=73 And x<=146), _
			-255*(x>=73)-x*3.5*(x<73) _
		)
#endif
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
		Else
			IsPaused = 1
		End If
		FSOUND_SetPaused(FSOUND_ALL, IsPaused)
   End If
   If doCommand = PSYMP3_PAUSE And IsPaused = 1 Then
      IsPaused = 0
      doCommand = PSYMP3_PLAY
   EndIf
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
		EndIf 
		If LCase(nkey) = "b" Then
			If doRepeat = 1 Then
				doRepeat = 0 
			Else 
				doRepeat = 1
			EndIf
		EndIf
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
			lastfm_scrobble()
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
			'getmp3albumart(stream)
			FSOUND_Stream_Play( FSOUND_FREE, stream )
			songlength = FSOUND_Stream_GetLengthMs(stream)
			IsPaused = 0
#ifdef __FB_WIN32__
			ClearWMP()
			AnnounceWMP(mp3artist, mp3name, mp3album)
#EndIf
			hClose(sock)
			sock = Lastfm_nowplaying()
		End If
	End If
	' Append song or m3u to playlist.
	If LCase(nkey) = "i" Then
		mp3file = file_getname(hWnd)
		If mp3file <> "" Then
			If LCase(Right(mp3file,4)) = ".m3u" Then
				parse_m3u_playlist(mp3file)
			ElseIf LCase(Right(mp3file,5)) = ".m3u8" Then
				' parse_m3u8_playlist(mp3file)
			Else
				Songlist.addFile(mp3file)
			EndIf
		End If
	End If
	' Dump playlist. DISABLED for 1.1-RELEASE due to buggyiness.
   /'	
	If LCase(nkey) = "o" Then
		PrintFT(30,130, "Saving playlist to " & CurDir() & "PsyMP3-" & time_(NULL) & ".m3u", sFont, 32, RGB(255,255,255))
		PCopy 1,0
		Songlist.savePlaylist("PsyMP3-" & time_(NULL) & ".m3u")
	EndIf
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
	EndIf
	If doCommand = PSYMP3_PLAY_NEXT Then
		doCommand = PSYMP3_PLAY
		Exit Do
	EndIf
	If doCommand = PSYMP3_PLAY_PREV Then
		doCommand = PSYMP3_PLAY
		If Songlist.getPosition() > 1 Then
			doPrev = 1
			Exit Do
		End If
	EndIf 
	If LCase(nkey) = "p" Then
		If Songlist.getPosition() > 1 Then
			doPrev = 1
			Exit Do
		End If
	EndIf
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
		sock = Lastfm_nowplaying()
   EndIf
End IF
Loop While (FSOUND_Stream_GetTime(stream) <> FSOUND_Stream_GetLengthMs(stream)) Or (doRepeat = 1)
' I don't want to quit at end-of-song anymore.
FSOUND_Stream_Stop(stream)
FSOUND_Stream_Close(stream)
lastfm_scrobble()
#ifdef __FB_WIN32__
	ClearWMP()
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
		parse_m3u_playlist(mp3file)
		mp3file = Songlist.getNextFile()
		printf(!"File to open: \"%s\"\n", mp3file)
		stream = FSOUND_Stream_Open( StrPtr(mp3file), FSOUND_MPEGACCURATE, 0, 0 )
		songstart = Time_(NULL)
		If( stream = 0 ) then 
			MsgBox hWnd, !"Can't load music file \"" + mp3file + !"\""
			end 1
		end if
		mp3name = getmp3name(stream)
		mp3artist = getmp3artist(stream)
		mp3album = getmp3album(stream)
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
	end if
	mp3name = getmp3name(stream)
	mp3artist = getmp3artist(stream)
	mp3album = getmp3album(stream)
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
#endif
EndPlayer()
