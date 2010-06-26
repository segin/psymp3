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

#Include Once "strings.bi"

Function MD5str Alias "MD5str" (chars As String) As String Export
	Dim md5hex As ZString * 32
	Dim digest As ZString * 16
	Dim state As md5_state_s
	Dim i As Integer

	md5_init(@state)
	md5_append(@state, Cast(md5_byte_t Ptr, StrPtr(chars)), Len(chars))
	md5_finish(@state, @digest)
	
	For i = 0 To 15
		sprintf(@md5hex _ 
		+ (i * 2), _
		"%02x", _
		digest[i])
	Next i
	Return md5hex
End Function

Function percent_encode Alias "percent_encode" (message As String) As String Export
   ' TODO: Test for strict RFC 1738 conformance
	Dim ret As String
	Dim i As Integer
	For i = 0 to Len(message) - 1
	Select Case(message[i])
		Case &h0 To &h2c, &h2f, &h3a, &h3b, &h3d, &h3f, &h40, &h5b, &h5d, &h80 to &hff
		ret &= "%" & Hex(message[i])
		Case Else
		ret &= Chr(message[i])
	End Select
	Next
	Return ret
End Function

Function wstring_to_utf8 Alias "wstring_to_utf8" (from As WString Ptr) As String Export
	Dim ret As String
	Dim i As Integer
	For i = 0 to Len(*from) - 1
	Select Case((*from)[i])
		Case Is < &h80
			ret &= Chr((*from)[i])
		Case Is < &h800
			ret &= Chr(&hc0 Or (*from)[i] Shr 6)
			ret &= Chr(&h80 Or (*from)[i] And &h3f)
		Case Is < &h10000
			ret &= Chr(&he0 Or (*from)[i] Shr 12)
			ret &= Chr(&h80 Or (*from)[i] Shr 6 And &h3f)
			ret &= Chr(&h80 Or (*from)[i] And &h3f)
		Case Is < &h200000
			ret &= Chr(&hf0 Or (*from)[i] Shr 18)
			ret &= Chr(&h80 Or (*from)[i] Shr 12 And &h3f)
			ret &= Chr(&h80 Or (*from)[i] Shr 6 And &h3f)
			ret &= Chr(&h80 Or (*from)[i] And &h3f)
	End Select
	Next
	Return ret
End Function

Function percent_encodeW Alias "percent_encodeW" (messageW As WString Ptr) As String Export
	Dim ret As String
 	Dim i As Integer
 	Dim message As String = wstring_to_utf8(messageW)
	Return percent_encode(message)
End Function
