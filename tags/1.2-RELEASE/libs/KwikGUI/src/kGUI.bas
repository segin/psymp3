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
'        Main GUI Structure
'---------------------------------------
Constructor _kGUI (x As Integer, y As Integer, dx As Integer, dy As Integer)

   This.Surface      = ImageCreate(dx,dy,,32)
   This.imgClrPick   = ImageCreate(240,200,32)
   This.x            = x
   This.y            = y
   This.dx           = dx
   This.dy           = dy
   This.nullctrl     = New _kGUI_Control(ctrlNONE,0)
   This.GotFocus     = This.nullctrl
   This.InitGrayScale_LUT
/'
   Create Color Picker image - Thanks to Mysoft!
'/
Dim As _RGB       HueTbl(255)
Dim As Integer    hidx=0
Dim As Integer    RA,GA,BA,RB,GB,BB,RR,GG,TON
Dim As _RGB       Hue(6) = {_
                           (255,0,0),_
                           (255,255,0),_
                           (0,255,0),_
                           (0,255,255),_
                           (0,0,255),_
                           (255,0,255),_
                           (255,0,0)_
                           }
   
   ' Creating HUE color table
   For h As Integer = 0 To 5
      RA=Hue(h).R
      GA=Hue(h).G
      BA=Hue(h).B
      RB=Hue(h+1).R
      GB=Hue(h+1).G
      BB=Hue(h+1).B
      For CNT As Integer = 0 To 39      
         With HueTbl(hidx)
         .R = Fraction(RA,RB,CNT,40)
         .G = Fraction(GA,GB,CNT,40)
         .B = Fraction(BA,BB,CNT,40)
         End With
         hidx += 1
      Next 
   Next   
      
   For x As Integer = 0 To 239
     With HueTbl(x)
       For y As Integer = -100 To 100
         If y < 0 Then TON = 255 Else TON=0
         RR = Fraction(.R,TON,Abs(y),100)
         GG = Fraction(.G,TON,Abs(y),100)
         BB = Fraction(.B,TON,Abs(y),100)      
         PSet This.imgClrPick,(x,y+100),RGB(RR,GG,BB)
       Next     
     End With
   Next 
   
   For x As Integer = 0 To 239 Step 10
      Line This.imgClrPick,(x,190)-Step(10,10),RGB(255-x,255-x,255-x),BF      
   Next
   
End Constructor

Destructor _kGUI
   Delete This.nullctrl
   ImageDestroy This.Surface
   ImageDestroy This.imgClrPick
End Destructor

Function _kGUI.AddControl (ctrlNEW As _kGUI_Control Ptr) As Integer
  
   With *ctrlNEW
   
      .GUI = @This
   
      If .ControlType <> ctrlPOPUP Then 
         This.SetControlFocus ctrlNEW
      End If
  
      'Give priority to menu bar and caption bar
      Select Case .ControlType
         Case ctrlTITLEBAR
            This.Control(0) = ctrlNEW 
            .x  = 0
            .y  = 0
            .dx = This.dx
            .dy = 25
            .Index = 0
            If Len(.tagname)=0 Then .tagname = "CTRL0"
            If This.Control(1) Then
               *This.Control(1).y = 25 '<--Move down menu bar is it exists
            End If
            Return 0
         Case ctrlMENU
            This.Control(1) = ctrlNEW
            .x  = 0
            .y  = IIf(This.Control(0)=0,0,25)
            .dx = This.dx
            .dy = 25
            .Index = 1
            If Len(.tagname)=0 Then .tagname = "CTRL1"            
            ParseMenuItems This.Menus(),.text
            Return 1
      End Select
   End With
      
   'Add control if not already added
   For c As Integer=2 To MAX_CTRL_INDEX
      If This.Control(c) = 0 Then
         This.Control(c) = ctrlNEW         
         'Perform any control auto adjustments here
         With *ctrlNEW
            Select Case .ControlType
               Case ctrlCHECKBOX,ctrlRADIO
                  If .size < 12 Then .size = 12
                  .dx = .size
                  .dy = .size
               Case ctrlFILELIST,ctrlDIRLIST
                  .value = 0
               Case ctrlCOLORPICK
                  .dx=240
                  .dy=200
               Case ctrlCOMBO
                  .selEnd = -1
            End Select
            .Index = c
            If Len(.tagname)=0 Then .tagname = "CTRL"+Str(C)
            .Refresh
         End With
         '----------------------------------------------------
         Return c
      EndIf
   Next
   Return -1
   
End Function

Sub _kGUI.RemoveControl(ctrl As _kGUI_Control Ptr)

   'Removes a control from the GUI
   For c As Integer=0 To MAX_CTRL_INDEX
      If ctrl = This.Control(c) Then
         This.Control(c) = 0
         Exit For
      End If
   Next
   
End Sub

Sub _kGUI.ClearControls()

   'Clear all controls from GUI
   For c As Integer=0 To MAX_CTRL_INDEX
      This.Control(c) = 0
   Next
      
End Sub

Function _kGUI.Duplicate(src As _kGUI_Control Ptr) As _kGUI_Control Ptr
Dim As _kGUI_Control Ptr newctrl

   newctrl = New _kGUI_Control(src->ControlType,src->MessageHandler)
   this.CopyProperties (src,newctrl)
   this.AddControl (newctrl)   
   Return newctrl
   
End Function

Sub _kGUI.CopyProperties(src As _kGUI_Control Ptr,dest As _kGUI_Control Ptr)

   With *src
      dest->alpha      = .alpha 
      dest->x          = .x     
      dest->y          = .y     
      dest->dx         = .dx     
      dest->dy         = .dy     
      dest->border     = .border    
      dest->backColor  = .backcolor
      dest->enabled    = .enabled 
      dest->foreColor  = .forecolor
      dest->interval   = .interval   
      dest->justify    = .justify
      dest->locked     = .locked   
      dest->min        = .min      
      dest->max        = .max      
      dest->path       = .path 
      dest->pattern    = .pattern  
      dest->picture    = .picture   
      dest->maskimage  = .maskimage
      dest->scrollbar  = .scrollbar
      dest->size       = .size     
      dest->sticky     = .sticky   
      dest->style      = .style
      dest->tag        = .tag      
      dest->text       = .text
      dest->trans      = .trans
      dest->value      = .value 
      dest->visible    = .visible 
      dest->wrap       = .wrap
   End With
   
End Sub

Sub _kGUI.ProcessEvents ()
Static captured As Integer
Dim As Integer sx,sy,mx,my,wheel,buttons,clip,cstart,cend
Dim As Integer newstate,processed,tx,ty,onctrl
Dim eData As _EventData
Dim char As String

   GetMouse sx,sy,wheel,buttons,clip
   
   eData.GUI    = @This
   eData.Button = buttons
   
   newstate = IIf(buttons=0,0,1)
   
   mx = sx - This.x
   my = sy - This.y
   
   'Save GUI coords in event data
   eData.Mx = mx
   eData.My = my

/'
   Special behavior when design mode
'/
   If This.designmode Then
      If This.GotFocus Then
         If This.GotFocus->captured Then
            'Process MouseOver event
            eData.Ctrl = This.GotFocus
            *This.GotFocus.MessageHandler (Ctrl_Captured,eData)
            This.GotFocus->captured=IIf(buttons=1,1,0)
            Exit Sub
         EndIf
      EndIf        
   End If

   cstart=0
   cend=MAX_CTRL_INDEX
   
   If captured Then
      cstart=captured
      cend=captured
      If buttons = 0 Then captured = 0
   End If
   
   'Cycle through all controls and process events
   For c As Integer = cstart To cend
      If This.Control(c) Then
         If *This.Control(c).Visible = 1 Then
            With *This.Control(c)
               
               eData.ctrl = This.Control(c)
               
               ._internal.mouseover=0
               
               'Check For Mouse Events when inside bounding box
               If (mx > .x) And (mx < .x+.dx) Or .captured>0 Then
                  If (my > .y) And (my < .y+.dy) Or .captured>0 Then
                     
                     onctrl=1
                     
                     ._internal.mouseover=1

                     Do

                        'See if control is disabled
                        If .enabled = 0 Then 
                           Exit Do
                        EndIf
                       
                        Select Case .ControlType
                           Case ctrlBUTTON
                              If .maskimage Then
                                 If MaskAlpha(Point(mx-.x,my-.y,.maskimage))<>0 Then
                                    Exit Do                                   
                                 EndIf
                              EndIf
                           Case ctrlPOPUP
                              'If This.control(c) <> This.GotFocus Then
                              '   Exit Do
                              'EndIf
                        End Select

                        'Process MouseOver event
                        *This.Control(c).MessageHandler (Mouse_Over,eData)

                        'If This.designmode Then Exit Sub
                     
                        If .state = 1 And newstate = 1 Then 'Holding Mouse Down
                           .captured = 1
                           captured  = c
                           Select Case .ControlType
                              Case ctrlTITLEBAR
                                 If This.moveable Then
                                    This.x = sx - This.xCapture
                                    This.y = sy - This.yCapture
                                    'Exit Sub
                                 End If
                              Case ctrlMENU,ctrlVSLIDER,ctrlHSLIDER
                                 Exit Sub                              
                           End Select                        
                        Else
                           .Captured = 0
                           captured  = 0
                        EndIf
                                             
                        If .state = 1 And newstate = 0 Then
                           'Process Click/MouseUp Event
                           *This.Control(c).MessageHandler (Mouse_Click,eData)
                           *This.Control(c).MessageHandler (Mouse_Up,eData)     
                           Select Case .ControlType
                              Case ctrlMENU,ctrlPOPUP
                                 If Len(.seltext) Then
                                    *This.Control(c).MessageHandler (Menu_Select,eData)                                    
                                    *This.Control(c).visible = IIf(.ControlType=ctrlPOPUP,0,1)
                                 EndIf
                           End Select                   
                        EndIf
   
                        If .state = 0 And newstate = 1 Then
                           'Check for DblClick-----------
                           If Timer-._internal.clicktime < .25 Then 
                              Select Case .ControlType
                                 Case ctrlDIRLIST,ctrlFILELIST
                                    'Dblclick event handled by control
                                 Case Else
                                    *This.Control(c).MessageHandler (Mouse_DblClick,eData)                                    
                              End Select
                              ._internal.clicktime=0
                              ._internal.dblclick=1
                           Else
                              ._internal.clicktime=Timer                              
                           EndIf
                           '-----------------------------
                           Select Case .ControlType
                              Case ctrlCHECKBOX,ctrlCOMBO,ctrlPOPUP,ctrlBUTTON,ctrlRADIO
                                 *This.Control(c).Value Xor= 1
                                 If .ControlType = ctrlRADIO Then
                                    If *This.Control(c).Value Then
                                       This.CheckRadios This.Control(c)
                                    EndIf
                                 EndIf                                 
                              Case ctrlMULTITEXT,ctrlTEXTBOX
                                 ._internal.mouseX  = mx-.x
                                 ._internal.mouseY  = my-.y
                                 ._internal.setflag = 1
                              'Case ctrlMENU,ctrlPOPUP
                              '   If .value And Len(.seltext) Then
                              '      *This.Control(c).MessageHandler (Menu_Select,eData)                                    
                              '      *This.Control(c).visible = IIf(.ControlType=ctrlPOPUP,0,1)
                              '   EndIf
                                 
                           End Select
   
                           'Process MouseDown Event
                           Select Case .ControlType
                              Case ctrlPOPUP,ctrlSHAPE
                                 If this.designmode Then
                                    SetControlFocus This.Control(c)
                                 EndIf
                              Case Else
                                 SetControlFocus This.Control(c)
                           End Select
                                                      
                           *This.Control(c).MessageHandler (Mouse_Down,eData)
                           
                           This.xCapture = mx
                           This.yCapture = my
                           
                        EndIf                    
   
                        .state    = newstate
                        processed = 1      
   
                     Loop Until (1)
                        
                  EndIf
               EndIf     
               
               If processed=0 Then
                  .state=0
               Else
                  processed=0
               EndIf               
               
               'Process Non-Mouse Events
               '------------------------
               Select Case .ControlType
                  Case ctrlTIMER
                     If .interval>0 And .enabled>0 Then
                        If (Timer-._internal.timecount) > .interval Then
                           *This.Control(c).MessageHandler (Timer_Event,eData)
                           ._internal.timecount = Timer
                        EndIf
                     End If
                  Case ctrlTEXTBOX,ctrlMULTITEXT
                     If This.Control(c)=This.GotFocus Then
                        KeyboardProc This.Control(c),this.CheckKey()
                     End If
                  Case ctrlCOMBO,ctrlCHECKBOX,ctrlRADIO,ctrlBUTTON,ctrlVSLIDER,ctrlHSLIDER
                     If This.Control(c)=This.GotFocus Then
                        ProcessKey This.Control(c),this.CheckKey(),eData
                     End If                     
                  Case Else
                     If this.KeyPeek Then
                        If This.Control(c)=This.GotFocus Then
                           char = this.CheckKey()
                        End If
                     EndIf
               End Select                        
               '-----------------------
               
            End With
         End If
      End If
   Next
  
   If buttons And onctrl=0 Then
      SetControlFocus This.nullctrl
      captured = 0
      If This.MessageHandler Then
         This.MessageHandler(Form_Click,eData)
      End If
   EndIf 
   
End Sub

Function _kGUI.LoadTextFile(ByVal Filename As String) As String
Dim hFile As Integer
Dim As String Temp,Buffer

   Buffer = ""

   If FileExists(filename) Then
      hFile = FreeFile
      Open Filename For Input As hFile
      While Not EOF(hFile)
         Line Input #hFile,Temp
         Buffer+=Temp+crlf
      Wend
      Close hFile
   EndIf 
   
   Return Buffer
  
End Function

Sub _kGUI.SetControlFocus (NewCtrl As _kGUI_Control Ptr)
Dim eData As _EventData

   If This.GotFocus <> NewCtrl Then

      Select Case NewCtrl->ControlType
         Case ctrlTEXTBOX,ctrlMULTITEXT
            ClearKeyboardBuffer()
      End Select

      With *This.GotFocus
         eData.ctrl = This.GotFocus
         .MessageHandler (ctrl_LostFocus,eData)
      End With
      This.GotFocus = NewCtrl
      With *This.GotFocus  
         eData.ctrl = NewCtrl
         .MessageHandler (ctrl_GotFocus,eData)
      End With
   EndIf
   
End Sub

Function _kGUI.GetGray (ByVal src As UInteger) As UInteger
Dim As Byte r,g,b,newclr
Dim As UInteger dest

   If maskalpha(src)=&hFF00FF Then Return src

Asm
      mov eax,[src]
      mov [r],al
      mov [g],ah
      Shr eax,8
      mov [b],ah
End Asm

   With This.gsLUT
      newclr = .r(r)+.g(g)+.b(b)
   End With

Asm
      mov al,[newclr]
      mov ah,al
      Shl eax,8
      mov al,ah
      mov [dest],eax
End Asm
   
   Return dest

End Function

Sub _kGUI.InitGrayScale_LUT()

    'Calculate GrayScale Conversion Tables
    For c As Single = 0 To 255
        this.gsLUT.r(c)  =  (c * .299)
        this.gsLUT.g(c)  =  (c * .587)
        this.gsLUT.b(c)  =  (c * .114)
    Next
    
End Sub

Function _kGUI.LoadBitmap (ByVal filename As String) As Any Ptr
Dim As Integer h,w,hf
Dim As Byte    head(1)
Dim As Any Ptr img

   Do
      If FileExists(filename) Then
         hf=FreeFile
         Open filename For Binary As hf
            Get #hf,,head()
            If head(0)=&h42 And head(1)=&h4d Then 'BM header
               Seek #hf,&h13
               Get  #hf,,w   'get width
               
               Seek #hf,&h17
               Get  #hf,,h   'get height
            Else
               Close #hf
               Exit Do
            End If            
         Close #hf 
         img = ImageCreate(w,h)
         BLoad filename,img
         Return img         
      Else
         Exit Do
      EndIf
   Loop   
      
End Function

Sub _kGUI.Console (ByVal text As String)
Dim fh As Integer = FreeFile
   
   Open cons For Output As #fh
      Print #fh, text
   Close #fh
   
End Sub

Function _kGUI.FindControl   (ByVal Tagname As String) As _kGUI_Control Ptr

   'Returns first control matching 'Tagname'
   For c As Integer=0 To MAX_CTRL_INDEX
      If This.Control(c) Then
         If This.Control(c)->tagname = Tagname Then 
            Return This.Control(c)
         End If
      End If
   Next
   
   Return 0

End Function

Function _kGUI.CheckKey() As String
Dim eData As _EventData
Dim As String temp

   temp = InKey
   If Len(temp) Then
      eData.GUI  = @This
      eData.ctrl = This.GotFocus
      eData.KeyCode = temp[0]
      If temp[0]=255 Then
         eData.ExtCode = temp[1]
      EndIf
      This.GotFocus->MessageHandler (Key_Press,eData)
      Return temp
   End If
   
End Function

Function _kGUI.KeyboardProc(ctrl As _kGUI_Control Ptr, ByVal char As String) As Integer
Dim As _kGUI Ptr GUI = ctrl->GUI
Dim As String  txt,lft,rgt
Dim As _EventData eData
      
   With *ctrl

      If Len(char)>0 Then
         
         Select Case .ControlType
            
            Case ctrlTEXTBOX,ctrlMULTITEXT 
               
               txt=.text
                           
               Select Case .selstart
                  Case 0
                     lft=""
                     rgt=txt
                  Case Is >= Len(txt)
                     lft=txt
                     rgt=""
                  Case Else
                     lft=Left(txt,.selstart)
                     rgt=Mid(txt,.selstart+1)
               End Select
               
               Select Case Char[0]
                  Case 3   'CTRL C
                     GUI->clipboard = txt
                  Case 22  'CTRL V
                     txt = GUI->clipboard
                  Case 24 'Ctrl x
                     txt = ""   
                  Case 9
                     If .ControlType = ctrlMULTITEXT Then
                        txt=lft+Chr(char[0])+rgt
                        .selstart+=1
                     EndIf
                  Case 28 To 128   
                     txt=lft+Chr(char[0])+rgt
                     .selstart+=1
                  Case 13
                     If .ControlType = ctrlMULTITEXT Then
                        txt=lft+Chr(13)+Chr(10)+rgt
                        .selstart+=2
                     End If               
                  Case 8   'Backspace
                     txt=Left(lft,Len(lft)-1)+rgt
                     .selstart-=1
                  Case 255
                     Select Case char[1] 'Process arrows *NEED TO ADD UP/DOWN
                        Case 75'left
                           .selstart-=1
                        Case 77'right
                           .selstart+=1
                        Case 83'delete
                           txt=lft+Mid(rgt,2)
                        Case 72 ' UP
                           If .ControlType = ctrlMULTITEXT Then
                              ._internal.setflag = 2
                              ._internal.mouseX  = ._internal.curx 
                              ._internal.mouseY  = ._internal.cury - 12
                           End If
                        Case 80 ' Down
                           If .ControlType = ctrlMULTITEXT Then
                              ._internal.setflag = 2
                              ._internal.mouseX  = ._internal.curx 
                              ._internal.mouseY  = ._internal.cury + 16
                           End If                     
                        Case 71  'Home
                           If .ControlType = ctrlMULTITEXT Then
                              ._internal.setflag = 3
                              ._internal.mouseX  = ._internal.curx 
                              ._internal.mouseY  = ._internal.cury + 16
                           Else
                              .selstart = 0
                           End If                                         
                        Case 79  'End
                           If .ControlType = ctrlMULTITEXT Then
                              ._internal.setflag = 4
                              ._internal.mouseX  = ._internal.curx 
                              ._internal.mouseY  = ._internal.cury + 16
                           Else
                              .selstart = Len(.text)
                           End If                     
                     End Select
                     
               End Select
   
               If .selstart<0 Then .selstart=0 'insert at beginning
               If .selstart>Len(txt) Then .selstart=Len(txt) 'insert at end
            
               .text=txt
            
         End Select
            
      End If
      
   End With   
      
   Return 0
   
End Function

Function _kGUI.ProcessKey(ctrl As _kGUI_Control Ptr,char As String,eData As _EventData) As Integer
Dim As _kGUI Ptr GUI = ctrl->GUI
Dim As String  txt,lft,rgt

      
   With *ctrl

      If Len(char)>0 Then
         
         Select Case char[0]
      
            Case 32
               .value Xor= 1
               .state = 1
               .MessageHandler (Mouse_Click,eData)
            Case 255
               
               Select Case char[1] 'Process arrows *NEED TO ADD UP/DOWN

                  Case 75'left
                     If .ControlType=ctrlHSLIDER Then
                        .value -= 1
                        .MessageHandler (Slider_Scroll,eData)
                     EndIf 
                  Case 77'right
                     If .ControlType=ctrlHSLIDER Then
                        .value += 1
                        .MessageHandler (Slider_Scroll,eData)
                     EndIf                      
                  Case 83'delete
                     
                  Case 72 ' UP
                     Select Case .ControlType   
                        Case ctrlVSLIDER   
                           .value -= 1   
                           .MessageHandler (Slider_Scroll,eData)
                        Case ctrlCOMBO   
                           .listindex -= 1   
                           .listindex  = IIf(.listindex<0,0,.listindex)
                           .MessageHandler (Combo_Select,eData)
                     End Select 
                  Case 80 ' Down
                     Select Case .ControlType   
                        Case ctrlVSLIDER   
                           .value += 1
                           .MessageHandler (Slider_Scroll,eData)
                        Case ctrlCOMBO
                           .listindex += 1
                           .listindex  = IIf(.listindex>(.GetItemCount-1),(.GetItemCount-1),.listindex)
                           .MessageHandler (Combo_Select,eData)
                     End Select
                  Case 71  'Home
                     .listindex = 0
                  Case 79  'End
                     .listindex = (.GetItemCount-1)
               End Select
                        
         End Select
            
      End If
      
      
      
   End With   
      
   Return 0
   
End Function

Sub _kGUI.GetLocalMouse(GUI As _kGUI Ptr, ctrl As _kGUI_Control Ptr, _
                        ByRef x As Integer=0,ByRef y As Integer=0, _
                        ByRef wheel As Integer=0,ByRef buttons As Integer=0)

   GetMouse x,y,wheel,buttons
   x -= GUI->x
   y -= GUI->y 
   x -= ctrl->x
   y -= ctrl->y                 
                  
End Sub

Sub _kGUI.CenterMe()
Dim As Integer w,h

   ScreenInfo w,h
   This.x = w/2 - This.dx/2
   This.y = h/2 - This.dy/2
   
End Sub

Sub _kGUI.Resize(dx As Integer,dy As Integer)
   ImageDestroy This.Surface
   This.Surface = ImageCreate(dx,dy,,32)  
   This.dx=dx
   This.dy=dy
End Sub

Sub _kGUI.CheckRadios(ctrl As _kGUI_Control Ptr)

   'Cycle through all controls and process radios
   For c As Integer = 0 To MAX_CTRL_INDEX
      If This.Control(c) Then
         If *This.Control(c).ControlType = ctrlRADIO Then
            If This.Control(c)<>ctrl Then
               This.Control(c)->value = 0
            End If
         End If
      End If
   Next
   
End Sub
