'
' Test program to implement PsyMP3's bargraph in Windows console
'

#include once "crt/math.bi"

Randomize Timer

cls

Dim As String artist, album, song
Dim As Integer display(160,40), i	'Using quarter and half box charaters
Dim As Single Ptr sdata = Allocate(512 * sizeof(Single))

Sub ReduceSpectrum(ByVal spectrum As Single Ptr, ByVal spectrum2 As Single Ptr)
	Dim i As Integer
	Dim r As Single
	Dim s As Single
	For i = 0 to 79 
		spectrum2[i] = (spectrum[i*2] + spectrum[(i*2)+1]) / 2
	Next i 
End Sub

'
' Renders spectrum analyzer using background-colored spaces.
'
Sub RenderSpectrum Alias "RenderSpectrum" (ByVal spectrum As Single Ptr) 
	Dim As Integer graphdata(160), i, j
	For i = 0 to 79
		graphdata(i) = Int(spectrum[i])
	Next i
	' Use offscreen compositing
	'ScreenSet 1, 0
	For i = 0 to 79 ' attempt to render to console
		For j = 20 to (20 - graphdata(i)) step -1
			Locate j + 1, i + 1
			Select Case i
			Case Is < 30
				Color 7, 10
			Case 30 To 43
				Color 7, 9
			Case 44 To 55
				Color 7, 1
			Case 55 To 70
				Color 7, 5
			Case 71 To 79
				Color 7, 13
			Case Else
				Color 7, 7
			End Select
			Print " "
		Next j
		' Print "graphdata(" & i & ") = " & graphdata(i) & _
		' 	", spectrum[" & i & "] = " & spectrum[i]
		Color 7, 0
	Next i
	'ScreenSet 0, 0
	PCopy 1, 0
End Sub

Sub RenderTags(artist As String, title As String, album As String)
	
End Sub

For i = 0 to 82
	sdata[i] = rnd * 20.0
Next i 
RenderSpectrum(sdata)
Deallocate(sdata)
