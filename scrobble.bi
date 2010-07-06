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

#Ifndef __SCROBBLE_BI__
#Define __SCROBBLE_BI__

'#Include Once "psymp3.bi"

Type Scrobble Alias "Scrobble" 
Public: 'This is probably a bad idea, but who cares?
	m_artist As String
	m_title As String
	m_album As String
	m_length As Integer
	m_curtime As Integer
	Declare Constructor()
	Declare Constructor(ByRef cpy As Const Scrobble)
	Declare Constructor(artist As Const String, title As Const String, album As Const String, length As Const UInteger, curtime As Const UInteger)
	Declare Destructor()
	Declare Sub setData Alias "setData" (artist As Const String, title As Const String, album As Const String, length As Const UInteger, curtime As Const UInteger)
End Type

Declare Operator = (ByRef lhs As Scrobble, ByRef rhs As Scrobble) As Integer
Declare Operator <> (ByRef lhs As Scrobble, ByRef rhs As Scrobble) As Integer

#EndIf /' __SCROBBLE_BI__ '/