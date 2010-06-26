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
Constructor Scrobble()
	This.setData("", "", "", 0, 0)
End Constructor

Constructor Scrobble(ByRef cpy As Scrobble)
	This.setData(cpy.m_artist, cpy.m_title, cpy.m_album, cpy.m_length, cpy.m_curtime)
End Constructor

Constructor Scrobble(artist As String, title As String, album As String, length As UInteger, curtime As UInteger)
	This.setData(artist, title, album, length, curtime)
End Constructor

Sub Scrobble.setData Alias "setData" (artist As String, title As String, album As String, length As UInteger, curtime As UInteger)
	This.m_artist = artist
	This.m_title = title
	This.m_album = album
	This.m_length = length
	This.m_curtime = curtime
End Sub