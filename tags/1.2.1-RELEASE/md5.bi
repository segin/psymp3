/'
 PsyMP3 libmd5 header

 Copyright © 1999 artofcode LLC.
 Copyright © 2009 Kirn Gill <segin2005@gmail.com>  

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
  
'/

#Ifndef __MD5_BI__
#Define __MD5_BI__

#Inclib "md5"

Type md5_byte_t As Byte
Type md5_word_t As Integer

Type md5_state_s 
   As md5_word_t count(2)
   As md5_word_t abcd(4)
   As md5_word_t buf(64)
End Type

Extern "C"
Declare Sub md5_init Alias "md5_init" (pms As md5_state_s Ptr)
Declare Sub md5_append Alias "md5_append" (pms As md5_state_s Ptr, Data As Const md5_byte_t Ptr, nbytes As Integer)
Declare Sub md5_finish Alias "md5_finish" (pms As md5_state_s Ptr, Digest As md5_byte_t Ptr)
End Extern
