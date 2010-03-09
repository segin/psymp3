''
''
'' id3tag -- header translated with help of SWIG FB wrapper
''

#ifndef __id3tag_bi__
#define __id3tag_bi__

#Inclib "id3tag"

#define ID3_TAG_VERSION_D &h0400

type id3_byte_t as ubyte
type id3_length_t as uinteger
type id3_ucs4_t as uinteger
type id3_latin1_t as ubyte
type id3_utf16_t as ushort
type id3_utf8_t as byte

#define ID3_TAG_QUERYSIZE 10
#define ID3_FRAME_TITLE "TIT2"
#define ID3_FRAME_ARTIST "TPE1"
#define ID3_FRAME_ALBUM "TALB"
#define ID3_FRAME_TRACK "TRCK"
#define ID3_FRAME_YEAR "TDRC"
#define ID3_FRAME_GENRE "TCON"
#define ID3_FRAME_COMMENT "COMM"
#define ID3_FRAME_OBSOLETE "ZOBS"

enum 
	ID3_TAG_FLAG_UNSYNCHRONISATION = &h80
	ID3_TAG_FLAG_EXTENDEDHEADER = &h40
	ID3_TAG_FLAG_EXPERIMENTALINDICATOR = &h20
	ID3_TAG_FLAG_FOOTERPRESENT = &h10
	ID3_TAG_FLAG_KNOWNFLAGS = &hf0
end enum

enum 
	ID3_TAG_EXTENDEDFLAG_TAGISANUPDATE = &h40
	ID3_TAG_EXTENDEDFLAG_CRCDATAPRESENT = &h20
	ID3_TAG_EXTENDEDFLAG_TAGRESTRICTIONS = &h10
	ID3_TAG_EXTENDEDFLAG_KNOWNFLAGS = &h70
end enum

enum 
	ID3_TAG_RESTRICTION_TAGSIZE_MASK = &hc0
	ID3_TAG_RESTRICTION_TAGSIZE_128_FRAMES_1_MB = &h00
	ID3_TAG_RESTRICTION_TAGSIZE_64_FRAMES_128_KB = &h40
	ID3_TAG_RESTRICTION_TAGSIZE_32_FRAMES_40_KB = &h80
	ID3_TAG_RESTRICTION_TAGSIZE_32_FRAMES_4_KB = &hc0
end enum

enum 
	ID3_TAG_RESTRICTION_TEXTENCODING_MASK = &h20
	ID3_TAG_RESTRICTION_TEXTENCODING_NONE = &h00
	ID3_TAG_RESTRICTION_TEXTENCODING_LATIN1_UTF8 = &h20
end enum

enum 
	ID3_TAG_RESTRICTION_TEXTSIZE_MASK = &h18
	ID3_TAG_RESTRICTION_TEXTSIZE_NONE = &h00
	ID3_TAG_RESTRICTION_TEXTSIZE_1024_CHARS = &h08
	ID3_TAG_RESTRICTION_TEXTSIZE_128_CHARS = &h10
	ID3_TAG_RESTRICTION_TEXTSIZE_30_CHARS = &h18
end enum

enum 
	ID3_TAG_RESTRICTION_IMAGEENCODING_MASK = &h04
	ID3_TAG_RESTRICTION_IMAGEENCODING_NONE = &h00
	ID3_TAG_RESTRICTION_IMAGEENCODING_PNG_JPEG = &h04
end enum

enum 
	ID3_TAG_RESTRICTION_IMAGESIZE_MASK = &h03
	ID3_TAG_RESTRICTION_IMAGESIZE_NONE = &h00
	ID3_TAG_RESTRICTION_IMAGESIZE_256_256 = &h01
	ID3_TAG_RESTRICTION_IMAGESIZE_64_64 = &h02
	ID3_TAG_RESTRICTION_IMAGESIZE_64_64_EXACT = &h03
end enum

enum 
	ID3_TAG_OPTION_UNSYNCHRONISATION = &h0001
	ID3_TAG_OPTION_COMPRESSION = &h0002
	ID3_TAG_OPTION_CRC = &h0004
	ID3_TAG_OPTION_APPENDEDTAG = &h0010
	ID3_TAG_OPTION_FILEALTERED = &h0020
	ID3_TAG_OPTION_ID3V1 = &h0100
end enum

enum 
	ID3_FRAME_FLAG_TAGALTERPRESERVATION = &h4000
	ID3_FRAME_FLAG_FILEALTERPRESERVATION = &h2000
	ID3_FRAME_FLAG_READONLY = &h1000
	ID3_FRAME_FLAG_STATUSFLAGS = &hff00
	ID3_FRAME_FLAG_GROUPINGIDENTITY = &h0040
	ID3_FRAME_FLAG_COMPRESSION = &h0008
	ID3_FRAME_FLAG_ENCRYPTION = &h0004
	ID3_FRAME_FLAG_UNSYNCHRONISATION = &h0002
	ID3_FRAME_FLAG_DATALENGTHINDICATOR = &h0001
	ID3_FRAME_FLAG_FORMATFLAGS = &h00ff
	ID3_FRAME_FLAG_KNOWNFLAGS = &h704f
end enum

enum id3_field_type_enum
	ID3_FIELD_TYPE_TEXTENCODING
	ID3_FIELD_TYPE_LATIN1
	ID3_FIELD_TYPE_LATIN1FULL
	ID3_FIELD_TYPE_LATIN1LIST
	ID3_FIELD_TYPE_STRING
	ID3_FIELD_TYPE_STRINGFULL
	ID3_FIELD_TYPE_STRINGLIST
	ID3_FIELD_TYPE_LANGUAGE
	ID3_FIELD_TYPE_FRAMEID
	ID3_FIELD_TYPE_DATE
	ID3_FIELD_TYPE_INT8
	ID3_FIELD_TYPE_INT16
	ID3_FIELD_TYPE_INT24
	ID3_FIELD_TYPE_INT32
	ID3_FIELD_TYPE_INT32PLUS
	ID3_FIELD_TYPE_BINARYDATA
end enum

enum id3_field_textencoding
	ID3_FIELD_TEXTENCODING_ISO_8859_1 = &h00
	ID3_FIELD_TEXTENCODING_UTF_16 = &h01
	ID3_FIELD_TEXTENCODING_UTF_16BE = &h02
	ID3_FIELD_TEXTENCODING_UTF_8 = &h03
end enum

type id3_field__NESTED__binary
	type as id3_field_type_enum
	data as id3_byte_t ptr
	length as id3_length_t
end type

type id3_field__NESTED__immediate
	type as id3_field_type_enum
	value as zstring * 9
end type

type id3_field__NESTED__stringlist
	type as id3_field_type_enum
	nstrings as uinteger
	strings as id3_ucs4_t ptr ptr
end type

type id3_field__NESTED__string
	type as id3_field_type_enum
	ptr as id3_ucs4_t ptr
end type

type id3_field__NESTED__latin1list
	type as id3_field_type_enum
	nstrings as uinteger
	strings as id3_latin1_t ptr ptr
end type

type id3_field__NESTED__latin1
	type as id3_field_type_enum
	ptr as id3_latin1_t ptr
end type

type id3_field__NESTED__number
	type as id3_field_type_enum
	value as integer
end type

enum id3_file_mode
	ID3_FILE_MODE_READONLY = 0
	ID3_FILE_MODE_READWRITE
end enum

union id3_field
	type as id3_field_type_enum
	binary as id3_field__NESTED__binary
	immediate as id3_field__NESTED__immediate
	stringlist as id3_field__NESTED__stringlist
	string as id3_field__NESTED__string
	latin1list as id3_field__NESTED__latin1list
	latin1 as id3_field__NESTED__latin1
	number as id3_field__NESTED__number
end union

type id3_frame
	id as zstring * 5
	description as zstring ptr
	refcount as uinteger
	flags as integer
	group_id as integer
	encryption_method as integer
	encoded as id3_byte_t ptr
	encoded_length as id3_length_t
	decoded_length as id3_length_t
	nfields as uinteger
	fields as id3_field ptr
end Type

type id3_tag
	refcount as uinteger
	version as uinteger
	flags as integer
	extendedflags as integer
	restrictions as integer
	options as integer
	nframes as uinteger
	frames as id3_frame ptr ptr
	paddedsize as id3_length_t
end Type

Type id3_file As Any Ptr

declare function id3_file_open cdecl alias "id3_file_open" (byval as zstring ptr, byval as id3_file_mode) as id3_file ptr
declare function id3_file_fdopen cdecl alias "id3_file_fdopen" (byval as integer, byval as id3_file_mode) as id3_file ptr
declare function id3_file_close cdecl alias "id3_file_close" (byval as id3_file ptr) as integer
declare function id3_file_tag cdecl alias "id3_file_tag" (byval as id3_file ptr) as id3_tag ptr
declare function id3_file_update cdecl alias "id3_file_update" (byval as id3_file ptr) as integer
declare function id3_tag_new cdecl alias "id3_tag_new" () as id3_tag ptr
declare sub id3_tag_delete cdecl alias "id3_tag_delete" (byval as id3_tag ptr)
declare function id3_tag_version cdecl alias "id3_tag_version" (byval as id3_tag ptr) as uinteger
declare function id3_tag_options cdecl alias "id3_tag_options" (byval as id3_tag ptr, byval as integer, byval as integer) as integer
declare sub id3_tag_setlength cdecl alias "id3_tag_setlength" (byval as id3_tag ptr, byval as id3_length_t)
declare sub id3_tag_clearframes cdecl alias "id3_tag_clearframes" (byval as id3_tag ptr)
declare function id3_tag_attachframe cdecl alias "id3_tag_attachframe" (byval as id3_tag ptr, byval as id3_frame ptr) as integer
declare function id3_tag_detachframe cdecl alias "id3_tag_detachframe" (byval as id3_tag ptr, byval as id3_frame ptr) as integer
declare function id3_tag_findframe cdecl alias "id3_tag_findframe" (byval as id3_tag ptr, byval as zstring ptr, byval as uinteger) as id3_frame ptr
declare function id3_tag_query cdecl alias "id3_tag_query" (byval as id3_byte_t ptr, byval as id3_length_t) as integer
declare function id3_tag_parse cdecl alias "id3_tag_parse" (byval as id3_byte_t ptr, byval as id3_length_t) as id3_tag ptr
declare function id3_tag_render cdecl alias "id3_tag_render" (byval as id3_tag ptr, byval as id3_byte_t ptr) as id3_length_t
declare function id3_frame_new cdecl alias "id3_frame_new" (byval as zstring ptr) as id3_frame ptr
declare sub id3_frame_delete cdecl alias "id3_frame_delete" (byval as id3_frame ptr)
declare function id3_frame_field cdecl alias "id3_frame_field" (byval as id3_frame ptr, byval as uinteger) as id3_field ptr
declare function id3_field_type cdecl alias "id3_field_type" (byval as id3_field ptr) as id3_field_type_enum
declare function id3_field_setint cdecl alias "id3_field_setint" (byval as id3_field ptr, byval as integer) as integer
declare function id3_field_settextencoding cdecl alias "id3_field_settextencoding" (byval as id3_field ptr, byval as id3_field_textencoding) as integer
declare function id3_field_setstrings cdecl alias "id3_field_setstrings" (byval as id3_field ptr, byval as uinteger, byval as id3_ucs4_t ptr ptr) as integer
declare function id3_field_addstring cdecl alias "id3_field_addstring" (byval as id3_field ptr, byval as id3_ucs4_t ptr) as integer
declare function id3_field_setlanguage cdecl alias "id3_field_setlanguage" (byval as id3_field ptr, byval as zstring ptr) as integer
declare function id3_field_setlatin1 cdecl alias "id3_field_setlatin1" (byval as id3_field ptr, byval as id3_latin1_t ptr) as integer
declare function id3_field_setfulllatin1 cdecl alias "id3_field_setfulllatin1" (byval as id3_field ptr, byval as id3_latin1_t ptr) as integer
declare function id3_field_setstring cdecl alias "id3_field_setstring" (byval as id3_field ptr, byval as id3_ucs4_t ptr) as integer
declare function id3_field_setfullstring cdecl alias "id3_field_setfullstring" (byval as id3_field ptr, byval as id3_ucs4_t ptr) as integer
declare function id3_field_setframeid cdecl alias "id3_field_setframeid" (byval as id3_field ptr, byval as zstring ptr) as integer
declare function id3_field_setbinarydata cdecl alias "id3_field_setbinarydata" (byval as id3_field ptr, byval as id3_byte_t ptr, byval as id3_length_t) as integer
declare function id3_field_getint cdecl alias "id3_field_getint" (byval as id3_field ptr) as integer
declare function id3_field_gettextencoding cdecl alias "id3_field_gettextencoding" (byval as id3_field ptr) as id3_field_textencoding
declare function id3_field_getlatin1 cdecl alias "id3_field_getlatin1" (byval as id3_field ptr) as id3_latin1_t ptr
declare function id3_field_getfulllatin1 cdecl alias "id3_field_getfulllatin1" (byval as id3_field ptr) as id3_latin1_t ptr
declare function id3_field_getstring cdecl alias "id3_field_getstring" (byval as id3_field ptr) as id3_ucs4_t ptr
declare function id3_field_getfullstring cdecl alias "id3_field_getfullstring" (byval as id3_field ptr) as id3_ucs4_t ptr
declare function id3_field_getnstrings cdecl alias "id3_field_getnstrings" (byval as id3_field ptr) as uinteger
declare function id3_field_getstrings cdecl alias "id3_field_getstrings" (byval as id3_field ptr, byval as uinteger) as id3_ucs4_t ptr
declare function id3_field_getframeid cdecl alias "id3_field_getframeid" (byval as id3_field ptr) as zstring ptr
declare function id3_field_getbinarydata cdecl alias "id3_field_getbinarydata" (byval as id3_field ptr, byval as id3_length_t ptr) as id3_byte_t ptr
declare function id3_genre_index cdecl alias "id3_genre_index" (byval as uinteger) as id3_ucs4_t ptr
declare function id3_genre_name cdecl alias "id3_genre_name" (byval as id3_ucs4_t ptr) as id3_ucs4_t ptr
declare function id3_genre_number cdecl alias "id3_genre_number" (byval as id3_ucs4_t ptr) as integer
declare function id3_ucs4_latin1duplicate cdecl alias "id3_ucs4_latin1duplicate" (byval as id3_ucs4_t ptr) as id3_latin1_t ptr
declare function id3_ucs4_utf16duplicate cdecl alias "id3_ucs4_utf16duplicate" (byval as id3_ucs4_t ptr) as id3_utf16_t ptr
declare function id3_ucs4_utf8duplicate cdecl alias "id3_ucs4_utf8duplicate" (byval as id3_ucs4_t ptr) as id3_utf8_t ptr
declare sub id3_ucs4_putnumber cdecl alias "id3_ucs4_putnumber" (byval as id3_ucs4_t ptr, byval as uinteger)
declare function id3_ucs4_getnumber cdecl alias "id3_ucs4_getnumber" (byval as id3_ucs4_t ptr) as uinteger
declare function id3_latin1_ucs4duplicate cdecl alias "id3_latin1_ucs4duplicate" (byval as id3_latin1_t ptr) as id3_ucs4_t ptr
declare function id3_utf16_ucs4duplicate cdecl alias "id3_utf16_ucs4duplicate" (byval as id3_utf16_t ptr) as id3_ucs4_t ptr
declare function id3_utf8_ucs4duplicate cdecl alias "id3_utf8_ucs4duplicate" (byval as id3_utf8_t ptr) as id3_ucs4_t ptr

#define ID3_VERSION_MAJOR 0
#define ID3_VERSION_MINOR 15
#define ID3_VERSION_PATCH 1
#define ID3_VERSION_EXTRA " (beta)"
#define ID3_VERSION "0.15.1 (beta)"
#define ID3_PUBLISHYEAR "2000-2004"
#define ID3_AUTHOR "Underbit Technologies, Inc."
#define ID3_EMAIL "info@underbit.com"


#endif
