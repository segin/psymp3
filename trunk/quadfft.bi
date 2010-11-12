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


#Ifndef __QUADFFT_BI__
#Define __QUADFFT_BI__

'#Include Once "psymp3.bi"

Declare Sub quadfft Alias "quadfft" (ByVal n As Integer, ByVal in_data As complex_t Ptr, ByVal out_data As complex_t Ptr, ByVal inverse As Integer = 0 )

#EndIf /' __QUADFFT_BI__ '/