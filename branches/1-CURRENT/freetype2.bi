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

#Ifndef __FREETYPE2_BI__
#Define __FREETYPE2_BI__

#Include Once "psymp3.bi"

' Alpha blending 
#define FT_MASK_RB_32         &h00FF00FF 
#define FT_MASK_G_32          &h0000FF00 

Type FT_Var 
	ErrorMsg   As FT_Error 
	Library    As FT_Library 
	PixelSize  As Integer 
End Type

Common Shared FT_Var As FT_Var 

Declare Function InitFT Alias "InitFT" () As Integer
Declare Sub DrawGlyph Alias "DrawGlyph" (ByVal FontFT As FT_Face, ByVal x As Integer, ByVal y As Integer, ByVal Clr As UInteger)
Declare Sub DrawGlyphBuffer Alias "DrawGlyphBuffer" (ByVal Buffer As Any Ptr, ByVal FontFT As FT_Face, ByVal x As Integer, ByVal y As Integer, ByVal Clr As UInteger)
Declare Function PrintFT Alias "PrintFT" (ByVal x As Integer, ByVal y As Integer, ByVal Text As String, ByVal Font As Integer, ByVal Size As Integer = 14, ByVal Clr As UInteger = Rgb(255, 255, 255)) as Integer
Declare Function PrintFTW Alias "PrintFTW" (ByVal x As Integer, ByVal y As Integer, ByVal Text As WString Ptr, ByVal Font As Integer, ByVal Size As Integer = 14, ByVal Clr As UInteger = Rgb(255, 255, 255)) as Integer
Declare Function PrintFTB Alias "PrintFTB" (ByVal Buffer As Any Ptr, ByVal x As Integer, ByVal y As Integer, ByVal Text As String, ByVal Font As Integer, ByVal Size As Integer = 14, ByVal Clr As UInteger = Rgb(255, 255, 255)) as Integer
Declare Function PrintFTBW Alias "PrintFTBW" (ByVal Buffer As Any Ptr, ByVal x As Integer, ByVal y As Integer, ByVal Text As WString Ptr, ByVal Font As Integer, ByVal Size As Integer = 14, ByVal Clr As UInteger = Rgb(255, 255, 255)) as Integer
Declare Function GetFont Alias "GetFont" (ByVal FontName As String) As Integer

#EndIf /' __FREETYPE2_BI__ '/