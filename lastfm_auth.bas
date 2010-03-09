#include "md5.bi"
#include "crt.bi"
#include "wshelper.bas"

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

Function percent_encode(message As String) As String
	Dim ret As String
	Dim i As Integer
	For i = 0 to Len(message) - 1
	Select Case(message[i])
		Case &h20 To &h2c, &h2f, &h3a, &h3b, &h3d, &h3f, &h40, &h5b, &h5d, &h80 to &hff
		ret &= "%" & Hex(message[i])
		Case Else
		ret &= Chr(message[i])
	End Select
	Next
	Return ret
End Function

Const artist = "Hollywood Undead"
Const album = "Swan Songs"
Const title = "Undead"
Const length = 265

Dim curtime As Integer
Dim user As String
Dim pass As String
Dim authkey As String
Dim httpdata As String
Dim response As ZString * 10000
Dim response_data As String
Dim sessionkey As String

Input "Username? ", user
Input "Password? ", pass

curtime = time_(NULL)
authkey = MD5str(MD5str(pass) & curtime)

httpdata = "GET /?hs=true&p=1.2.1&c=tst&v=1.0&u=" & user & "&t=" & curtime & "&a=" & authkey & " HTTP/1.1" & Chr(10) & "Host: post.audioscrobbler.com" & Chr(10) & "User-Agent: PsyMP3/1.1beta" & Chr(10) & Chr(10)

print httpdata

hStart()
Dim s As SOCKET, addr As Integer
s = hOpen()
addr = hResolve("post.audioscrobbler.com")
hConnect(s, addr, 80)
hSend(s, strptr(httpdata), len(httpdata))
hReceive(s, strptr(response), 10000)

print response

response_data = Mid(response, InStr(response, !"\r\n\r\n") + 4)
if left(response_data,3) = !"OK\n" Then
	print "Server response OK."
	sessionkey = Mid(response_data, 4, 32)
	print "Session key: " & sessionkey
End If

hClose(s)

httpdata = 	!"POST /np_1.2 HTTP/1.1\n" & _
		!"Host: post.audioscrobbler.com\n" & _ 
		!"User-Agent: PsyMP3/1.1beta\n" 
 
Dim As String postdata = _
		"s=" & sessionkey & "&" & _ 
		"a=" & percent_encode(artist) & "&" & _
		"t=" & percent_encode(title) & "&" & _ 
		"b=" & percent_encode(album) & "&" & _
		"l=" & length & "&" & _
		"n=&m="

httpdata &= _
		!"Content-Length: " & Len(postdata) & !"\n" & _
		!"Content-Type: application/x-www-form-urlencoded\n\n" & _
		postdata

print httpdata

hSend(s, strptr(httpdata), len(httpdata))
hReceive(s, strptr(response), 10000)
response_data = Mid(response, InStr(response, !"\r\n\r\n") + 4)

print response

if left(response_data,3) = !"OK\n" Then
	print "Server response OK. Track sucessfully sent as nowplaying!"
End If 

memset(strptr(response), 0, 10000)

Sleep 25000

httpdata = 	!"POST /protocol_1.2 HTTP/1.1\n" & _
		!"Host: post2.audioscrobbler.com\n" & _ 
		!"User-Agent: PsyMP3/1.1beta\n" 
 
postdata = 	"s=" & sessionkey & "&" & _ 
		"a[0]=" & percent_encode(artist) & "&" & _
		"t[0]=" & percent_encode(title) & "&" & _ 
		"i[0]=" & time_(NULL) & "&" & _ 
		"o[0]=P&" & _ 
		"r[0]=&" & _ 
		"l[0]=" & length & "&" & _
		"b[0]=" & percent_encode(album) & "&" & _
		"n[0]=&m[0]="

httpdata &= _
		!"Content-Length: " & Len(postdata) & !"\n" & _
		!"Content-Type: application/x-www-form-urlencoded\n\n" & _
		postdata

print httpdata

s = hOpen()
addr = hResolve("post2.audioscrobbler.com")
hConnect(s, addr, 80)

hSend(s, strptr(httpdata), len(httpdata))
hReceive(s, strptr(response), 10000)
response_data = Mid(response, InStr(response, !"\r\n\r\n") + 4)
if left(response_data,3) = !"OK\n" Then
	print "Server response OK. Track sucessfully scrobbled!"
End If 
