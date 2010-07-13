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

#Define PSYMP3_CORE_GLOBALS
	#Include "psymp3.bi"

Constructor LastFM() Export
	this.readConfig()
	this.loadScrobbles()
	printf(!"LastFM::LastFM(): username: %s, password: %s.\n", this.c_username, String(Len(this.c_password), "*")) 
	this.m_session = this.getSessionKey()
	'Lastfm_sessionkey = this.m_session
	printf(!"LastFM::LastFM(): Last.fm login successful!\n")
	this.c_xmlpath = CurDir()
	printf(!"LastFM::LastFM(): last.fm directory: %s\n", this.c_xmlpath)
End Constructor

Destructor LastFM() Export
	printf(!"LastFM::~LastFM(): Dumping scrobbles\n")
	this.dumpScrobbles()
End Destructor

Sub LastFM.readConfig Alias "readConfig" () Export
	Dim fd As Integer = FreeFile()
	Open "lastfm_config.txt" For Input As #fd
	Line Input #fd, this.c_username
	Line Input #fd, this.c_password
	Close #fd
End Sub

Function LastFM.getSessionKey Alias "getSessionKey" () As String Export
	Dim As Integer curtime = time_(NULL)
	Dim As String authkey = MD5str(MD5str(this.c_password) & curtime)
	Dim As ZString * 10000 response
	Dim As String response_data, httpdata

	If this.c_username = "" Or this.c_password = "" Then Return ""
   
	printf(!"LastFM::getSessionKey(): Getting session key.\n")

	httpdata = "GET /?hs=true&p=1.2.1&c=psy&v=" & PSYMP3_VERSION & "&u=" & this.c_username & "&t=" & curtime & "&a=" & authkey & " HTTP/1.1" & Chr(10) & "Host: post.audioscrobbler.com" & Chr(13) & Chr(10) & "User-Agent: PsyMP3/" & PSYMP3_VERSION & Chr(13) & Chr(10) & Chr (13) & Chr(10)
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
		printf(!"LastFM::getSessionKey(): Session key retreived:%s\n", Mid(response_data, 4, 32))
		Function = Mid(response_data, 4, 32)
		response_data = Mid(response_data, InStr(response_data, !"\n") + 1)
		response_data = Mid(response_data, InStr(response_data, !"\n") + 1)
		Dim As String surl(2)
		Dim As String buf, host, port, path
		For curtime = 1 To 2
			surl(curtime) = Left(response_data, InStr(response_data, !"\n") - 1)
			host = Mid(response_data, InStr(response_data, !"http://") + 7)
			buf = host
			host = Left(host, InStr(host, !":") - 1)
			port = Mid(buf, InStr(buf, !":") + 1)
			buf = port
			port = Left(port, InStr(buf, "/") - 1)
			path = Mid(buf, InStr(buf, "/"))
			path = Left(path, InStr(path, !"\n") - 1)
			this.c_apihost(curtime) = host
			this.c_apiport(curtime) = Val(port)
			this.c_apipath(curtime) = path
			response_data = Mid(response_data, InStr(response_data, !"\n") + 1)
		Next curtime
	Else
		printf(!"LastFM::getSessionKey(): Failed to authenticate (bad password?)\n")
		Function = ""
	End If   
End Function

Function LastFM.setNowPlaying Alias "setNowPlaying" () As SOCKET Export
	Dim As Integer curtime = time_(NULL)
	Dim As String authkey = MD5str(MD5str(this.c_password) & curtime)
	Dim As ZString * 10000 response
	Dim As String response_data, httpdata, postdata
   
   Dim As Integer length = Int(FSOUND_Stream_GetLengthMs(stream)/1000)

	If this.m_session = "" Then this.m_session = this.getSessionKey() 

	Dim As Boolean submitted = FALSE 
	
	While submitted = FALSE 

		httpdata = 	"POST " & this.c_apipath(1) & !" HTTP/1.1\n" & _
						"Host: " & this.c_apihost(1) & !"\n" & _ 
						"User-Agent: PsyMP3/" & PSYMP3_VERSION & !"\n"
	
		postdata = 	"s=" & this.m_session & "&" & _ 
						"a=" & percent_encodeW(mp3artistW) & "&" & _
						"t=" & percent_encodeW(mp3nameW) & "&" & _ 
						"b=" & percent_encodeW(mp3albumW) & "&" & _
						"l=" & length & "&" & _
						"n=&m="

		httpdata &= !"Content-Length: " & Len(postdata) & !"\n" & _
						!"Content-Type: application/x-www-form-urlencoded\n\n" & _
						postdata
	
	
		Dim s As SOCKET = this.submitData(httpdata, 1)
		hReceive(s, strptr(response), 10000)
		response_data = Mid(response, InStr(response, !"\r\n\r\n") + 4)
		Dim status As String = Left(response_data, InStr(response_data, !"\n") - 1)
	
		Select Case status
			Case "OK"
				printf !"LastFM::setNowPlaying(): Server response OK. Track sucessfully sent as nowplaying!\n"
				this.submitSavedScrobbles()
				submitted = TRUE
			Case "BADSESSION"
				this.m_session = this.getSessionKey()  
			Case Else
				printf !"LastFM::setNowPlaying(): Sending track as nowplaying failed\n"
				submitted = TRUE
		End Select
	Wend
End Function

Function LastFM.scrobbleTrack Alias "scrobbleTrack" () As Integer Export
	Dim As Integer curtime = time_(NULL)
	Dim As Integer length = Int( FSOUND_Stream_GetLengthMs(stream) /1000) 
	
	Dim As String artist, Title, album
	Artist = percent_encodeW(mp3artistW)
	Title = percent_encodeW(mp3nameW)
	Album = percent_encodeW(mp3albumW)
	
	Dim As Integer qual
	
	' If this fails, just forget it and store the scrobble.
	If this.m_session = "" Then this.m_session = this.getSessionKey() 
	
	' IIf keeps giving me "Invalid data types" so go with a more verbose workaround
	If (songlength / 4000) > 240 Then
		qual = 240
	Else
		qual = songlength / 4000
	EndIf
	
	If songlength < 30000 Then Return 0
	
	If (curtime - songstart) < qual Then Return 0
	
	curtime = this.submitScrobble(Artist, Title, Album, Length, curtime)
	If curtime = TRUE Then 
		this.submitSavedScrobbles()
	EndIf
End Function

Function LastFM.submitData Alias "submitData" (sData As String, host As Integer) As SOCKET
	Dim rhost As String
	Dim rport As Integer
	If this.c_apihost(host) <> "" Then rhost = this.c_apihost(host) Else rhost = "post.audioscrobbler.com"
	If this.c_apiport(host) <> 0 Then rport = this.c_apiport(host) Else rport = 80
	Dim s As SOCKET, addr As Integer
	s = hOpen()
	printf !"LastFM::submitData(): host %s port %d\n", rhost, rport
	addr = hResolve(rhost)
	hConnect(s, addr, rport)
	hSend(s, strptr(sData), len(sData))
	Return(s)
End Function

Function LastFM.submitScrobble Alias "submitScrobble" (artist As String, Title As String, album As String, length As Integer, curtime As UInteger) As Integer Export 
	Dim As Boolean submitted = FALSE
	Dim As ZString * 10000 response
	Dim As String response_data, httpdata, postdata
	
	While submitted = FALSE 
	
		httpdata = 	"POST " & this.c_apipath(2) & !" HTTP/1.1\n" & _
			"Host: " &this.c_apihost(2) & !"\n" & _ 
			"User-Agent: PsyMP3/" & PSYMP3_VERSION & !"\n" 
	
		postdata = 	"s=" & this.m_session & "&" & _ 
						"a[0]=" & Artist & "&" & _
						"t[0]=" & Title & "&" & _ 
						"i[0]=" & curtime & "&" & _ 
						"o[0]=P&" & _ 
						"r[0]=&" & _ 
						"l[0]=" & length & "&" & _
						"b[0]=" & Album & "&" & _
						"n[0]=&m[0]="

		httpdata &= _
				!"Content-Length: " & Len(postdata) & !"\n" & _
				!"Content-Type: application/x-www-form-urlencoded\n\n" & _
				postdata

		Dim s As SOCKET = this.submitData(httpdata, 2)
		hReceive(s, strptr(response), 10000)
		response_data = Mid(response, InStr(response, !"\r\n\r\n") + 4)

		Dim status As String = Left(response_data, InStr(response_data, !"\n") - 1)

		Select Case status
			Case "OK"
				printf !"LastFM::submitScrobble(): Server response OK. Track sucessfully scrobbled!\n"
				submitted = TRUE
			Case "BADSESSION"
				this.m_session = this.getSessionKey()  
			Case Else
				printf !"LastFM::submitScrobble(): Scrobbling track failed!\n"
				this.saveScrobble(artist, Title, album , length, curtime)
				submitted = TRUE
		End Select
	Wend
End Function

Sub LastFM.submitSavedScrobbles Alias "submitSavedScrobbles" ()
	Dim s As Scrobble Ptr
	If This.m_scrobbles.Empty() Then Return
	While(This.m_scrobbles.Empty() = 0)
		s = This.m_scrobbles.Front()
		this.submitScrobble(s->m_artist, s->m_title, s->m_album, s->m_length, s->m_curtime)
		This.m_scrobbles.Pop()
	Wend
End Sub

Function LastFM.saveScrobble Alias "saveScrobble" (artist As String, Title As String, album As String, length As Integer, curtime As UInteger) As Integer Export
	Dim s As Scrobble
	printf(!"LastFM::saveScrobble(): storing scrobble in memory.\n")
	s.setData(artist, title, album, length, curtime)
	This.m_scrobbles.Push(s)
End Function

Sub LastFM.dumpScrobbles2 Alias "dumpScrobbles2" () Export
	Dim fd As FILE Ptr
	Dim i As Integer, ret As Integer
	Dim s As Scrobble Ptr
	fd = fopen(this.c_xmlpath & "/lastfm.xml","wb")
   
	If fd = 0 Then 
		printf(!"LastFM::dumpScrobbles2(): Error opening lastfm.xml.\n")
		Return
	EndIf
   
	ret = fprintf(fd, !"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<lastfm>\n")
	If fd = 0 Then 
		printf(!"LastFM::dumpScrobbles2(): Error writing lastfm.xml.\n")
		Return
	EndIf
	While(This.m_scrobbles.Empty() = 0)
		s = This.m_scrobbles.Front()
		ret = fprintf(fd, !"\t<entry artist=\"%s\" title=\"%s\" album=\"%s\" time=\"%d\" length=\"%d\" />\n", s->m_artist, s->m_title, s->m_album, s->m_curtime, s->m_length)
		If fd = 0 Then 
			printf(!"LastFM::dumpScrobbles2(): Error writing lastfm.xml.\n")
			Return
		EndIf
		This.m_scrobbles.Pop()
	Wend
	ret = fprintf(fd, "</lastfm>")
	If fd = 0 Then 
		printf(!"LastFM::dumpScrobbles2(): Error writing lastfm.xml.\n")
		Return
	EndIf
End Sub

Sub LastFM.dumpScrobbles Alias "dumpScrobbles" () Export
	Dim As xmlTextWriterPtr writer
	Dim As Integer ret, i
	Dim As Scrobble Ptr s 
   
	printf(!"LastFM::dumpScrobbles(): Dumping scrobbles to " & this.c_xmlpath & !"/lastfm.xml\n")
	writer = xmlNewTextWriterFilename(this.c_xmlpath & "/lastfm.xml", 0)
   
	If writer = 0 Then 
		printf !"LastFM::dumpScrobbles(): xmlNewTextWriterFilename() failed!\n"
		Return 
	EndIf
   
	xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL)   
	xmlTextWriterStartElement(writer, "lastfm")
	xmlTextWriterStartElement(writer, "encoding")
	xmlTextWriterWriteAttribute(writer, "character", "UTF-8")
	xmlTextWriterWriteAttribute(writer, "data", "RFC 1378")
	xmlTextWriterEndElement(writer)
	While(This.m_scrobbles.Empty() = 0)
		s = This.m_scrobbles.Front()
		xmlTextWriterStartElement(writer, "entry")  
		xmlTextWriterWriteAttribute(writer, "artist", s->m_artist)
		xmlTextWriterWriteAttribute(writer, "title", s->m_title)
		xmlTextWriterWriteAttribute(writer, "album", s->m_album)
		xmlTextWriterWriteAttribute(writer, "time", Str(s->m_curtime))
		xmlTextWriterWriteAttribute(writer, "length", Str(s->m_length))
		xmlTextWriterEndElement(writer)
		This.m_scrobbles.Pop()
	Wend
	xmlTextWriterEndElement(writer)
	xmlTextWriterEndDocument(writer)
	xmlFreeTextWriter(writer)
	
	printf(!"LastFM::dumpScrobbles(): Done.\n")
End Sub

Sub LastFM.loadScrobbles Alias "loadScrobbles" () Export
	Dim reader As xmlTextReaderPtr
	Dim ret As Integer, getNext As Integer
	Dim As Integer entries
	dim constname as zstring ptr, value as zstring Ptr
	Dim As String Artist, Title, Album
	Dim As Integer curtime, length
   
	reader = xmlReaderForFile("lastfm.xml", NULL, 0)
   
	If reader = NULL Then Return
	ret = xmlTextReaderRead(reader)
	ret = 1
	Do While(ret = 1)
		constname = xmlTextReaderConstName(reader)
		value = xmlTextReaderConstValue(reader)
		Select Case *constname
			Case "lastfm"
				Select Case xmlTextReaderNodeType(reader)
					Case 1
						printf !"LastFM::loadScrobbles(): Start of database\n"
					Case 15
						printf !"LastFM::loadScrobbles(): End of database\n"
				End Select
			Case "entry"
				Artist = *xmlTextReaderGetAttribute(reader, "artist")
				Title = *xmlTextReaderGetAttribute(reader, "title")
				Album = *xmlTextReaderGetAttribute(reader, "album")
				curtime = Val(*xmlTextReaderGetAttribute(reader, "time"))
				length = Val(*xmlTextReaderGetAttribute(reader, "length"))
				this.saveScrobble(artist, Title, album, curtime, length)
		End Select
		ret = xmlTextReaderRead(reader)
	Loop
    
End Sub

'' End Last.fm code
