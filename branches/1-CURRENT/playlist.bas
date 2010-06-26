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

#Include Once "playlist.bi"

'' Playlist code. Object-oriented as an experiment

Constructor Playlist () Export
	printf(!"Playlist::Playlist(): Creating playlist.\n")
	this.m_entries = 0
	this.m_position = 0
	this.m_playlist(1) = ""
End Constructor

Sub Playlist.addFile Alias "addFile" (file As String) Export
	printf(!"Playlist::addFile(): Adding file %s\n", file)
	this.m_entries += 1
	this.m_playlist(this.m_entries) = file
End Sub

Sub Playlist.addRawFile Alias "addRawFile" (file As String) Export
	printf(!"Playlist::addRawFile(): Adding raw file %s\n", file)
	this.m_entries += 1
	this.m_playlist(this.m_entries) = file
	this.m_israw(this.m_entries) = 1
End Sub

Function Playlist.getNextFile Alias "getNextFile" () As String Export
	If this.m_entries = 0 Or this.m_entries <= this.m_position Then
		this.m_position = this.m_entries + 1
		printf(!"Playlist::getNextFile(): No next file\n")
		Return ""
	EndIf 
	this.m_position += 1
	printf(!"Playlist::getNextFile(): Next file is %s.\n", this.m_playlist(this.m_position))
	Return this.m_playlist(this.m_position)
End Function

Function Playlist.getPrevFile Alias "getPrevFile" () As String Export
	If this.m_entries = 0 Then 
		printf(!"Playlist::getPrevFile(): No previous file\n")
		Return ""
	End If
	this.m_position -= 1
	Return this.m_playlist(this.m_position)
End Function

Function Playlist.getPosition Alias "getPosition" () As Integer Export
	Return this.m_position
End Function

Function Playlist.getEntries Alias "getEntries" () As Integer Export
	Return this.m_entries
End Function

Function Playlist.getFirstEntry Alias "getFirstEntry" () As String Export
	printf(!"Playlist::getFirstEntry(): Returning to top of playlist.\n")
	this.m_position = 0
	Return this.getNextFile()
End Function

Sub Playlist.savePlaylist Alias "savePlaylist" (file As String) Export
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
