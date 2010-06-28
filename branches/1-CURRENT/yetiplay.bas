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

#Include Once "psymp3.bi"

''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
' Callbacks for ogg/mp3 decoding and feeding to audio backend
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

sub af_read_until Cdecl Alias "af_read_until" _
	( _
		byval af     as audiofile_t ptr, _
		byval amount as integer _
	) Export

	while len( af->pcm_buf ) < amount

	dim as integer read_len
	dim as string  _in     = space( 16384 )
	dim as string  _out    = space( 32768 )
	dim as integer INBUFF  = 16384
	dim as integer OUTBUFF = 32768

	select case af->t
		case AF_MP3
			dim as mp3_t ptr mp3 = af->mp3

			' Try and decode some more mp3 if running low
			dim as integer size
			dim as integer ret

			read_len = fread( @_in[0], 1, INBUFF, af->hfile )
			if read_len > 0 then
				ret = mpg123_decode( af->mp3->m, @_in[0], read_len, @_out[0], OUTBUFF, @size )

				af->pcm_buf += left( _out, size )

				while (ret <> MPG123_ERR) and (ret <> MPG123_NEED_MORE)
					ret = mpg123_decode( af->mp3->m,NULL,0,@_out[0],OUTBUFF,@size)
					af->pcm_buf += left( _out, size )
				wend

				if ret = MPG123_ERR then
					puts "audiofile_open: MPG123_ERR"
					end 1
				end if
			elseif read_len < 0 then
				puts "fread error 3"
				end
			else
				exit sub
			end if
		case AF_OGG
			dim as integer section

			read_len = ov_read( @af->ogg->oggStream, @_out[0], OUTBUFF, 0, 2, 1, @section )
			if read_len > 0 then
				af->pcm_buf += left( _out, read_len )
			elseif read_len < 0 then
				puts "vorbis error 3"
				end
			else
				exit sub
			end if
	end select

	wend

end sub

dim shared as integer callbacks

'::::::::
sub af_callback Cdecl Alias "af_callback" _
	( _
		byval userdata as any ptr, _
		byval stream   as UByte ptr, _
		byval _len     as integer _
	) Export 

	callbacks += 1

	dim as audio_t ptr audio = userdata
	dim as audiofile_t ptr af = audio->af
	dim as integer skipbytes = (audio->skip / 1000) * af->rate * af->channels * sizeof( short )

	' Read enough to be able to skip
	af_read_until( af, skipbytes )

	' Perform skip
	af->pcm_buf = right( af->pcm_buf, len( af->pcm_buf ) - skipbytes )
	audio->skip = 0

	' Read some to play
	af_read_until( af, _len )

	' play what we have, and end if we have none left
	if len( af->pcm_buf ) > _len then
		memcpy( stream, @af->pcm_buf[0], _len )
		af->pcm_buf = right( af->pcm_buf, len( af->pcm_buf ) - _len )
	elseif len( af->pcm_buf ) > 0 then
		memcpy( stream, @af->pcm_buf[0], len( af->pcm_buf ) )
		af->pcm_buf = ""
	else
		SDL_PauseAudio(1)
		audio->playing = 0
		exit sub
	end if

	audio->pcm += space( _len )

	dim as any ptr p = @audio->pcm[len( audio->pcm ) - _len]

	memcpy( p, stream, _len )

	if audio->mute then
		memset( stream, 0, _len )
	end if

end sub

'::::::::
function audiofile_open Alias "audiofile_open" _
	( _
		byref _filename_ as string _
	) as audiofile_t Ptr Export

	dim as audiofile_t ptr af = new audiofile_t

	select case lcase( right( _filename_, 4 ) )
		case ".mp3"
			af->t = AF_MP3
			af->mp3 = new mp3_t
		case ".ogg"
			af->t = AF_OGG
			af->ogg = new ogg_t
		case else
			puts "audiofile_open: bad filetype"
			end 1
	end select

	af->filename = _filename_
	if _filename_ = "" then
		puts "audiofile_open: attempt to open an file with no name specified"
		end 1
	end if

	af->hfile = fopen( _filename_, "rb" )
	if af->hfile = NULL then
		puts "audiofile_open: fopen failed"
		end 1
	end if

	select case af->t
		case AF_MP3
			af->mp3->m = mpg123_new( NULL, NULL )
			if af->mp3->m = NULL then
				puts "audiofile_open: mpg123_new failed"
				end 1
			end if

			mpg123_open_feed( af->mp3->m )

			dim as integer read_len
			dim as integer size
			dim as integer ret
			dim as string  _in     = space( 16384 )
			dim as string  _out    = space( 32768 )
			dim as integer INBUFF  = 16384
			dim as integer OUTBUFF = 32768

			dim as integer skip_size ' id3v2 skip size

			read_len = fread( @_in[0], 1, INBUFF, af->hfile )

			' skip any ID3 header, we need to get the NEW_FORMAT asap
			if left( _in, 3 ) = "ID3" then
				dim as uinteger i = *cast( uinteger ptr, @_in[6] )
				dim as uinteger j

				for k as integer = 0 to 3
					j shl= 7
					j or= cast( ubyte ptr, @i )[k]
				next k

				skip_size = j + 10 ' footer not going to be a problem here?
			end if

			while skip_size > 0
				if skip_size >= INBUFF then
					read_len = fread( @_in[0], 1, INBUFF, af->hfile )
					if read_len <= 0 then
						puts "audiofile_open: empty mp3 file?"
						end 1
					end if
					skip_size -= read_len
				else
					' need to do more here really...
					exit while
				end if
			wend

			if read_len <= 0 then
				puts "audiofile_open: empty mp3 file?"
				end 1
			end if

			ret = mpg123_decode( af->mp3->m, @_in[0], read_len, @_out[0], OUTBUFF, @size )
			if ret = MPG123_NEW_FORMAT then
				dim as long rate
				dim as integer channels, enc
				mpg123_getformat( af->mp3->m, @rate, @channels, @enc )
				af->channels = channels
				af->rate = rate
				puts "New format: " & rate & " Hz, " & channels & " channels, encoding value " & enc
			else
				puts "Expected format"
				end 1
			end if

			af->pcm_buf += left( _out, size )

			while (ret <> MPG123_ERR) and (ret <> MPG123_NEED_MORE)
				ret = mpg123_decode( af->mp3->m,NULL,0,@_out[0],OUTBUFF,@size)
				af->pcm_buf += left( _out, size )
			wend

			if ret = MPG123_ERR then
				puts "audiofile_open: MPG123_ERR"
				end 1
			end if

			' Read some to play
			af_read_until( af, 32768 )
		case AF_OGG
			dim as vorbis_info ptr vorbisInfo

			if ov_open( af->hfile, @af->ogg->oggStream, NULL, 0 ) <> 0 then
				puts "audiofile_open: ov_open failed"
				end 1
			end if

			vorbisInfo = ov_info( @af->ogg->oggStream, -1 )

			af->rate = vorbisInfo->rate
			af->channels = vorbisInfo->channels

			' Read some to play
			af_read_until( af, 32768 )
	end select

	function = af

end function

'::::::::
sub audiofile_close Alias "audiofile_close" _
	( _
		byval af as audiofile_t ptr _
	) Export

	select case af->t
		case AF_MP3
			mpg123_delete( af->mp3->m )
			delete af->mp3
			af->t = AF_NONE
		case AF_OGG
			ov_clear( @af->ogg->oggStream )
			delete af->ogg
			af->t = AF_NONE
	end select

	delete af

end sub

''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
' audio backend object functions
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

'::::::::
function audio_open Alias "audio_open" _
	( _
		byval af      as audiofile_t ptr, _
		byval samples as integer _
	) as audio_t Ptr Export

	dim as audio_t ptr   audio = new audio_t
	dim as SDL_AudioSpec desired

	audio->af        = af
	audio->samples   = samples

	desired.freq     = af->rate
	desired.format   = AUDIO_S16
	desired.channels = af->channels
	desired.samples  = samples
	desired.callback = @af_callback
	desired.userdata = audio

	if SDL_OpenAudio( @desired, NULL ) then
		puts "SDL_OpenAudio: " & *SDL_GetError( )
		end
	end if

	function = audio

end function

'::::::::
sub audio_play Alias "audio_play" _
	( _
		byval audio as audio_t ptr _
	) Export

	audio->playing = -1
	SDL_PauseAudio(0)

end sub

'::::::::
sub audio_stop Alias "audio_stop" _
	( _
		byval audio as audio_t ptr _
	) Export

	SDL_PauseAudio(1)
	audio->playing = 0

end sub

'::::::::
sub audio_skip Alias "audio_skip" _
	( _
		byval audio        as audio_t ptr, _
		byval milliseconds as integer _
	) Export 

	SDL_LockAudio( )
	audio->skip += milliseconds
	SDL_UnlockAudio( )

end sub

'::::::::
sub audio_close Alias "audio_close" _
	( _
		ByVal audio as audio_t ptr _
	) Export

	SDL_CloseAudio( )

	delete audio

end sub

''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
' Program MAIN
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

dim shared as string  playlist_(0 to 499)
dim shared as integer playlist_pos = 0
dim shared as integer playlist_len = 0

sub getplaylist _
	( _
		byval argc as integer, _
		byval argv as zstring ptr ptr _
	)

	dim as integer arg = 1

	while command( arg ) <> ""
		playlist_(playlist_len) = command( arg )
		playlist_len += 1
		arg += 1
	wend

	if playlist_len = 0 then
		puts "No files given to play"
		end 1
	end if

end sub


sub mainloop _
	( _
		byval audio as audio_t ptr, _
		byval af    as audiofile_t ptr _
	)

	dim as complex_t ptr in_data     = callocate( yetiplay.fourier_size * sizeof( complex_t ) )
	dim as complex_t ptr out_data    = callocate( yetiplay.fourier_size * sizeof( complex_t ) )
	dim as double        t           = timer( )
	dim as integer       frames      = 0
	dim as double        key_timeout = timer( )

	while not multikey( fb.sc_escape )
		dim as integer k_up
		dim as integer k_dn
		dim as integer k_le
		dim as integer k_ri
		dim as integer k_m

		if (timer( ) - key_timeout) > 0 then 
			k_up = multikey( fb.sc_up )
			k_dn = multikey( fb.sc_down )
			k_le = multikey( fb.sc_left )
			k_ri = multikey( fb.sc_right )
			k_m  = multikey( fb.sc_m )
		end if

		dim as integer start_next

		if k_up then
			SDL_LockAudio( )
			start_next = 1
			if playlist_pos < (playlist_len - 1) then
				playlist_pos += 1
			else
				playlist_pos = 0
			end if
			key_timeout = timer( ) + 0.3
			SDL_UnlockAudio( )
		elseif k_dn then
			SDL_LockAudio( )
			start_next = 1
			if playlist_pos > 0 then
				playlist_pos -= 1
			end if
			key_timeout = timer( ) + 0.3
			SDL_UnlockAudio( )
		elseif k_ri then
			audio_skip( audio, 1000 )
			key_timeout = timer( ) + 0.1
		end if

		dim as integer todo, do_draw
		dim as string pcm
		dim as integer needed = yetiplay.fourier_size * af->channels * sizeof( short )

		dim as integer channels

		SDL_LockAudio( )
			if k_m then
				audio->mute = not audio->mute
				key_timeout = timer( ) + 0.3
			end if
			todo = len( audio->pcm )
			if len( audio->pcm ) >= (needed * 8) then
				audio->pcm = right( audio->pcm, needed * 8 )
			end if
			if len( audio->pcm ) >= needed then
				pcm = audio->pcm
				audio->pcm = right( audio->pcm, len( audio->pcm ) - needed )
			end if
			if (audio->playing = 0) then
				if playlist_pos < (playlist_len - 1) then
					playlist_pos += 1
				else
					playlist_pos = 0
				end if
				start_next = 1
			end if
			channels = af->channels
		SDL_UnlockAudio( )

		if start_next then
			SDL_LockAudio( )
				dim as integer old_channels = af->channels
				dim as integer old_rate     = af->rate
				audiofile_close( af )
				af = NULL

				af = audiofile_open( playlist_(playlist_pos) )
				if (af->channels <> old_channels) or (af->rate <> old_rate) then
					' a new backend
					audio_stop( audio )
					audio_close( audio )
					audio = audio_open( af, yetiplay.buffer_count )
					audio_play( audio )
				else
					' just swap in the new audiofile
					audio->af = af
					audio_play( audio )
				end if
			SDL_UnlockAudio( )
		end if

		if len( pcm ) >= needed then
			for i as integer = 0 to yetiplay.fourier_size - 1
				dim as short l = cast( short ptr, strptr( pcm ) )[int(i * channels)]
				dim as short r = 0
				if channels = 2 then
					r = cast( short ptr, strptr( pcm ) )[int(i * channels)+1]
					in_data[i].re = ((cast(integer, l) + cast(integer, r)) / 2) / 32768
				else
					in_data[i].re = l / 32768
				end if
				in_data[i].im = 0
			next i
			quadfft( yetiplay.fourier_size, in_data, out_data )
			do_draw = 1
		end if

		frames += 1
		dim as integer fps = frames / (timer( ) - t)

		screenlock( )
			if do_draw then
				draw_screen( yetiplay.fourier_size, in_data, out_data, 44100 \ 4 )
			end if
			SDL_LockAudio( )
			locate 1, 1
			print "Filename   : " & af->filename          & "         "
			print "FPS        : " & fps                   & "         "
			print "TODO       : " & todo                  & "         "
			print "CBs        : " & callbacks             & "         "
			print "Playing    : " & audio->playing        & "         "
			print "SDL Status : " & SDL_GetAudioStatus( ) & "         "
			SDL_UnlockAudio( )
			#define dec( s, n ) string( n - len( "" & s ), "0" ) & s
		screenunlock( )

		sleep 10, 1

		if (timer( ) - t) > 1 then
			frames = 0
			t = timer( )
		end if

	wend

end sub

function main _
	( _
		byval argc as integer, _
		byval argv as zstring ptr ptr _
	) as integer

	dim as audio_t ptr     audio
	dim as audiofile_t ptr af

	' Initialize libraries
	mpg123_init( )
	SDL_Init( SDL_INIT_EVERYTHING )
	SDL_AudioInit( NULL )

	' Initialize program
	getplaylist( argc, argv )
	screenres yetiplay.screen_width, yetiplay.screen_height, 32
	af = audiofile_open( playlist_(playlist_pos) )
	audio = audio_open( af, yetiplay.buffer_count )
	audio_play( audio )

	' run the main loop
	mainloop( audio, af )

	' Cleanly shut down program
	audio_stop( audio )
	audio_close( audio )
	audiofile_close( af )

	' Cleanly shut down libraries
	SDL_AudioQuit( )
	SDL_Quit( )
	mpg123_exit( )

	function = 0

end Function
