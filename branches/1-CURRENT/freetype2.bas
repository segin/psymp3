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

#Include Once "freetype2.bi"

Function InitFT Alias "InitFT" () As Integer Export
	Dim console As Integer
	console = FreeFile
	Open Err As #console
	Print #console, "Initalizing FreeType... ";
	FT_Var.ErrorMsg = FT_Init_FreeType(@FT_Var.Library) 
	If FT_Var.ErrorMsg Then
		Print #console, "failed." 
		Function = 0
	Else
		Print #console, "done."
		Function = 1
	End If
	Close #console
End Function

Function GetFont Alias "GetFont" (ByVal FontName As String) As Integer Export 
	Dim Face As FT_Face 
	Dim ErrorMsg As FT_Error 
	Dim console As Integer
	console = FreeFile
	Open Err As #console
	Print #console, "Loading font ";FontName;"... ";
	ErrorMsg = FT_New_Face(FT_Var.Library, FontName, 0, @Face ) 
	If ErrorMsg Then
		Print #console, "failed!" 
		close #console
		Return 0 
	else
		Print #console, "done." 
		close #console
		Return CInt(Face) 
	end if 
End Function 

' Print Text 
' ---------- 
Function PrintFT Alias "PrintFT" (ByVal x As Integer, ByVal y As Integer, ByVal Text As String, ByVal Font As Integer, ByVal Size As Integer = 14, ByVal Clr As UInteger = Rgb(255, 255, 255)) as Integer Export
	Dim ErrorMsg   As FT_Error 
	Dim FontFT     As FT_Face 
	Dim GlyphIndex As FT_UInt 
	Dim Slot       As FT_GlyphSlot 
	Dim PenX       As Integer 
	Dim PenY       As Integer 
	Dim i          As Integer 
    
	' Get rid of any alpha channel in AlphaClr 
	Clr = Clr Shl 8 Shr 8 
	
	' Convert font handle 
	FontFT = Cast(FT_Face, Font) 
	
	' Set font size 
	ErrorMsg = FT_Set_Pixel_Sizes(FontFT, Size, Size) 
	FT_Var.PixelSize = Size 
	If ErrorMsg Then Return 0 
	
	' Draw each character 
	Slot = FontFT->Glyph 
	PenX = x 
	PenY = y 
        
	For i = 0 To Len(Text) - 1 
		' Load character index 
		GlyphIndex = FT_Get_Char_Index(FontFT, Text[i]) 
		
		' Load character glyph 
		ErrorMsg = FT_Load_Glyph(FontFT, GlyphIndex, FT_LOAD_DEFAULT) 
		If ErrorMsg Then Return 0 
		
		' Render glyph 
		ErrorMsg = FT_Render_Glyph(FontFT->Glyph, FT_RENDER_MODE_NORMAL) 
		If ErrorMsg Then Return 0 
		
		' Check clipping 
		If (PenX + FontFT->Glyph->Bitmap_Left + FontFT->Glyph->Bitmap.Width) > 640 Then Exit For 
		If (PenY - FontFT->Glyph->Bitmap_Top + FontFT->Glyph->Bitmap.Rows) > 400 Then Exit For 
		If (PenX + FontFT->Glyph->Bitmap_Left) < 0 Then Exit For 
		If (PenY - FontFT->Glyph->Bitmap_Top) < 0 Then Exit For 
		
		' Set pixels 
		DrawGlyph FontFT, PenX + FontFT->Glyph->Bitmap_Left, PenY - FontFT->Glyph->Bitmap_Top, Clr 
		
		PenX += Slot->Advance.x Shr 6 
	Next i 
End Function

Function PrintFTW Alias "PrintFTW" (ByVal x As Integer, ByVal y As Integer, Text As WString Ptr, ByVal Font As Integer, ByVal Size As Integer = 14, ByVal Clr As UInteger = Rgb(255, 255, 255)) as Integer Export
	Dim ErrorMsg   As FT_Error 
	Dim FontFT     As FT_Face 
	Dim GlyphIndex As FT_UInt 
	Dim Slot       As FT_GlyphSlot 
	Dim PenX       As Integer 
	Dim PenY       As Integer 
	Dim i          As Integer 
	Dim WText      As WString * 1024
    
	WText = *Text
    
	' Get rid of any alpha channel in AlphaClr 
	Clr = Clr Shl 8 Shr 8 
	
	' Convert font handle 
	FontFT = Cast(FT_Face, Font) 
    
	' Set font size 
	ErrorMsg = FT_Set_Pixel_Sizes(FontFT, Size, Size) 
	FT_Var.PixelSize = Size 
	If ErrorMsg Then Return 0 
    
	' Draw each character 
	Slot = FontFT->Glyph 
	PenX = x 
	PenY = y 
        
	For i = 0 To Len(WText) - 1 
		' Load character index 
		GlyphIndex = FT_Get_Char_Index(FontFT, WText[i]) 
		
		' Load character glyph 
		ErrorMsg = FT_Load_Glyph(FontFT, GlyphIndex, FT_LOAD_DEFAULT) 
		If ErrorMsg Then Return 0 
		
		' Render glyph 
		ErrorMsg = FT_Render_Glyph(FontFT->Glyph, FT_RENDER_MODE_NORMAL) 
		If ErrorMsg Then Return 0 
		
		' Check clipping 
		If (PenX + FontFT->Glyph->Bitmap_Left + FontFT->Glyph->Bitmap.Width) > 640 Then Exit For 
		If (PenY - FontFT->Glyph->Bitmap_Top + FontFT->Glyph->Bitmap.Rows) > 400 Then Exit For 
		If (PenX + FontFT->Glyph->Bitmap_Left) < 0 Then Exit For 
		If (PenY - FontFT->Glyph->Bitmap_Top) < 0 Then Exit For 
		
		' Set pixels 
		DrawGlyph FontFT, PenX + FontFT->Glyph->Bitmap_Left, PenY - FontFT->Glyph->Bitmap_Top, Clr 
		
		PenX += Slot->Advance.x Shr 6 
	Next i 
End Function 

Function PrintFTB Alias "PrintFTB" (ByVal Buffer As Any Ptr, ByVal x As Integer, ByVal y As Integer, ByVal Text As String, ByVal Font As Integer, ByVal Size As Integer = 14, ByVal Clr As UInteger = Rgb(255, 255, 255)) as Integer Export
	Dim ErrorMsg   As FT_Error 
	Dim FontFT     As FT_Face 
	Dim GlyphIndex As FT_UInt 
	Dim Slot       As FT_GlyphSlot 
	Dim PenX       As Integer 
	Dim PenY       As Integer 
	Dim i          As Integer 
	
	' Get rid of any alpha channel in AlphaClr 
	Clr = Clr Shl 8 Shr 8 
	
	' Convert font handle 
	FontFT = Cast(FT_Face, Font) 
	
	' Set font size 
	ErrorMsg = FT_Set_Pixel_Sizes(FontFT, Size, Size) 
	FT_Var.PixelSize = Size 
	If ErrorMsg Then Return 0 
	
	' Draw each character 
	Slot = FontFT->Glyph 
	PenX = x 
	PenY = y 
	
	For i = 0 To Len(Text) - 1 
		' Load character index 
		GlyphIndex = FT_Get_Char_Index(FontFT, Text[i]) 
		
		' Load character glyph 
		ErrorMsg = FT_Load_Glyph(FontFT, GlyphIndex, FT_LOAD_DEFAULT) 
		If ErrorMsg Then Return 0 
		
		' Render glyph 
		ErrorMsg = FT_Render_Glyph(FontFT->Glyph, FT_RENDER_MODE_NORMAL) 
		If ErrorMsg Then Return 0 
        
		' Check clipping 
		If (PenX + FontFT->Glyph->Bitmap_Left + FontFT->Glyph->Bitmap.Width) > 640 Then Exit For 
		If (PenY - FontFT->Glyph->Bitmap_Top + FontFT->Glyph->Bitmap.Rows) > 400 Then Exit For 
		If (PenX + FontFT->Glyph->Bitmap_Left) < 0 Then Exit For 
		If (PenY - FontFT->Glyph->Bitmap_Top) < 0 Then Exit For 
		
		' Set pixels 
		DrawGlyphBuffer Buffer, FontFT, PenX + FontFT->Glyph->Bitmap_Left, PenY - FontFT->Glyph->Bitmap_Top, Clr 
		
		PenX += Slot->Advance.x Shr 6 
	Next i 
End Function

Function PrintFTBW Alias "PrintFTBW" (ByVal Buffer As Any Ptr, ByVal x As Integer, ByVal y As Integer, Text As WString Ptr, ByVal Font As Integer, ByVal Size As Integer = 14, ByVal Clr As UInteger = Rgb(255, 255, 255)) as Integer Export
	Dim ErrorMsg   As FT_Error 
	Dim FontFT     As FT_Face 
	Dim GlyphIndex As FT_UInt 
	Dim Slot       As FT_GlyphSlot 
	Dim PenX       As Integer 
	Dim PenY       As Integer 
	Dim i          As Integer 
	Dim WText      As WString * 1024
	
	WText = *Text
	
	' Get rid of any alpha channel in AlphaClr 
	Clr = Clr Shl 8 Shr 8 
	
	' Convert font handle 
	FontFT = Cast(FT_Face, Font) 
	
	' Set font size 
	ErrorMsg = FT_Set_Pixel_Sizes(FontFT, Size, Size) 
	FT_Var.PixelSize = Size 
	If ErrorMsg Then Return 0 
	
	' Draw each character 
	Slot = FontFT->Glyph 
	PenX = x 
	PenY = y 
		
	For i = 0 To Len(WText) - 1 
		' Load character index 
		GlyphIndex = FT_Get_Char_Index(FontFT, WText[i]) 
		
		' Load character glyph 
		ErrorMsg = FT_Load_Glyph(FontFT, GlyphIndex, FT_LOAD_DEFAULT) 
		If ErrorMsg Then Return 0 
		
		' Render glyph 
		ErrorMsg = FT_Render_Glyph(FontFT->Glyph, FT_RENDER_MODE_NORMAL) 
		If ErrorMsg Then Return 0 
		
		' Check clipping 
		If (PenX + FontFT->Glyph->Bitmap_Left + FontFT->Glyph->Bitmap.Width) > 640 Then Exit For 
		If (PenY - FontFT->Glyph->Bitmap_Top + FontFT->Glyph->Bitmap.Rows) > 400 Then Exit For 
		If (PenX + FontFT->Glyph->Bitmap_Left) < 0 Then Exit For 
		If (PenY - FontFT->Glyph->Bitmap_Top) < 0 Then Exit For 
		
		' Set pixels 
		DrawGlyphBuffer Buffer, FontFT, PenX + FontFT->Glyph->Bitmap_Left, PenY - FontFT->Glyph->Bitmap_Top, Clr 
		
		PenX += Slot->Advance.x Shr 6 
	Next i 
End Function 

Sub DrawGlyph Alias "DrawGlyph" (ByVal FontFT As FT_Face, ByVal x As Integer, ByVal y As Integer, ByVal Clr As UInteger) Export
	Dim BitmapFT As FT_Bitmap 
	Dim BitmapPtr As UByte Ptr 
	Dim DestPtr As UInteger Ptr 
	
	Dim BitmapHgt As Integer 
	Dim BitmapWid As Integer 
	Dim BitmapPitch As Integer 
	
	Dim Src_RB As UInteger 
	Dim Src_G As UInteger 
	Dim Dst_RB As UInteger 
	Dim Dst_G As UInteger 
	Dim Dst_Color As UInteger 
	Dim Alpha As Integer 
	
	Dim ScreenWid As Integer
	
	ScreenInfo(ScreenWid)
	
	BitmapFT = FontFT->Glyph->Bitmap 
	BitmapPtr = BitmapFT.Buffer 
	BitmapWid = BitmapFT.Width 
	BitmapHgt = BitmapFT.Rows 
	BitmapPitch = ScreenWid - BitmapFT.Width 
	
	DestPtr = Cast(UInteger Ptr, ScreenPtr) + (y * ScreenWid) + x 
    
	Do While BitmapHgt 
		Do While BitmapWid 
			' Thanks, GfxLib 
			Src_RB = Clr And FT_MASK_RB_32 
			Src_G  = Clr And FT_MASK_G_32 
			
			Dst_Color = *DestPtr 
			Alpha = *BitmapPtr 
			
			Dst_RB = Dst_Color And FT_MASK_RB_32 
			Dst_G  = Dst_Color And FT_MASK_G_32 
			
			Src_RB = ((Src_RB - Dst_RB) * Alpha) Shr 8 
			Src_G  = ((Src_G - Dst_G) * Alpha) Shr 8 
			
			*DestPtr = ((Dst_RB + Src_RB) And FT_MASK_RB_32) Or ((Dst_G + Src_G) And FT_MASK_G_32) 
			
			DestPtr += 1 
			BitmapPtr += 1 
			BitmapWid -= 1 
		Loop 
		
		BitmapWid = BitmapFT.Width 
		BitmapHgt -= 1 
		DestPtr += BitmapPitch 
	Loop 
	
End Sub

Sub DrawGlyphBuffer Alias "DrawGlyphBuffer" (ByVal Buffer As Any Ptr, ByVal FontFT As FT_Face, ByVal x As Integer, ByVal y As Integer, ByVal Clr As UInteger) Export
	Dim BitmapFT As FT_Bitmap 
	Dim BitmapPtr As UByte Ptr 
	Dim DestPtr As UInteger Ptr 
	
	Dim BitmapHgt As Integer 
	Dim BitmapWid As Integer 
	Dim BitmapPitch As Integer 
	
	Dim BufferHgt As Integer 
	Dim BufferWid As Integer
	Dim BufferPtr As Any Ptr
	
	ImageInfo(Buffer, BufferWid, BufferHgt, , , BufferPtr)
	
	Dim Src_RB As UInteger 
	Dim Src_G As UInteger 
	Dim Dst_RB As UInteger 
	Dim Dst_G As UInteger 
	Dim Dst_Color As UInteger 
	Dim Alpha As Integer 
	
	BitmapFT = FontFT->Glyph->Bitmap 
	BitmapPtr = BitmapFT.Buffer 
	BitmapWid = BitmapFT.Width 
	BitmapHgt = BitmapFT.Rows 
	BitmapPitch = BufferWid - BitmapFT.Width 
	
	DestPtr = BufferPtr + (y * BufferWid) + x
	
	Do While BitmapHgt 
		Do While BitmapWid 
			' Thanks, GfxLib 
			Src_RB = Clr And FT_MASK_RB_32 
			Src_G  = Clr And FT_MASK_G_32 
			
			Dst_Color = *DestPtr 
			Alpha = *BitmapPtr 
			
			Dst_RB = Dst_Color And FT_MASK_RB_32 
			Dst_G  = Dst_Color And FT_MASK_G_32 
			
			Src_RB = ((Src_RB - Dst_RB) * Alpha) Shr 8 
			Src_G  = ((Src_G - Dst_G) * Alpha) Shr 8 
			
			*DestPtr = ((Dst_RB + Src_RB) And FT_MASK_RB_32) Or ((Dst_G + Src_G) And FT_MASK_G_32) 
			
			DestPtr += 1 
			BitmapPtr += 1 
			BitmapWid -= 1 
		Loop 
		
		BitmapWid = BitmapFT.Width 
		BitmapHgt -= 1 
		DestPtr += BitmapPitch 
	Loop 
	
End Sub
''
'' End FreeType2 functions
''
