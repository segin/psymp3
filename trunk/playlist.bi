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

#Ifndef __PLAYLIST_BI__
#Define __PLAYLIST_BI__

'#Include Once "psymp3.bi"

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
	Declare Sub addFile Alias "addFile" (file As String)
	Declare Sub addRawFile Alias "addRawFile" (file As String)
	Declare Sub savePlaylist Alias "savePlayList" (file As String)
	Declare Sub addPlaylist Alias "addPlayList" (m3u_file As String)
	Declare Function getNextFile Alias "getNextFile" () As String
	Declare Function getPrevFile Alias "getPrevFile" () As String
	Declare Function getPosition Alias "getPosition" () As Integer
	Declare Function getEntries Alias "getEntries" () As Integer
	Declare Function isFileRaw Alias "isFileRaw" () As Integer
	Declare Function getFirstEntry Alias "getFirstEntry" () As String
End Type

#EndIf