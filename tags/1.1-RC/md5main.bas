#include "md5.bi"
#include "crt.bi"

Function MD5str (chars As String) As String
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
   Next
   Return md5hex
End Function

print MD5Str("")
print MD5str("a")
print MD5str("abc")
print MD5str("message digest")
