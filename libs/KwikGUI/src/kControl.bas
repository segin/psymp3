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

'---------------------------------------
'     Primary Control Structure
'---------------------------------------
Constructor _kGUI_Control (ByVal ctrlTYPE As _CTRL_TYPE, ByVal pMH As Any Ptr)
   MessageHandler = IIf (pMH=0,@DefaultMessageHandler,pMH)
   ControlType    = ctrlTYPE   
End Constructor
   
Destructor _kGUI_Control
   This.Clear()
End Destructor

Sub _kGUI_Control.Clear() 
   'Deallocate datalist resources
   If This.datalist Then 
      For x As Integer = 0 To This.datacount -1
         DeAllocate This.datalist[x].text
      Next
      This.datacount = 0
      DeAllocate This.datalist
      'Reset pointer to NULL
      This.datalist=0
   End If   
   '
End Sub

Function _kGUI_Control.AddItem (ByVal Text As String, ItemData As Integer=0) As Integer

   This.datacount+=1

   If This.datacount=1 Then
      This.datalist = Cast(_ItemList Ptr,Callocate(SizeOf(_ItemList)))
   Else
      This.datalist = Cast(_ItemList Ptr,ReAllocate(This.datalist,SizeOf(_ItemList)*datacount))
   EndIf
      
   With This.datalist[datacount-1]      
      .ItemData   = ItemData
      .text = Callocate(SizeOf(String))
      *.text = text
   End With
      
   Return This.datacount
      
End Function

Function _kGUI_Control.RemoveItem (Index As Integer) As Integer

   '-------Check for Index out of bounds--------
   If This.datacount < 1 Then Return (-1)
   If (Index < 0) Then Return (-1)
   If (Index > This.datacount-1) Then Return (-1)
   '---------------------------------------------
   
   Select Case This.datacount
      Case 1   'Only one item, just delete list
         If Index=0 Then
            DeAllocate This.datalist[0].text
            DeAllocate This.datalist
            This.datacount = 0 
         End If
      Case Is > 1 'More than once item, may need to shift items
         DeAllocate This.datalist[Index].text
         If Index <> This.datacount-1 Then 'If not last item, shift list
            For x As Integer = Index To This.datacount-2
               This.datalist[x] = This.datalist[x+1] 
            Next
         End If
         This.datacount -= 1
         This.datalist = Cast(_ItemList Ptr,ReAllocate(This.datalist,SizeOf(_ItemList)*datacount))         
   End Select
   
   Return datacount
   
End Function

Function _kGUI_Control.GetItem (Index As Integer) As String
   
   If Index<0 Or Index>(This.datacount-1) Then
      Return ""
   Else
      Return This.datalist[Index].text[0]
   End If   
   
End Function

Function _kGUI_Control.GetItemData (Index As Integer) As Integer
   
   If Index<0 Or Index>(This.datacount-1) Then
      Return 0
   Else
      Return This.datalist[Index].ItemData
   End If   
   
End Function

Function _kGUI_Control.GetItemCount () As Integer
   Return This.datacount  
End Function

Function _kGUI_Control.SetMaskImage (MaskImage As Any Ptr) As Integer
Dim FBimg As FB.IMAGE Ptr

   If MaskImage Then
      This.maskimage = MaskImage
      FBimg = Cast(FB.Image Ptr,MaskImage)
      This.dx = FBimg->Width
      This.dy = FBimg->height
      Return 1
   Else
      This.maskimage = 0
   End If
   
End Function

Sub _kGUI_Control.Refresh()
Dim As _EventData eData
Dim GUI As _kGUI Ptr

   Select Case This.ControlType
      Case ctrlDIRLIST
         This.LoadDirList         
      Case ctrlFILELIST
         This.LoadFileList
      Case ctrlMENU
         GUI = This.GUI
         ParseMenuItems GUI->Menus(),This.text
   End Select

   eData.GUI  = This.GUI
   eData.Ctrl = @This   
   This.MessageHandler (Ctrl_Refresh,eData)
   
End Sub

Sub _kGUI_Control.LoadDirList()
Dim As Integer need, accept, attrout
Dim As String StartDir = This.path
Dim As String temp,cDir

   cDir = CurDir 'Save initial position

   'Clear Old Data
   This.Clear
   'Set directory requirements
   need = fbDirectory: accept = -1
   'Set initial directory
   ChDir StartDir
   'Get directories   
   temp = _Dir("*", need, accept, attrout)
   While Len(temp)
       This.AddItem(temp)
       temp = _Dir( , need, accept, attrout )
   Wend
   'Store new path
   This.path = CurDir
   This.value = 0
   
   ChDir cDir 'return to original directory
   
End Sub

Sub _kGUI_Control.LoadFileList()
Dim As Integer need, accept, attrout
Dim As String temp,cDir
Dim As String StartDir = This.path
Dim As String fspec    = This.pattern
Dim As _kGUI Ptr GUI   = This.GUI

   cDir = CurDir 'Save initial position

   'Clear Old Data
   This.Clear
   'Set directory requirements
   need = 0: accept = Not fbDirectory
   'Set initial directory
   ChDir StartDir
   'Get files   
   temp = _Dir(fspec, need, accept, attrout)
   While Len(temp)
      This.AddItem(temp)
      temp = _Dir( , need, accept, attrout )
   Wend
   ChDir cDir 'return to original directory
   
End Sub

Function _kGUI_Control.RaiseEvent(EventMsg As _EventMsg, param As Any Ptr=0) As Integer
Dim As _kGUI Ptr GUI = This.GUI
Dim EventData As _EventData
Dim As Integer mx,my,button,wheel

   GUI->GetLocalMouse This.GUI,@This,mx,my,wheel,button   
   With EventData
      .GUI   = This.GUI
      .Ctrl  = @This
      .Mx    = mx
      .My    = my
      .param = param
   End With
   Return This.MessageHandler (EventMsg,EventData)
   
End Function

