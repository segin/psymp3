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
Declare Sub DrawRoundRectangle( dst   As Any Ptr,_
                                x     As Integer,_
                                y     As Integer,_
                                dx    As Integer,_
                                dy    As Integer,_
                                clr   As UInteger=&hFFFFFF,_
                                round As Integer,_
                                fill  As Integer=0)

Declare Function NextWordIn(text As String, tpos As Integer) As Integer

Sub _kGUI.Render (Target As Any Ptr=0)

   'Draw GUI Background
   If This.Trans Then
      Line This.surface,(0,0)-(This.dx,This.dy),&hFF00FF,BF   
   Else   
      Line This.surface,(0,0)-(This.dx,This.dy),This.BackColor,BF
   EndIf
      
   If This.Picture Then
      Put This.surface,(0,0),This.Picture,PSet
   EndIf

   'Cycle through all controls and render them
   For c As Integer = 0 To MAX_CTRL_INDEX
      If This.Control(c) Then
         This.RenderControl (this.control(c))
      End If
   Next

   'Render focus control on top
   If This.GotFocus Then
      This.RenderControl (This.GotFocus)
      If This.GotFocus->showfocus Then
         If This.GotFocus->state = 0 then
            Line This.surface,(This.GotFocus->x,This.GotFocus->y)-Step(This.GotFocus->dx,This.GotFocus->dy),&hFFFF00,B,&hAAAA
         End if
      End if
   EndIf

   'Cycle again and menu & title bars
   For c As Integer = 0 To MAX_CTRL_INDEX
      If This.Control(c) Then
         If *This.Control(c).Visible = 1 Then
            With *This.Control(c)
               Select Case .ControlType
                  Case ctrlTITLEBAR                  
                     This.RenderTitleBar  This.Control(c)
                  Case ctrlMENU
                     This.RenderMenu      This.Control(c)
               End Select
            End With
         End If
      End If
   Next

   'Draw border if necessary
   If This.Border Then
      For w As Integer = 0 To This.Border
         Line This.surface,(w,w)-(This.dx-w-1,This.dy-w-1),This.ForeColor,B
      Next
   EndIf

   'Draw complete GUI to target
   Select Case This.Alpha
      Case 255
         If This.Trans Then
            Put Target,(This.x,This.y),This.surface,TRANS
         Else
            Put Target,(This.x,This.y),This.surface,PSet
         End If
      Case Else
         Put Target,(This.x,This.y),This.surface,ALPHA,This.Alpha
   End Select

End Sub

Sub _kGUI.RenderControl (ctrl As _kGUI_Control Ptr)

   If ctrl->visible = 0 Then Exit Sub

   With *ctrl
      Select Case .ControlType
         Case ctrlLABEL
            This.RenderLabel     ctrl
         Case ctrlTEXTBOX
            This.RenderTextBox   ctrl
         Case ctrlCHECKBOX
            This.RenderCheckBox  ctrl
         Case ctrlBUTTON
            This.RenderButton    ctrl
         Case ctrlPICTURE
            This.RenderPicture   ctrl       
         Case ctrlTEXTBLOCK
            This.RenderTextBlock ctrl
         Case ctrlHOTSPOT
            This.RenderHotSpot   ctrl
         Case ctrlVSLIDER
            This.RenderVSlider   ctrl
         Case ctrlHSLIDER
            This.RenderHSlider   ctrl         
         Case ctrlCOMBO
            This.RenderCombo     ctrl
         Case ctrlMULTITEXT
            This.RenderMultiText ctrl
         Case ctrlPROGRESS
            This.RenderProgress  ctrl          
         Case ctrlDIRLIST
            This.RenderDirList   ctrl
         Case ctrlFILELIST
            This.RenderFileList  ctrl
         Case ctrlLISTBOX
            This.RenderListBox   ctrl          
         Case ctrlCOLORPICK
            This.RenderColorPick ctrl    
         Case ctrlRADIO
            This.RenderRadio     ctrl
         Case ctrlSHAPE
            This.RenderShape     ctrl
         Case ctrlPOPUP
            This.RenderPopupMenu ctrl            
      End Select
   End With
   
End Sub

Sub _kGUI.RenderButton(Ctrl As _kGUI_Control Ptr)
Dim As Integer tx,ty,w,h,Offset,newclr,pushed
Dim As FB.Image Ptr FBimg
Dim As Any Ptr img

   With *Ctrl
            
      'Check for mask image
      If .maskimage Then
         FBimg = Cast(FB.Image Ptr, .maskimage)
         img = ImageCreate(FBimg->Width,FBimg->height,,32)
      Else
         img = ImageCreate(.dx+1,.dy+1,,32)
      EndIf
      
      'Determing if button is transparent
      If .trans Then
         Line img,(0,0)-Step(.dx,.dy),.BackColor,B
      Else
         newclr=IIf(.enabled=1,.backcolor,this.GetGray(.backcolor))
         For y As Integer=0 To .dy
            If y<.dy/2 Then
               newclr=AddClr(newclr,3)
            Else
               newclr=SubClr(newclr,3)
            EndIf
            Line img,(0,y)-(.dx,y),newclr
         Next
      EndIf
      
      'Draw border
      Line img,(0,0)-Step(.dx,.dy),0,B
      
      'If button has picture, draw it
      If .picture Then
         Offset=IIf(.maskimage>0,0,2)
         ImageInfo .picture,w,h
         If .state Then
            Put img,(.dx/2-w/2+Offset,.dy/2-h/2+Offset),.picture,TRANS
         Else
            Put img,(.dx/2-w/2,.dy/2-h/2),.picture,TRANS
         End If
      EndIf

      'Calc text position
      Select Case .justify
         Case 0
            tx = 3
         Case 1
            tx = .dx - Len(.text)*8 - 3
         Case 2   
            tx = (.dx/2) - Len(.text)*4 + 2            
      End Select

      ty = (.dy/2) - 3

      'Draw Button
      pushed = .state
      If .sticky Then
         pushed = IIf(.value>0,1,0)   
      EndIf
      
      If pushed Then
         tx+=IIf(.maskimage>0,0,2)
         ty+=IIf(.maskimage>0,0,2)
         If .maskimage = 0 Then
            Line img,(1,1)-Step(.dx-1,.dy-1),0,B
            Line img,(2,2)-Step(.dx-2,.dy-2),0,B
         End If
      Else
         If .maskimage = 0 Then
            Line img,(0,0)-Step(.dx,0),0
            Line img,(0,0)-Step(0,.dy),0
         End if   
      EndIf
                  
      Draw String img,(tx,ty),.text,.ForeColor
      
      If .maskimage Then
         Put img,(0,0),.maskimage,Or
         Put img,(0,0),.maskimage,Xor
         Put This.surface,(.x+pushed,.y+pushed),img,CUSTOM,@CustomButton,Cast(Any Ptr,ctrl)
      Else
         Put This.surface,(.x,.y),img,PSet
      End If
         
   End With
 
   ImageDestroy img

End Sub

Sub _kGUI.RenderLabel      (Ctrl As _kGUI_Control Ptr)
Dim As Integer tx,ty

   With *Ctrl
      
      Var img = ImageCreate(.dx+1,.dy+1,,32)
      
      Line img,(0,0)-Step(.dx,.dy),IIf(.trans  = 0,.BackColor,&hFF00FF),BF
      
      If .border Then
         'Line img,(0,0)-Step(.dx,.dy),IIf(.border = 1,.ForeColor,&hFF00FF),B
         Line img,(0,0)-Step(.dx,.dy),.ForeColor,B
      End If
      
      Select Case .justify
         Case 0
            tx = 3
         Case 1
            Tx = .dx - Len(.text)*8 - 3
         Case 2   
            tx = (.dx/2) - Len(.text)*4 
            
      End Select

      ty = (.dy/2) - 3
                  
      Draw String img,(tx,ty),.text,.ForeColor
      
      Put This.surface,(.x,.y),img,TRANS
           
   End With
 
   ImageDestroy img
   
End Sub

Sub _kGUI.RenderCombo      (Ctrl As _kGUI_Control Ptr)
Dim As Integer tx,ty,enabled
Dim As Integer r,mx,my,cnt,maxsel,scrollpress
Dim As _EventData eData
Dim As Integer ComboDY = 16
Dim As Integer barsize = 16

   With *Ctrl

      GetLocalMouse @This,Ctrl,mx,my

      .dy = ComboDY    'Fixed combo height
            
      Var img = ImageCreate(.dx+1,.dy+1,,32)      

      Line img,(0,0)-Step(.dx,.dy),IIf(.trans  = 0,.BackColor,&hFF00FF),BF
      Line img,(0,0)-Step(.dx,.dy),IIf(.border = 1,.ForeColor,&hFF00FF),B
      
      .text = Ctrl->GetItem(.listindex)

      Select Case .justify
         Case 0   :tx = 3
         Case 1   :tx = .dx - Len(.text)*8 - 3
         Case 2   :tx = (.dx/2) - Len(.text)*4            
      End Select

      ty = (.dy/2) - 3
      
      Draw String img,(tx,ty),.text,.ForeColor

      Line img,(.dx-15,1)-Step(ComboDY-2,ComboDY-2),&hc0c0c0,BF
      Draw String img,(.dx-11,ty),Chr(31),0
      
      Put This.surface,(.x,.y),img,TRANS

      ImageDestroy img

      If .sticky And (ctrl=This.GotFocus) Then
         If .scrollbar And .state  Then
            .value = IIf (mx > .dx-barsize,1,0) 
         EndIf
         enabled = .value 
      Else
         enabled = .state
         .value  = 0
      End If

      Select Case enabled
         Case 0   'Not Expanded
            .selStart = 0
            If .selEnd > -1 Then
               .text = Ctrl->GetItem(.selEnd)
               .listindex = .selEnd
               .selEnd = -1
               eData.Ctrl = Ctrl
               Ctrl->messageHandler (Combo_Select,eData)
            End If
         Case 1   'Expanded

            scrollpress = IIf (mx > .dx-barsize,1,0)
            
            r = Fix(my/ComboDY)-1 '12
            r = IIf(r>10,10,r)
            r = IIf(r<-1,-1,r)

            maxsel = Ctrl->GetItemCount-8
            
            If r=0 And scrollpress Then 
               If Timer-._internal.scrolltime > .1 Then
                  ._internal.scrolltime = Timer                  
                  .selstart -=1
                  .selstart=IIf(.selstart<0,0,.selstart)
               End If
            EndIf
            
            If r>6 And scrollpress Then 
               If Timer-._internal.scrolltime > .1 Then
                  ._internal.scrolltime = Timer
                  .selstart +=1
                  .selstart=IIf(.selstart > maxsel,maxsel,.selstart)
               End If
            EndIf
                                              
            .dy = 128
            Var ht = Ctrl->GetItemCount*16
            ht = IIf(ht>128,128,ht)
            'Var img = ImageCreate(.dx+1,.dy,,32)
            Var img = ImageCreate(.dx+1,ht+1,,32)
            
            Line img,(0,0)-Step(.dx,.dy),IIf(.trans  = 0,.BackColor,&hFF00FF),BF
            Line img,(0,0)-Step(.dx,.dy),IIf(.border = 1,.ForeColor,&hFF00FF),B
            
            'List items
            If Ctrl->GetItemCount > 0 Then
               cnt = 0
               .selend = -1
               Do                
                  If cnt=r Then
                     Line img,(0,cnt*ComboDY)-Step(.dx,ComboDY),0,BF
                     Draw String img,(3,4+cnt*ComboDY),Ctrl->GetItem(.selstart+cnt),&hFFFFFF
                     .selend = .selstart+cnt
                  Else
                     Draw String img,(3,4+cnt*ComboDY),Ctrl->GetItem(.selstart+cnt),0
                  EndIf
                  cnt += 1
               Loop Until (.selstart+cnt)>(Ctrl->GetItemCount-1) Or cnt>9
            End If

            If .scrollbar Then
      
               Line img,(.dx-barsize,0)-Step(barsize,.dy),&hAAAABB,BF
               Line img,(.dx-barsize+1,barsize)-Step(14,.dy-barsize*2),&hFFFFFF,BF
               Line img,(.dx-barsize,0)-Step(barsize,.dy),0,B
               
               Draw String img,(.dx-barsize+4,5),Chr(30),0
               Draw String img,(.dx-barsize+4,.dy-barsize+5),Chr(31),0
            
            End If
           
            Put This.surface,(.x,.y+ComboDY),img,TRANS
            
            .dy = 142
            
      End Select
                 
   End With
 
   
End Sub

Sub _kGUI.ActivatePopup   (index As Integer, x As Integer,y As Integer)
Dim As Integer cnt=0
   
   For c As Integer = 0 To MAX_CTRL_INDEX
      If This.Control(c) Then
         If *This.Control(c).ControlType = ctrlPOPUP Then
            If cnt=index Then
               *This.Control(c).x = x
               *This.Control(c).y = y
               *This.Control(C).value = 1
               *This.Control(C).visible = 1
               *This.Control(C).seltext = ""
               This.SetControlFocus This.Control(c)
               Exit Sub
            Else
               cnt+=1
            EndIf
         EndIf
      EndIf
   Next
   
End Sub

Sub _kGUI.RenderPopupMenu (Ctrl As _kGUI_Control Ptr)
Dim As Integer tx,ty,enabled
Dim As Integer r,mx,my,cnt,maxsel
Dim As Integer lheight = 12
Dim As _EventData eData

   With *Ctrl

      If This.GotFocus <> Ctrl Then Exit Sub
      If .GetItemCount() < 1 Then Exit Sub
      
      If .value = 0 Then
         Exit Sub
      EndIf
      
      GetLocalMouse @This,ctrl,mx,my

      .dy = (.GetItemCount() * lheight ) +lheight'/2
            
      Var img = ImageCreate(.dx+1,.dy+1,,32)      

      Line img,(0,0)-Step(.dx,.dy),IIf(.trans  = 0,.BackColor,&hFF00FF),BF
      For x As Integer = 0 To 20
         Line img,(x,0)-Step(0,.dy),AddClr(.BackColor,-x*10)
      Next           
      Line img,(0,0)-Step(.dx,.dy),IIf(.border = 1,.ForeColor,&hFF00FF),B
                 
      r = Fix(my/lheight)-1
      r = IIf(r>Ctrl->GetItemCount-1,Ctrl->GetItemCount-1,r)
      r = IIf(r<0,0,r)
            
      'List items
      cnt = 0
      Do       

         Select Case .justify
            Case 0
               tx = 24
            Case 1
               Tx = .dx - Len(Ctrl->GetItem(cnt))*8 - 3
            Case 2   
               tx = (.dx/2) - Len(Ctrl->GetItem(cnt))*4 
         End Select
         
         If cnt=r Then
            Line img,(0,cnt*lheight)-Step(.dx,lheight),0,BF
            Draw String img,(tx,4+cnt*lheight),Ctrl->GetItem(cnt),&hFFFFFF
            .selstart = cnt
            .seltext  = Ctrl->GetItem(cnt)
            .text     = .seltext
         Else
            Draw String img,(tx,4+cnt*lheight),Ctrl->GetItem(cnt),0
         EndIf
         cnt += 1
      Loop Until cnt > (Ctrl->GetItemCount-1)

      Select Case .Alpha
         Case 255
            Put This.surface,(.x,.y+lheight),img,TRANS
         Case Else
            Put This.surface,(.x,.y+lheight),img,ALPHA,.Alpha
      End Select
                
   End With
 
   
End Sub

Sub _kGUI.RenderHotSpot      (Ctrl As _kGUI_Control Ptr)

   With *Ctrl
      If .border Then
         Line This.surface,(.x,.y)-Step(.dx,.dy),0,B
      End If
   End With
   
End Sub

Sub _kGUI.RenderTextBox    (Ctrl As _kGUI_Control Ptr)
Dim As Integer tx,ty,curs,dist,lastdist

   With *Ctrl
      
      Var img = ImageCreate(.dx+1,.dy+1,,32)
      
      Line img,(0,0)-Step(.dx,.dy),.BackColor,BF
      
      If .border Then
         Line img,(0,0)-Step(.dx,.dy),.ForeColor,B
      End If

      If Ctrl = This.GotFocus Then
         Select Case Timer-This.CursorBlink
            Case 0 To .3
               curs = 1
            Case .3 To .6
               curs = 0
            Case Is > .6 
               This.CursorBlink = Timer
               curs = 1
         End Select
      Else
         curs = 0
      End If
      
      Select Case .justify
         Case 0
            tx = 3
         Case 1
            tx = .dx - Len(.text)*8 - 3
         Case 2   
            tx = (.dx/2) - Len(.text)*4 
      End Select

      ty = (.dy/2) - 3

      If Curs Then      
         Line img,(tx+(.selstart*8),ty-2)-Step(2,12),0,BF
      End If
      
      lastdist=.dx
      For char As Integer = 1 To Len(.text)+1
         Draw String img,(tx,ty),Mid(.text,char,1),.ForeColor
         'Calculate nearest character
         'to set new cursor position
         If ._internal.setflag=1 Then
            dist=Abs(tx-._internal.mouseX)
            If dist<lastdist Then
               .selstart=char-1
               lastdist=dist
            End If
         End If
         tx+=8
      Next
      
      ._internal.setflag=0
      
      Put This.surface,(.x,.y),img,PSet
           
   End With
 
   ImageDestroy img
  
End Sub

Sub _kGUI.RenderMultiText  (Ctrl As _kGUI_Control Ptr)
Dim As Integer tx,ty,col,row,maxcol,maxrow,mx,my
Dim As Integer curs,char,Start,lines,dist,wlen
Dim As Integer lastdist,lastchar,thischar,firstpos,lastpos
Dim As Integer showchar,showflag,flag,prevline
Dim As String  txt
Dim As Integer startline = -1

   With *Ctrl
      
      lastdist = (.dx*.dy)
      
      Var img = ImageCreate(.dx+1,.dy+1,,32)
      
      'Render Backgroung
      Line img,(0,0)-Step(.dx,.dy),.BackColor,BF
      
      'Render Border
      If .border Then
         Line img,(0,0)-Step(.dx,.dy),.ForeColor,B
      EndIf
      
      'Calculate max rows/cols displayed
      maxcol = .dx\8-IIf(.scrollbar=0,0,2)
      maxrow = .dy\12

      'If control has focus show cursor and time blinking                        
      If Ctrl = This.GotFocus Then
         Select Case Timer-This.CursorBlink
            Case 0 To .3
               curs = 1
            Case .3 To .6
               curs = 0
            Case Is > .6 
               This.CursorBlink = Timer
               curs = 1
         End Select
      Else
         curs = 0
      End If
 
      'If we are moving cursor, make it visible
      curs Or= ._internal.setflag
      
      If maxcol > 0 Then
         
         txt = .text & Chr(174)
         char = 1
         col  = 0
         row  = 0
         Start = -.value

         'Loop through characters and display
         'only the ones that will show in window 
         Do
            
            showchar=1
            
            thischar = Asc(Mid(txt,char,1))
            
            'Handle Tab/Lf/Cr
            Select Case thischar
               Case 9 'tab
                  showchar=0
               Case 10
                  showchar=IIf(lastchar=13,0,1)
               Case 13
                  showchar=0
            End Select           

            'Calculate char position
            tx = (col*8)
            ty = (row*12)
            
            'Calculate nearest character
            'to set new cursor position
            Select Case ._internal.setflag
               Case 1   'nearest char to mouse
                  dist=Sqr((tx-._internal.mouseX)^2 + (ty-._internal.mouseY)^2)
                  If dist<lastdist Then
                     .selstart=char-1
                     lastdist=dist
                  End If
               Case 2   'Row Up/Dn
                  If row = (._internal.mouseY\12) Then
                     dist=Sqr((tx-._internal.mouseX)^2 + (ty-._internal.mouseY)^2)
                     If dist<lastdist Then
                        .selstart=char-1
                        lastdist=dist
                     End If                     
                  EndIf      
               Case 3   'Home
                  If thischar=13 And char>=.selstart Then
                     .selstart=prevline+1
                     ._internal.setflag=0
                  EndIf                  
               Case 4   'End
                  If thischar=13 And char>=.selstart Then
                     .selstart=char-1
                     ._internal.setflag=0
                  EndIf
            End Select
                        
            'Here is where we show the characters on screen
            If showchar Then
               If Start >= 0 And Start<maxrow Then
                  Draw String img,(tx,ty),Chr(thischar),.forecolor
                  If startline < 0 Then startline = lines
                  If firstpos = 0 Then firstpos = char
                  If char>lastpos Then lastpos  = char
                  col +=1
                  'If wrap is enabled then check                 
                  If .wrap Then
                     wlen = IIf(thischar=32,NextWordIn(txt,char+1),0)
                     If col+wlen >= maxcol Then
                        col=0
                        row+=1
                        lines+=1
                        Start+=1
                     EndIf
                  EndIf
               End If
            End If

            'Check if we are at position to display cursor
            If (char-1=.selstart) Then 
               If curs And (._internal.setflag=0) Then
                  Line img,(tx,ty-2)-Step(2,12),0,BF
               End If
               showflag = 1
               ._internal.curx = tx
               ._internal.cury = ty    
            EndIf

            'Handle Tab/Lf/Cr
            Select Case thischar
               Case 9 'tab
                  col +=3
               Case 13
                  If Start >= 0 Then row+=1
                  col=0
                  Start+=1
                  lines+=1
                  prevline = char
            End Select           
            
            lastchar = thischar
            char+=1
                        
         Loop Until (char>Len(txt)) 

      EndIf

      ._internal.setflag = 0
      
      'Scroll text box
      If This.ProcessScrollbar(Ctrl,img,startline,lines) Then
         ._internal.setflag = 1
      Else
         'Check if we are scrolling with arrows
         If ._internal.mouseY<0 Then
            .value -=1
            ._internal.mouseY += 16
         EndIf
         If .selstart > lastpos Then
            .value +=1               
         End If
      End If   
      
      .value=IIf(.value>lines,lines,.value)
      .value=IIf(.value<0,0,.value)
      
      Put This.surface,(.x,.y),img,PSet
           
   End With
 
   ImageDestroy img
  
End Sub

Function _kGUI.ProcessScrollbar (Ctrl As _kGUI_Control Ptr, img As Any Ptr,posval As Integer,maxval As Integer) As Integer
Dim As Integer barsize = 16
Dim As Integer mx,my,mark 

   With *Ctrl

      If .scrollbar Then

         Line img,(.dx-barsize,0)-Step(barsize,.dy),&hAAAABB,BF
         Line img,(.dx-barsize+1,barsize)-Step(14,.dy-barsize*2),&hFFFFFF,BF

         If maxval<1 Then maxval=1
         mark = (posval/maxval)*.dy
         If mark < barsize Then mark=barsize+4
         If mark > .dy-barsize-4 Then mark=.dy-barsize-4
         
         Line img,(.dx-barsize,mark-5)-Step(barsize,10),0,BF
         Line img,(.dx-barsize,0)-Step(barsize,.dy),0,B
         
         Draw String img,(.dx-barsize+4,5),Chr(30),0
         Draw String img,(.dx-barsize+4,.dy-barsize+5),Chr(31),0
      
         Select Case .state
   
            Case 0   '      
            Case 1   'Mouse Down
   
               mx = This.xCapture -.x
               my = This.yCapture -.y

               If (mx > .dx-barsize) And (mx < .dx) Then   'Clicked on scroll bar
                  If (my < barsize) Then  'Scroll up one
                     .value-=1
                     .value=IIf(.value<0,0,.value)
                     Line img,(.dx-barsize,0)-Step(barsize,barsize),0,BF
                  End If
                  If (my > .dy-barsize) Then  'Scroll down one
                     .value+=1
                     Line img,(.dx-barsize,.dy-barsize)-Step(barsize,barsize),0,BF
                  End If
                  Return 1
               End If
               
         End Select
                        
      End If
   
   End With
   
   Return 0
   
End Function

Sub _kGUI.RenderCheckBox   (Ctrl As _kGUI_Control Ptr)
Dim As Integer tx,ty

   With *Ctrl
      .dx = .Size
      .dy = .Size
      Line This.surface,(.x,.y)-Step(.dx,.dy),0,BF
      Line This.surface,(.x+1,.y+1)-Step(.Size-2,.Size-2),.BackColor,BF
      If .value > 0 Then
         Select Case .style
            Case 0   'Standard X            
               Line This.surface,(.x,.y)-Step(.Size,.Size),0
               Line This.surface,(.x,.y+.Size)-Step(.Size,-.Size),0
            Case Else   'Block mark
               Line This.surface,(.x+2,.y+2)-Step(.Size-4,.Size-4),.ForeColor,BF
         End Select
      EndIf

      tx = .x+.Size+5 
      ty = .y+(.Size/2)-3 
      Draw String This.surface,(tx,ty),.text,.ForeColor   

   End With
   
End Sub

Sub _kGUI.RenderRadio(Ctrl As _kGUI_Control Ptr)
Dim As Integer tx,ty,cx,cy

   With *Ctrl
      
      Var img = ImageCreate(.Size+1,.Size+1,&hFF00FF,32)
      
      .dx = .size
      .dy = .size
      cx = (.size)\2
      cy = cx
      
      'Circle img,(cx,cy),.Size-5,.backcolor,,,,F
      'Line img,(0,0)-Step(.size,.size),.backcolor,BF
      'Circle img,(cx,cy),.Size,0
      Circle img,(cx,cy),.Size\3+2,.forecolor,,,,F
      Circle img,(cx,cy),.Size\3+1,&hFF00FF,,,,F      
      If .value > 0 Then
         Circle img,(cx,cy),.Size\3-1,.forecolor,,,,F
      EndIf

      tx = .x+.Size+5 
      ty = .y+((.Size+3)\2)-3 

      Put This.surface,(.x,.y),img,TRANS
      Draw String This.surface,(tx,ty),.text,.ForeColor
      
  End With
   
         
   ImageDestroy img
   
End Sub

Sub _kGUI.RenderTitleBar   (Ctrl As _kGUI_Control Ptr)
Dim newclr As UInteger

   With *Ctrl
      .x=0
      .y=0
      .dx=This.dx
      .dy=25
      
      If .trans = 0 Then
         newclr = .backcolor  
         For x As Integer = 0 To .dy
            If x<12 Then
               NewClr = AddClr(newclr,10)
            Else
               NewClr = SubClr(newclr,10)
            End If
            Line This.surface,(0,x)-Step(This.dx,0),newclr
         Next
         Draw String This.surface,(10,10),.text,.ForeColor
      End If
            
      If this.CloseBtn Then
         this.CloseBtn->x  = this.dx - (this.CloseBtn->dx + 5) 
         this.CloseBtn->y  = 12 - (this.CloseBtn->dy/2)
         this.RenderControl this.CloseBtn
      End if
      
   End With
   
End Sub

Sub _kGUI.RenderPicture   (Ctrl As _kGUI_Control Ptr)
Dim As FB.IMAGE Ptr img
Dim As Integer tx,ty,dx,dy

   With *Ctrl
  
      If .picture Then
      
         If .alpha<255 Then
            Put This.surface,(.x,.y),.picture,ALPHA,.alpha
         ElseIf .trans Then         
            Put This.surface,(.x,.y),.picture,TRANS
         Else
            Put This.surface,(.x,.y),.picture,PSet
         End If
         
         If Len(.text) Then
            
            img = .picture
            dx  = img->Width
            dy  = img->height
            
            Select Case .justify
               Case 0
                  tx = 3
               Case 1
                  tx = dx - Len(.text)*8 - 3
               Case 2   
                  tx = (dx/2) - Len(.text)*4 
                  
            End Select
      
            ty = (dy/2) - 3
                        
            Draw String This.surface,(.x+tx,.y+ty),.text,.foreColor
            
         EndIf

      End If

      If .Border Then         
         Line This.surface,(.x,.y)-Step(.dx,.dy),0,B
      EndIf
      
   End With
   
End Sub

Sub _kGUI.RenderMenu       (Ctrl As _kGUI_Control Ptr)
Dim As Integer n,r,mx,my,size,top
Dim As Integer enabled
Dim As UInteger NewClr

   With *Ctrl
      
      If .sticky Then
         enabled = IIf (Ctrl=This.GotFocus,1,0)
      Else
         enabled = .state
      End If

      top = IIf(This.Control(0)=0,0,25)
       
      NewClr = .backcolor  
      For x As Integer = 0 To 25
         If x<12 Then
            NewClr = AddClr(NewClr,10)
         Else
            NewClr = SubClr(NewClr,10)
         End If
         Line This.surface,(0,top+x)-Step(This.dx,0),NewClr
      Next
   
      For x As Integer=0 To 5
         Draw String This.surface,(x*56+4,top+10),This.Menus(x,0),.forecolor
      Next

      Select Case enabled

         Case 0   'Collapsed

            .dy=25

         Case 1   'Mouse Down

            GetMouse mx,my
            mx -= This.x
            my -= This.y + IIf(This.Control(0)=0,8,0)'Offset mouse if no title bar
            
            n = Fix(mx/56)
            n = IIf(n>10,10,n)
            n = IIf(n<0,0,n)
            
            r = Fix(my/16)-Fix(top/10)
            r = IIf(r>20,20,r)
            r = IIf(r<0,0,r)
            
            Size=1
            For x As Integer=1 To 20
               If Len(This.menus(n,x)) Then
                  Size+=1
               EndIf
            Next
            Size*=16
            .dy=Size
                           
            If Len(This.Menus(n,0)) Then 

               'Hightlight menu item on bar
               NewClr = .forecolor Xor &hFFFFFF
               Draw String This.surface,(n*56+4,top+10),This.Menus(n,0),NewClr
   
               'Render Drop down window
               Line This.surface,(n*56,top+25)-Step(112,Size),&hFFFFFF,BF
               
               newclr=.backcolor   
               For x As Integer = 0 To 20
                  newclr = AddClr(newclr,10)
                  Line This.surface,(n*56+x,top+25)-Step(0,Size),newclr
               Next           
   
               Line This.surface,(n*56,top+25)-Step(112,Size),0,B
   
               'List menu items            
               For x As Integer = 1 To 20
                  If Len(Menus(n,x)) Then
                     Draw String This.surface,(n*56+30,(top+15)+16*x),This.Menus(n,x),0
                  EndIf
               Next
               
               'Highlight selected item
               If r>0 And (16*r)<Size Then
                  Line This.surface,(n*56+20,(top+10)+16*r)-Step(92,16),0,BF
                  If Len(Menus(n,r)) Then
                     Draw String This.surface,(n*56+30,(top+15)+16*r),This.Menus(n,r),&hFFFFFF
                     .seltext = This.Menus(n,r)
                  Else
                     .seltext = ""
                  EndIf
               Else
                  .seltext = ""
               End If
                           
            End If
            
      End Select

      .x=0
      .y=top
      .dx=This.dx
      .dy=25
      
   End With
   
End Sub

Sub _kGUI.RenderTextBlock(Ctrl As _kGUI_Control Ptr)
Dim As Integer mx,my,tx,ty,char,col,row,maxcol,maxrow,Start,scrollbar,lines
Dim As Integer BarSize = 16

   With *Ctrl
      
      Var img = ImageCreate(.dx+1,.dy+1,,32)
      
      Line img,(0,0)-Step(.dx,.dy),.BackColor,BF
      
      If .border Then
         Line img,(0,0)-Step(.dx,.dy),.ForeColor,B
      End If
      
      maxcol = Fix(.dx/8)  '# of text columns
      maxrow = Fix(.dy/8)  '# of text rows
      
      scrollbar   = .scrollbar
      Lines       = 0
      
      If maxcol > 0 Then
         char = 1
         col  = 0
         row  = 0
         Start = -.value
         Do
            tx = (col*8)+4
            ty = (row*8)+4
            Select Case Asc(Mid(.text,char,1))
               Case 10
                  If Start >= 0 Then row+=1
                  Start+=1
                  lines+=1
               Case 13
                  col=0
               Case Else
                  If Start >= 0 And row < maxrow Then
                     Draw String img,(tx,ty),Mid(.text,char,1),.forecolor
                  End If
                  col +=1
            End Select
            
            char+=1
            If col=maxcol And .wrap Then
               col=0
               If Start >= 0 Then row+=1
               Start+=1
            EndIf
                  
         Loop Until char>Len(.text) 
      EndIf
           
      If scrollbar Then

         Line img,(.dx-barsize,0)-Step(barsize,.dy),&hAAAABB,BF
         Line img,(.dx-barsize+1,barsize)-Step(14,.dy-barsize*2),&hFFFFFF,BF
         Line img,(.dx-barsize,0)-Step(barsize,.dy),0,B
         
         Draw String img,(.dx-barsize+4,5),Chr(30),0
         Draw String img,(.dx-barsize+4,.dy-barsize+5),Chr(31),0
      
         Select Case .state
   
            Case 0   '      
            Case 1   'Mouse Down
   
               GetLocalMouse @This,ctrl,mx,my

               If (mx > .dx-barsize) And (mx < .dx) Then   'Clicked on scroll bar
                  If (my < barsize) Then  'Scroll up one
                     .value-=1
                     .value=IIf(.value<0,0,.value)
                     Line img,(.dx-barsize,0)-Step(barsize,barsize),0,BF
                  End If
                  If (my > .dy-barsize) Then  'Scroll down one
                     .value+=1
                     .value=IIf(.value>lines,lines,.value)
                     Line img,(.dx-barsize,.dy-barsize)-Step(barsize,barsize),0,BF
                  End If
               End If
         End Select
         
         'Allow keyboard only if active control
         If (Ctrl=This.GotFocus) Then
            If MultiKey(FB.SC_UP)=-1 Then   'Clicked on scroll bar
               .value-=1
               .value=IIf(.value<0,0,.value)
            End If
            If MultiKey(FB.SC_DOWN)=-1 Then   'Clicked on scroll bar                  
               .value+=1
               .value=IIf(.value>lines,lines,.value)
            End If
         End If

               
      End If
      
      Put This.surface,(.x,.y),img,PSet
      
   End With
   
   ImageDestroy img
   
End Sub

Sub _kGUI.RenderVSlider(Ctrl As _kGUI_Control Ptr)
Dim As _EventData eData
Dim As Integer mx,my,lastval
Dim As Single  sval

   With *Ctrl
      
      Var img = ImageCreate(.dx+1,.dy+1,,32)

      Line img,(0,0)-Step(.dx,.dy),.backcolor,bf
      Line img,(.dx/2-1,0)-Step(2,.dy),0,BF  'Draw Verticle bar
      
      If .border Then
         Line img,(0,0)-Step(.dx,.dy),0,b
      EndIf

      .value = IIf(.value<.min,.min,.value)
      .value = IIf(.value>.max,.max,.value)
      
      Select Case (.state Or .captured)

         Case 0   '      
         Case 1   'Mouse Down

            GetLocalMouse @This,ctrl,mx,my
         
            my = IIf(my<0,0,my)
            my = IIf(my>.dy,.dy,my)
            
            sval = my/.dy 
            
            lastval = .value
            
            .value =.min+(.max-.min)*sval 
            .value = IIf(.value<.min,.min,.value)
            .value = IIf(.value>.max,.max,.value)

            If lastval<>.value Then
               eData.GUI  = @This
               eData.Ctrl = Ctrl
               .MessageHandler (Slider_Scroll,eData)
            EndIf
        
      End Select
      
      sval = (.value-.min) / (.max-.min)
      
      If .size<1 Then .size=1
      Line img,(.dx/4,.dy*sval-.size)-Step(.dx/2,.size*2),.forecolor,BF
      Line img,(.dx/4,.dy*sval-.size)-Step(.dx/2,.size*2),0,B
      
      Put This.surface,(.x,.y),img,PSet
           
   End With
 
   ImageDestroy img
   
End Sub

Sub _kGUI.RenderHSlider(Ctrl As _kGUI_Control Ptr)
Dim As _EventData eData
Dim As Integer mx,my,lastval
Dim As Single  sval

   With *Ctrl
      
      Var img = ImageCreate(.dx+1,.dy+1,,32)

      Line img,(0,0)-Step(.dx,.dy),.backcolor,bf
      Line img,(0,.dy/2-1)-Step(.dx,2),0,BF  'Draw Verticle bar

      If .border Then
         Line img,(0,0)-Step(.dx,.dy),0,b
      EndIf

      .value = IIf(.value<.min,.min,.value)
      .value = IIf(.value>.max,.max,.value)
      
      Select Case (.state Or .captured)

         Case 0   '      
         Case 1   'Mouse Down

            GetLocalMouse @This,Ctrl,mx,my
         
            mx = IIf(mx<0,0,mx)
            mx = IIf(mx>.dx,.dx,mx)
            
            sval = mx/.dx 
            
            lastval = .value
            .value =.min+(.max-.min)*sval 
            .value = IIf(.value<.min,.min,.value)
            .value = IIf(.value>.max,.max,.value)
            If lastval<>.value Then
               eData.GUI  = @This
               eData.Ctrl = Ctrl
               .MessageHandler (Slider_Scroll,eData)
            EndIf
        
      End Select
      
      sval = (.value-.min) / (.max-.min)
      
      If .size<1 Then .size=1
      Line img,(.dx*sval-.size,.dy/4)-Step(.size*2,.dy/2),.forecolor,BF
      Line img,(.dx*sval-.Size,.dy/4)-Step(.size*2,.dy/2),0,B
      
      Put This.surface,(.x,.y),img,PSet
           
   End With
 
   ImageDestroy img
   
End Sub

Sub _kGUI.RenderProgress(Ctrl As _kGUI_Control Ptr)
Dim As Integer posval,tx,ty
Dim As UInteger newclr

   With *Ctrl
      
      If .dy<1 Then .dy=1 'So we dont get div/0 error
            
      Var img = ImageCreate(.dx+1,.dy+1,,32)

      Line img,(0,0)-Step(.dx,.dy),.backcolor,bf  

      .value = IIf(.value<0,0,.value)
      .value = IIf(.value>100,100,.value)

      posval = .dx * (.value/100)

      newclr = .forecolor
      For y As Integer = 0 To .dy
         If y<.dy/2 Then
            newclr = AddClr(newclr,10)
         Else
            newclr = SubClr(newclr,10)
         End If         
         Line img,(0,y)-Step(posval,0),newclr
      Next

      If Len(.text) Then
         Select Case .justify
            Case 0
               tx = 3
            Case 1
               Tx = .dx - Len(.text)*8 - 3
            Case 2   
               tx = (.dx/2) - Len(.text)*4 
         End Select
         ty = (.dy/2) - 3      
         Draw String img,(tx,ty),.text, &hFFFFFF,,Xor
      End If
      
      If .border Then
         Line img,(0,0)-Step(.dx,.dy),0,b
      EndIf

      Put This.surface,(.x,.y),img,PSet
           
   End With
 
   ImageDestroy img
             
End Sub

Sub _kGUI.RenderDirList(Ctrl As _kGUI_Control Ptr)
Dim As _EventData eData
Dim As Integer mx,my,tx,ty,char,col,row,cnt,r,scrolling
Dim As Integer maxcol,maxrow,Start,maxsel,buttons
Dim As Integer BarSize = 16
Dim As Integer lnspace = 12

   With *Ctrl
      
      Var img = ImageCreate(.dx+1,.dy+1,,32)
      
      Line img,(0,0)-Step(.dx,.dy),.BackColor,BF
      
      If .border Then
         Line img,(0,0)-Step(.dx,.dy),.ForeColor,B
      End If
      
      Do 
         
         If .GetItemCount < 1 Then Exit Do
         
         GetLocalMouse @This,Ctrl,mx,my,,buttons
         
         With eData        
            .GUI     = @This
            .Ctrl    = Ctrl
            .Mx      = mx
            .My      = my
            .Button  = buttons
         End With
         
         maxrow = Fix(.dy/lnspace)  '# of text rows
         maxsel = .GetItemCount-1
         .scrollbar  = Abs(maxsel>=maxrow)
   
         r = (my\lnspace)
         r = IIf(r<0,0,r)
         r = IIf(r>maxrow,maxrow,r)
         
         'Get new directory listing
         If ._internal.dblclick And mx<(.dx-barsize) Then
            If (.value+cnt)<(Ctrl->GetItemCount) Then
               .text = Ctrl->GetItem(.value+r)
               .path += "/"+Ctrl->GetItem(.value+r)
               .Refresh()
               Ctrl->MessageHandler (Mouse_DblClick,eData)
            EndIf
         EndIf
   
         cnt = 0
         
         Do       
            If cnt=r And ._internal.mouseover Then
               Line img,(0,cnt*lnspace)-Step(.dx,lnspace),0,BF
               Draw String img,(3,4+cnt*lnspace),Ctrl->GetItem(.value+cnt),&hFFFFFF
            Else
               Draw String img,(3,4+cnt*lnspace),Ctrl->GetItem(.value+cnt),0
            End If
            cnt += 1
         Loop Until (.value+cnt)>(Ctrl->GetItemCount-1) Or cnt>maxrow
   
         scrolling = This.ProcessScrollbar(Ctrl,img,.value,maxsel)
         .value=IIf(.value<0,0,.value)
         .value=IIf(.value>maxsel,maxsel,.value)
            
      Loop Until 1
    
      Put This.surface,(.x,.y),img,TRANS
          
      ._internal.dblclick=0 'reset flag
      
   End With
   

   ImageDestroy img
   
End Sub

Sub _kGUI.RenderFileList(Ctrl As _kGUI_Control Ptr)
Dim As _EventData eData
Dim As Integer mx,my,tx,ty,char,col,row,cnt,r,scrolling
Dim As Integer maxcol,maxrow,Start,maxsel,buttons
Dim As Integer BarSize = 16
Dim As Integer lnspace = 12

   With *Ctrl
      
      Var img = ImageCreate(.dx+1,.dy+1,,32)
      
      Line img,(0,0)-Step(.dx,.dy),.BackColor,BF
      
      If .border Then
         Line img,(0,0)-Step(.dx,.dy),.ForeColor,B
      End If
      
      Do
         
         If .GetItemCount() < 1 Then Exit Do
         
         GetLocalMouse @This,Ctrl,mx,my,,buttons
         
         With eData        
            .GUI     = @This
            .Ctrl    = Ctrl
            .Mx      = mx
            .My      = my
            .Button  = buttons
         End With
         
         maxrow = Fix(.dy/lnspace)  '# of text rows
         maxsel = .GetItemCount-1
         .scrollbar  = Abs(maxsel>=maxrow)
   
         r = (my\lnspace)
         r = IIf(r<0,0,r)
         r = IIf(r>maxrow,maxrow,r)
         
         'Click on listing
         If ._internal.dblclick And mx<(.dx-barsize) Then
            If (.value+cnt)<(Ctrl->GetItemCount) Then
               .text = Ctrl->GetItem(.value+r)
               Ctrl->MessageHandler (Mouse_DblClick,eData)
            EndIf
         EndIf
   
         cnt = 0
         
         Do       
            If cnt=r And ._internal.mouseover Then
               Line img,(0,cnt*lnspace)-Step(.dx,lnspace),0,BF
               Draw String img,(3,4+cnt*lnspace),Ctrl->GetItem(.value+cnt),&hFFFFFF
            Else
               Draw String img,(3,4+cnt*lnspace),Ctrl->GetItem(.value+cnt),0
            End If
            cnt += 1
         Loop Until (.value+cnt)>(Ctrl->GetItemCount-1) Or cnt>maxrow
   
         scrolling = This.ProcessScrollbar(Ctrl,img,.value,maxsel)
         .value=IIf(.value<0,0,.value)
         .value=IIf(.value>maxsel,maxsel,.value)
   
      Loop Until 1
            
      Put This.surface,(.x,.y),img,TRANS
   
      ._internal.dblclick=0 'reset flag
      
   End With
   

   ImageDestroy img
   
End Sub

Sub _kGUI.RenderListBox    (Ctrl As _kGUI_Control Ptr)
Dim As _EventData eData
Dim As Integer mx,my,tx,ty,char,col,row,cnt,r,scrolling
Dim As Integer maxcol,maxrow,Start,maxsel,buttons
Dim As Integer BarSize = 16
Dim As Integer lnspace = 12

   With *Ctrl
      
      Var img = ImageCreate(.dx+1,.dy+1,,32)
      
      Line img,(0,0)-Step(.dx,.dy),.BackColor,BF
      
      If .border Then
         Line img,(0,0)-Step(.dx,.dy),.ForeColor,B
      End If
      
      Do
         
         If .GetItemCount() < 1 Then Exit Do
         
         GetLocalMouse @This,Ctrl,mx,my,,buttons
         
         With eData        
            .GUI     = @This
            .Ctrl    = Ctrl
            .Mx      = mx
            .My      = my
            .Button  = buttons
         End With
         
         maxrow = Fix(.dy/lnspace)  '# of text rows
         maxsel = .GetItemCount-1
         .scrollbar  = Abs(maxsel>=maxrow)
   
         r = (my\lnspace)
         r = IIf(r<0,0,r)
         r = IIf(r>maxrow,maxrow,r)
         
         'Click on listing
         If ._internal.dblclick And mx<(.dx-barsize) Then
            If (.value+cnt)<(Ctrl->GetItemCount) Then
               .text = Ctrl->GetItem(.value+r)
               Ctrl->MessageHandler (Mouse_DblClick,eData)
            EndIf
         EndIf
   
         cnt = 0
         
         Do       
            If cnt=r And ._internal.mouseover Then
               Line img,(0,cnt*lnspace)-Step(.dx,lnspace),0,BF
               Draw String img,(3,4+cnt*lnspace),Ctrl->GetItem(.value+cnt),&hFFFFFF
            Else
               Draw String img,(3,4+cnt*lnspace),Ctrl->GetItem(.value+cnt),0
            End If
            cnt += 1
         Loop Until (.value+cnt)>(Ctrl->GetItemCount-1) Or cnt>maxrow
   
         scrolling = This.ProcessScrollbar(Ctrl,img,.value,maxsel)
         .value=IIf(.value<0,0,.value)
         .value=IIf(.value>maxsel,maxsel,.value)
   
      Loop Until 1
            
      Put This.surface,(.x,.y),img,TRANS
   
      ._internal.dblclick=0 'reset flag
      
   End With
   
   ImageDestroy img
   
End Sub

Sub _kGUI.RenderColorPick(Ctrl As _kGUI_Control Ptr)
Dim As Integer mx,my

   With *Ctrl
      
      Put This.surface,(.x,.y),This.imgClrPick,PSet
      
      If .Border Then
         Line This.surface,(.x,.y)-Step(.dx,.dy),0,B
      EndIf
      
      If .state Then
         GetLocalMouse (@This,Ctrl,mx,my)
         .text = Hex(MaskAlpha(Point(mx,my,This.imgClrPick)))
         .text = String(6-Len(.text),"0")+.text
      EndIf
      
   End With
   
End Sub

Sub _kGUI.RenderShape(ctrl As _kGUI_Control Ptr)
   
   With *ctrl
      Select Case .style
         Case 0   'Rectangle
            Line This.surface,(.x,.y)-Step(.dx,.dy),.forecolor,B
         Case 1   'Rounded Rectangle
            DrawRoundRectangle (This.surface,.x,.y,.dx,.dy,.forecolor,15)
      End Select   
   End With
   
End Sub
/'
   Draw Rounded Corner   
'/
Sub DrawCorner(dst As Any Ptr,x As Integer,y As Integer,r As Integer,clr As UInteger=&hFFFFFF,corner As Integer)
Dim As Single aStart,aEnd

   Select Case corner
      Case 0   'upper left
         aStart=(pi/2):aEnd=pi
      Case 1   'upper right
         aStart=0:aEnd=(pi/2)
      Case 2   'lower right
         aStart=pi*2-(pi/2):aEnd=pi*2
      Case 3   'lower left
         aStart=pi:aEnd=pi*2-(pi/2)
   End Select

   Circle dst,(x,y),r,clr,aStart,aEnd
         
End Sub
/'
   Draw Rounded Rectangle   
'/
Sub DrawRoundRectangle( dst   As Any Ptr,_
                        x     As Integer,_
                        y     As Integer,_
                        dx    As Integer,_
                        dy    As Integer,_
                        clr   As UInteger=&hFFFFFF,_
                        round As Integer,_
                        fill  As Integer=0)
                        
   'Draw straight segments
   Line dst,(x+round,y)   -Step(dx-round*2,0),clr
   Line dst,(x+round,y+dy)-Step(dx-round*2,0),clr
   Line dst,(x,y+round)   -Step(0,dy-round*2),clr
   Line dst,(x+dx,y+round)-Step(0,dy-round*2),clr
   'Draw arc segments
   DrawCorner (dst,x+round,y+round,round,clr,0)
   DrawCorner (dst,x+dx-round,y+round,round,clr,1)
   DrawCorner (dst,x+dx-round,y+dy-round,round,clr,2)
   DrawCorner (dst,x+round,y+dy-round,round,clr,3)
   
   If fill Then
      Paint dst,(x+round,y+round),clr
   End If
   
End Sub
/'
   Custom Button PUT   
'/
Function CustomButton (ByVal src As UInteger, ByVal dest As UInteger, ByVal param As Any Ptr ) As UInteger
Dim As _kGUI_Control Ptr ctrl = Cast(_kGUI_Control Ptr, param)
Dim As _kGUI Ptr GUI = Cast(_kGUI Ptr,ctrl->GUI)
Dim As Integer pushed
Static hilo As Integer

   With *ctrl
      
      pushed=.state
      If .sticky Then
         pushed = IIf(.value>0,1,0)
      EndIf
      
      If pushed Then
         Select Case src
            Case Is < 10
               If hilo Then
                  hilo=0
                  Return &hFFFFFF
               EndIf
                  Return dest
            Case Else
               If hilo=0 Then
                  hilo=1
                  Return 0
               EndIf
               If .enabled Then
                  Return src
               Else
                  Return GUI->GetGray(src)
               End If
         End Select
      Else
         Select Case src
            Case Is < 10
               If hilo Then
                  hilo=0
                  Return 0
               EndIf
               Return dest
            Case Else
               If hilo=0 Then
                  hilo=1
                  Return &hFFFFFF
               EndIf
               If .enabled Then
                  Return src
               Else
                  Return GUI->GetGray(src)
               End If
         End Select
      End If
   End With
   
End Function

/'
   Change RGB components of color by adding (x) amount
'/
Function AddClr (clr As UInteger, delta As UByte) As UInteger

Asm
   emms
   Xor ebx,ebx
   mov al,[delta]
   mov bl,al
   Shl ebx,8
   mov bl,al
   Shl ebx,8
   mov bl,al
   movd mm0,ebx
   movd mm1,[clr]
   paddusb mm0,mm1
   movd [clr],mm0
   emms
End Asm
      
   Return clr
   
End Function
/'
   Change RGB components of color by subtracting (x) amount
'/
Function SubClr (clr As UInteger, delta As UByte) As UInteger

Asm
   emms
   Xor ebx,ebx
   mov al,[delta]
   mov bl,al
   Shl ebx,8
   mov bl,al
   Shl ebx,8
   mov bl,al
   movd mm0,ebx
   movd mm1,[clr]
   psubusb mm1,mm0
   movd [clr],mm1
   emms
End Asm
      
   Return clr
   
End Function

Function NextWordIn(text As String, tpos As Integer) As Integer
Dim As Integer wpos

   For pchar As Integer = tpos-1 To Len(text)-1
      If text[pchar]=32 Then
         Return wpos
      Else
         wpos+=1
      EndIf
   Next

   Return wpos

End Function