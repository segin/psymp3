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

#Ifndef __YETIPLAY_BI__
#Define __YETIPLAY_BI__

'#Include Once "psymp3.bi"

'::::::::
type ogg_t
	oggStream as OggVorbis_File
end type

'::::::::
type mp3_t
	m as any ptr
end type

'::::::::
enum audiofile_e
	AF_BAD
	AF_NONE
	AF_MP3
	AF_OGG
end enum

'::::::::
type audiofile_t
	t        as audiofile_e
	mp3      as mp3_t ptr
	ogg      as ogg_t ptr
	filename as string
	hfile    as FILE ptr
	rate     as integer
	channels as integer
	pcm_buf  as string
end type

'::::::::
type audio_t
	af      as audiofile_t ptr
	samples as integer
	skip    as integer
	playing as integer
	mute    as integer
	pcm     as string
end Type

Declare Sub af_read_until Cdecl Alias "af_read_until" _
	( _
		byval af     as audiofile_t ptr, _
		byval amount as integer _
	)
Declare sub af_callback Cdecl Alias "af_callback" _
	( _
		byval userdata as any ptr, _
		byval stream   as UByte ptr, _
		byval _len     as integer _
	)
Declare function audiofile_open Alias "audiofile_open" _
	( _
		byref _filename_ as string _
	) as audiofile_t Ptr
Declare sub audiofile_close Alias "audiofile_close" _
	( _
		byval af as audiofile_t ptr _
	)
Declare Function audio_open Alias "audio_open" _
	( _
		byval af      as audiofile_t ptr, _
		byval samples as integer _
	) as audio_t Ptr
Declare sub audio_play Alias "audio_play" _
	( _
		byval audio as audio_t ptr _
	)
Declare sub audio_stop Alias "audio_stop" _
	( _
		byval audio as audio_t ptr _
	)
Declare sub audio_skip Alias "audio_skip" _
	( _
		byval audio        as audio_t ptr, _
		byval milliseconds as integer _
	)
Declare Sub audio_close Alias "audio_close" _
	( _
		ByVal audio as audio_t ptr _
	)

#EndIf /' __YETIPLAY_BI__ '/