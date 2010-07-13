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

#If __FB_UNIX__
function loadfile( byref defdir as string = "" ) as string
	var ff = freefile
	var directory = defdir
	var olddir = curdir
	if directory <> "" then
		chdir directory
	end if
	var result = ""
	open pipe "zenity --file-selection" for input as #ff
		line input #ff, result
	close ff
	if olddir <> directory then chdir olddir
	return result
end function

sub zmessage( byref msg as string )
	shell( !"zenity --info --text=\"" & msg & !"\"" )
end sub

zmessage("oh noes!")

? loadfile("/home/sirmud/projects")
#EndIf