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
/'
         _kGUI_Design Object
'/
Constructor _kGUI_Design (ByVal filename As String, ByVal pMH As Any Ptr)
Dim fh As Integer = FreeFile

   This.pMH = pMH

   If FileExists(filename) Then
      Open filename For Binary As fh
         ReadGUI (fh)
         ReadControls (fh)
      Close fh
   EndIf
   
End Constructor 

Destructor _kGUI_Design
   DestroyDesign 
End Destructor

Function _kGUI_Design.ReadGUI(filnum As Integer) As Integer
Dim As Integer x,y,dx,dy
Dim sTemp As String
Dim uTemp As UInteger
Dim res   As UInteger
 
   Do
      
      Input #filnum,sTemp
      
      If sTemp="<GUI>" Then
         Input #filnum,x
         Input #filnum,y
         Input #filnum,dx
         Input #filnum,dy
         
         this.dx = dx
         this.dy = dy
         
         CreateNewDesigner
         
         If This.GUI Then
            Input #filnum,This.GUI->Alpha  
            Input #filnum,This.GUI->BackColor
            Input #filnum,This.GUI->Border   
            Input #filnum,This.GUI->ForeColor
            Input #filnum,This.GUI->Moveable           
            Input #filnum,This.GUI->Trans    
            Input #filnum,This.GUI->designmode
            This.GUI->designmode=0  
            Return 1
         End If
         
         Exit Do
          
      EndIf
   
   Loop Until Eof(filnum)
   
   Return 0
   
End Function

/'
      Create New Designer
'/
Function _kGUI_Design.CreateNewDesigner () As Integer
   
   This.GUI = New _kGUI(100,0,this.dx,this.dy)
   
   Return 1
   
End Function

Function _kGUI_Design.ReadControls(filnum As Integer) As Integer
Dim ctrl  As _kGUI_Control Ptr
Dim sTemp As String
Dim uTemp As UInteger
   
   Do   
      
      Input #filnum,sTemp
      
      If Left(sTemp,5)="<CTRL" Then      
      
         Input #filnum,uTemp

         'Add New Control
         ctrl = AddControl (uTemp)
            
         Input #filnum,ctrl->alpha 
         Input #filnum,ctrl->x     
         Input #filnum,ctrl->y     
         Input #filnum,ctrl->dx     
         Input #filnum,ctrl->dy     
         Input #filnum,ctrl->border    
         Input #filnum,ctrl->backcolor
         Input #filnum,ctrl->enabled 
         Input #filnum,ctrl->forecolor
         Input #filnum,ctrl->interval   
         Input #filnum,ctrl->justify
         Input #filnum,ctrl->locked   
         Input #filnum,ctrl->min      
         Input #filnum,ctrl->max      
         Input #filnum,ctrl->path 
         Input #filnum,ctrl->pattern  
         Input #filnum,ctrl->size     
         Input #filnum,ctrl->sticky   
         Input #filnum,ctrl->tag      
         Input #filnum,ctrl->tagname
         Input #filnum,ctrl->text
         Input #filnum,ctrl->trans
         Input #filnum,ctrl->value 
         Input #filnum,ctrl->visible 
         Input #filnum,ctrl->wrap
         
         If sTemp="<CTRL_V2>" Then
            Input #filnum,ctrl->style
         EndIf
         
      End If
   
   Loop Until EOF(filnum)
   
   Return 1
      
End Function
/'
      Add a new control to designer
'/
Function _kGUI_Design.AddControl(cType As _CTRL_TYPE) As _kGUI_Control Ptr 

   Select Case cType
   
      Case ctrlLABEL To ctrlSHAPE
         
         If GUI Then
            For c As Integer = 0 To MAX_CTRL_INDEX
               If Ctrl(c)=0 Then
                  Ctrl(c)= New _kGUI_Control(cType,pMH)
                  GUI->AddControl(Ctrl(c))
                  Return Ctrl(c)
               End If
            Next 
         End If

   End Select
      
   Return 0
   
End Function

Sub _kGUI_Design.DestroyDesign ()

   If This.GUI Then
      For c As Integer = 0 To MAX_CTRL_INDEX
         If This.Ctrl(c) Then
            Delete This.Ctrl(c)
            This.Ctrl(c)=0
         End If
      Next
      Delete This.GUI
      This.GUI = 0
   End If
     
End Sub

