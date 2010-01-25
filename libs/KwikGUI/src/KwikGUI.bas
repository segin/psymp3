/'--------------------------------------------------------------------
                  KwikGUI - GUI Library for FreeBASIC              
                     Copyright Vincent DeCampo 2008      

   This program is free software: you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public License
as published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

--------------------------------------------------------------------'/
#Define DEBUG_ENABLED
#Ifdef DEBUG_ENABLED
	#Define DPrint(Msg)  Open cons For Output As #99:Print #99, Msg:Close #99
#Else
	#Define DPrint(Msg):
#EndIf

#Define _MakeLib_

#Include Once  "fbgfx.bi"
#Include Once  "file.bi"
#Include Once  "dir.bi"
#Include       "kwikgui.bi"

#Ifndef crlf
   #Define crlf   !"\13\10"
#EndIf

#Define MaskAlpha(c)       (c Or &hFF000000 Xor &hFF000000) 
#define Fraction(A,B,P,M)  (A+((B-A)/M)*P)

Using FB

'---------------------------------------
'     Default Message Handler 
'---------------------------------------
Function DefaultMessageHandler(EventMsg As _EventMsg, EventData As _EventData) As Integer
   Return 0   
End Function

Declare Sub ClearKeyboardBuffer()

#Include "kControl.bas"
#Include "kGUI.bas"
#Include "kRender.bas"
#Include "kDesign.bas"

/'
   Parse Menu Control Text and fill array
'/
Sub ParseMenuItems (Menus() As String, temp As String)
Dim As String tmp,char,last
Dim As Integer n,i,s

   'Clear Menu Array
   For x As Integer = 0 To 10
      For y As Integer = 0 To 20
         Menus(x,y)=""      
      Next
   Next
      
   n=0
   i=1
   tmp=""

   Do
      char = Mid(temp,i,1)
      Select Case char
         Case "&"
            If last="&" Then
               Menus(n,s)=tmp
               s+=1
               tmp=""
            End If
         
         Case Else
   
            If last="&" And Len(tmp) Then
               Menus(n,s)=tmp
               tmp=""
               n+=1
               s=0
            End If                  

            tmp+=char                                      

      End Select

      last=char
      i+=1

   Loop Until i>Len(temp) 

   If Len(tmp) Then
      Menus(n,s)=tmp
   End If
   
End Sub
/'
   Selective DIR function - Thanks to counting_pine
'/
Function _Dir( _
                ByRef filespec As String = "", _
                ByVal need  As Integer = 0, _
                ByVal accept As Integer = -1, _
                ByRef attrout As Integer = 0) As String
   
Const fbAll = fbNormal Or fbHidden Or fbSystem Or fbDirectory
Dim Temp As String
   
    Temp = Dir(filespec, fbAll, attrout)
    While Len(Temp)
        If (attrout And need) = need Then _
            If (attrout And Not accept) = 0 Then _
                Return temp
        Temp = Dir(attrout)
    Wend
   
    Return ""
   
End Function

Sub ClearKeyboardBuffer()
Dim char As String

   Do
      char = InKey      
   Loop While Len(char)
   
End Sub