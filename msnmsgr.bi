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

#Ifndef __MSNMSGR_BI__
#Define __MSNMSGR_BI__

#Include Once "psymp3.bi"

#Ifdef __FB_WIN32__
Declare Sub AnnounceWMP Alias "AnnounceWMP" (artist As String, Title As String, Album As String)
Declare Sub ClearWMP Alias "ClearWMP" ()
#EndIf __FB_WIN32__

#EndIf /' __MSNMSGR_BI__ '/