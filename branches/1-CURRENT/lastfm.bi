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

#Ifndef __LASTFM_BI__
#Define __LASTFM_BI__

fbext_TDeclare(fbext_Queue, ((Scrobble)) )

Namespace ext	
	Type ScrobbleList As fbext_Queue( ((Scrobble)) )
End Namespace

Type LastFM Alias "LastFM"
Private:
	m_artist(500) As String
	m_name(500) As String
	m_album(500) As String
	m_length(500) As UInteger
	m_curtime(500) As Integer
	m_entries As Integer
	m_scrobbles As ext.ScrobbleList
	m_session As String
	c_username As String
	c_password As String
	c_apihost(2) As String
	c_apiport(2) As Short
	c_apipath(2) As String
	c_xmlpath As String
Public:
	Declare Constructor()
	Declare Destructor()
	Declare Sub readConfig Alias "readConfig" ()
	Declare Function getSessionKey Alias "getSessionKey" () As String
	Declare Function setNowPlaying Alias "setNowPlaying" () As SOCKET
	Declare Function scrobbleTrack Alias "scrobbleTrack" () As Integer
	Declare Function submitData Alias "submitData" (sData As String, host As Integer) As SOCKET
	Declare Function submitScrobble Alias "submitScrobble" (artist As String, Title As String, album As String, length As Integer, curtime As UInteger) As Integer
	Declare Function saveScrobble Alias "saveScrobble" (artist As String, Title As String, album As String, length As Integer, curtime As UInteger) As Integer
	Declare Sub dumpScrobbles Alias "dumpScrobbles" ()
	Declare Sub dumpScrobbles2 Alias "dumpScrobbles2" () 
	Declare Sub loadScrobbles Alias "loadScrobbles" ()
	Declare Sub submitSavedScrobbles Alias "submitSavedScrobbles" ()
End Type

#EndIf /' __LASTFM_BI__ '/ 