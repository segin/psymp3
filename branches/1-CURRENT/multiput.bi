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

#Ifndef __MULTIPUT_BI__
#Define __MULTIPUT_BI__

#Include Once "psymp3.bi"

Declare Sub MultiPut Alias "MultiPut" (Byval lpTarget As Any Ptr= 0, Byval xMidPos  As Integer= 0, Byval yMidPos  As Integer= 0, Byval lpSource As Any Ptr   , Byval xScale   As Single = 1, Byval yScale   As Single = 1, Byval Rotate   As Single = 0, Byval Trans    As Integer= 0)

#EndIf /' __MULTIPUT_BI__ '/