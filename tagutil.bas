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

#Include Once "tagutil.bi"

Function getmp3name Alias "getmp3name" ( byval stream as FSOUND_STREAM ptr ) as String Export 
	dim tagname as zstring ptr, taglen as integer

	FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "TIT2", @tagname, @taglen )
	tagname += 1 
	If( taglen = 0 ) then 
		FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V1, "TITLE", @tagname, @taglen )
	End if
	printf(!"getmp3name(): tagname = \"%s\" taglen = %d\n", tagname, taglen)
	getmp3name = left( *tagname, taglen )
end Function

Function getmp3nameW Alias "getmp3nameW" ( byval stream as FSOUND_STREAM ptr ) as WString Ptr Export
   Static wret As WString * 256
	dim tagname as zstring ptr, taglen as integer

	FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "TIT2", @tagname, @taglen )
	tagname += 1 
	If( taglen = 0 ) then 
		FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V1, "TITLE", @tagname, @taglen )
	End if
	If Left(*tagname,2) = Chr(255) + Chr(254) Then
		tagname += 2
		wret = *CPtr(WString Ptr, tagname)  
	Else
		wret = left( *tagname, taglen )
	EndIf
	printf(!"getmp3nameW(): tagname = \"%ls\" taglen = %d\n", @wret, taglen)
	Return @wret
end Function

'':::::
Function getmp3artist Alias "getmp3artist" ( byval stream as FSOUND_STREAM ptr ) as String Export
	dim tagname as zstring ptr, taglen as integer
	FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "TPE1", @tagname, @taglen )
	tagname += 1
	If( taglen = 0 ) then 
		FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V1, "ARTIST", @tagname, @taglen )
        	if( taglen = 0 ) then 
			FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ASF, "WM/AlbumArtist", @tagname, @taglen )
        	end if
	End if
	printf(!"getmp3artist(): tagname = \"%s\" taglen = %d\n", tagname, taglen)
	getmp3artist = left( *tagname, taglen )
end function

function getmp3artistW Alias "getmp3artistW" ( byval stream as FSOUND_STREAM ptr ) as WString Ptr Export
   Static wret As WString * 256
	dim tagname as zstring ptr, taglen as integer

	FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "TPE1", @tagname, @taglen )
	tagname += 1 
	If( taglen = 0 ) then 
		FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V1, "ARTIST", @tagname, @taglen )
	End if
	If Left(*tagname,2) = Chr(255) + Chr(254) Then
		tagname += 2
		wret = *CPtr(WString Ptr, tagname)  
	Else
		wret = left( *tagname, taglen )
	EndIf
	printf(!"getmp3artistW(): tagname = \"%ls\" taglen = %d\n", @wret, taglen)
	Return @wret
end Function

function getmp3album Alias "getmp3album" ( byval stream as FSOUND_STREAM ptr ) as String Export
	dim tagname as zstring ptr, taglen as integer
   
	FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "TALB", @tagname, @taglen )
	tagname += 1
	If( taglen = 0 ) then 
		FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V1, "ALBUM", @tagname, @taglen )
		if( taglen = 0 ) then 
			'Assume at this point we're decoding Windows Media Audio...
			FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ASF, "WM/AlbumTitle", @tagname, @taglen )
		End If
	End if
	printf(!"getmp3album(): tagname = \"%s\" taglen = %d\n", tagname, taglen)
	getmp3album = left( *tagname, taglen ) 
end function

function getmp3albumW Alias "getmp3albumW" ( byval stream as FSOUND_STREAM ptr ) as WString Ptr Export
	Static wret As WString * 256
	dim tagname as zstring ptr, taglen as integer

	FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "TALB", @tagname, @taglen )
	tagname += 1 
	If( taglen = 0 ) then 
		FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V1, "ALBUM", @tagname, @taglen )
	End if
	If Left(*tagname,2) = Chr(255) + Chr(254) Then
		tagname += 2
		wret = *CPtr(WString Ptr, tagname)  
	Else
		wret = left( *tagname, taglen )
	End If
	printf(!"getmp3albumW(): tagname = \"%ls\" taglen = %d\n", @wret, taglen)
	Return @wret
end Function


function getmp3albumart Alias "getmp3albumart" ( byval stream as FSOUND_STREAM ptr ) as Any Ptr Export
	dim imgdata As Any Ptr, buflen As Integer
	Dim console As Integer
	Dim ret As Boolean
	console = FreeFile()
	Open Err As #console
	ret = FSOUND_Stream_FindTagField( stream, FSOUND_TAGFIELD_ID3V2, "APIC", @imgdata, @buflen )
	
	If ret = FALSE Then
		Print #console, "getmp3albumart(): FindTagField(APIC) FAILED!"
		Close #console
		Return NULL
	Else
		Print #console, "getmp3albumart(): FindTagField(APIC) succeeded."
		Close #console
		If StrnCmp(imgdata+1,"image/jpeg", 10) = 0 Then
			Var imgbuf = imgdata + 20
			Var imglen = buflen - 20 
		End If
		Dim fp As FILE Ptr
		'fp = fopen("crack.kills.jpg","w+")
		'fwrite(imgdata,buflen,1,fp)
		'fclose(fp)
		Return NULL 
	End If
End Function
