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

#Ifndef __STRINGS_BI__
#Define __STRINGS_BI__

'#Include Once "psymp3.bi"

Declare Function MD5str Alias "MD5str" (chars As String) As String
Declare Function percent_encode Alias "percent_encode" (message As String) As String
Declare Function percent_encodeW Alias "percent_encodeW" (messageW As WString Ptr) As String
Declare Function wstring_to_utf8 Alias "wstring_to_utf8" (from As WString Ptr) As String

#EndIf /' __STRINGS_BI__ '/