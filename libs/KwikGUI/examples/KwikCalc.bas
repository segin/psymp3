'***************************************
'  KwikCalc - by Vincent DeCampo 2008  *
'***************************************
#Include      "fbgfx.bi"
#Include      "KwikGUI.bi"

Declare Function MessageHandler(EventMsg As _EventMsg, EventData As _EventData) As Integer
Declare Function ProcessCalc (nextop As String,entry As Double, result As Double) As String

Dim Shared selection As String

#Define ScrW      320
#Define ScrH      240
#Define MaxIdx    17
#Define BtnSize   40
#Define DIV_ERROR "DIV/0"
#Define _RoundButtons      '< comment this line for default buttons

   ScreenRes ScrW,ScrH,32
   
   'Create Main GUI Object
   Var GUI      = _kGUI(0,0,ScrW,ScrH)

   Dim btn(MaxIdx) As _kGUI_Control Ptr
   
   Dim As Integer offx = 40
   Dim As Integer offy = 15

#Ifdef _RoundButtons
   'Create round image mask
   Dim As Any Ptr mask = ImageCreate(BtnSize,BtnSize,&hFFFFFF,32)
   Circle mask,(BtnSize/2,BtnSize/2),BtnSize/3,0,,,,F
#EndIf
   
   'Create Buttons
   For x As Integer = 0 To MaxIdx
      btn(x) = New _kGUI_Control(ctrlBUTTON,@MessageHandler)
#Ifdef _RoundButtons
      btn(x) ->SetMaskImage(mask)
#EndIf
      GUI.AddControl(btn(x))
   Next
   
   'Create Display
   Dim cDisplay As _kGUI_Control = _kGUI_Control(ctrlLABEL,@MessageHandler)

   'Set Attributes of controls
   With cDisplay
      .x=35
      .y=15
      .dx=ScrW-70
      .dy=16
      .justify = 1 'Left
      .backcolor = 0
      .forecolor = &hFFFF00
      .text = "0"
      GUI.AddControl(@cDisplay)
   End With

   'Setup Buttons Positions
   For x As Integer = 0 To 17 
      With *btn(x)
         .text = Str(x)
         Select Case x
            Case 7 To 9
               .x=(x-7)*BtnSize
               .y=BtnSize
            Case 4 To 6
               .x=(x-4)*BtnSize
               .y=BtnSize*2
            Case 1 To 3
               .x=(x-1)*BtnSize
               .y=BtnSize*3
            Case 0
               .x=(x+1)*BtnSize
               .y=BtnSize*4
            Case 10
               .x=(x-10)*BtnSize + BtnSize*4
               .y=BtnSize
               .text = "C"
            Case 11
               .x=(x-10)*BtnSize + BtnSize*4
               .y=BtnSize
               .text = "CE"
            Case 12
               .x=(x-12)*BtnSize + BtnSize*4
               .y=BtnSize*2
               .text = "+"
            Case 13
               .x=(x-12)*BtnSize + BtnSize*4
               .y=BtnSize*2
               .text = "-"
            Case 14
               .x=(x-14)*BtnSize + BtnSize*4
               .y=BtnSize*3
               .text = "*"
            Case 15
               .x=(x-14)*BtnSize + BtnSize*4
               .y=BtnSize*3
               .text = "/" 
            Case 16
               .x=(x-16)*BtnSize + BtnSize*4
               .y=BtnSize*4
               .text = "="
            Case 17
               .x=(x-16)*BtnSize+ BtnSize*4
               .y=BtnSize*4  
               .text = "."            
         End Select
         .x += offx
         .y += offy 

#Ifndef _RoundButtons
         .dx = BtnSize
         .dy = BtnSize
#EndIf
         .forecolor = &hFFFFFF  
         .backcolor = &h209090    
         .justify = 2
      End With
   Next 

   WindowTitle "KwikCalc 1.0"

   Dim As Double entry,result
   Dim As String nextop,temp

   nextop = ""
      
Do

   selection = ""
   
   GUI.ProcessEvents
   ScreenLock
      GUI.Render
   ScreenUnLock

   If Len(selection) Then
      Select Case selection
         Case "C"
            entry  = 0
            result = 0
            temp   = ""
            nextop = ""
            cDisplay.text = "0"
         Case "CE"
            entry = 0
            temp  = ""
            cDisplay.text = "0"
         Case "+","-","/","*"            
            If nextop = "" Then
               entry  = CDbl(temp)
               result = entry
               entry = 0
            Else       
               entry  = CDbl(temp)
               temp   = ProcessCalc(nextop,entry,result)
               If temp = DIV_ERROR Then
                  entry  = 0
                  result = 0
                  nextop = ""    
                  cDisplay.text = DIV_ERROR        
               Else
                  Result = CDbl(temp)
                  cDisplay.text = Str(Result)
               EndIf      
            End If
            temp   = ""
            nextop = selection
         Case "="
            entry  = CDbl(temp)
            temp   = ProcessCalc(nextop,entry,result)
            If temp = DIV_ERROR Then
               entry  = 0
               result = 0
               cDisplay.text = DIV_ERROR
            Else
               Result = CDbl(temp)
               cDisplay.text = Str(Result)
            EndIf         
            nextop = ""
         Case Else
            If selection = "." And InStr(1,temp,".") Then
               Exit Select 'allow only one decimal
            EndIf
            temp += selection
            cDisplay.text = temp
      End Select
   End If
   
   Sleep 3,1

Loop Until MultiKey(Fb.SC_ESCAPE)

   'Delete Buttons
   For x As Integer = 0 To MaxIdx
      Delete btn(x)
   Next
   
#Ifdef _RoundButtons
   ImageDestroy mask
#endif

Function ProcessCalc (nextop As String,entry As Double, result As Double) As String
   Select Case nextop
      Case "+"
         Return Str(result+entry)
      Case "-"
         Return Str(result-entry)
      Case "*"
         Return Str(result*entry)
      Case "/"
         If entry=0 Then Return DIV_ERROR
         Return Str(result/entry)
   End Select
End Function

Function MessageHandler(EventMsg As _EventMsg, EventData As _EventData) As Integer
   Dim As _kGUI_Control Ptr Ctrl = Cast(_kGUI_Control Ptr, EventData.Ctrl)

   With *Ctrl
      Select Case EventMsg
         Case Mouse_Over
         Case Mouse_Click
            selection = .text
         Case Mouse_DblClick
         Case Mouse_Down
         Case Mouse_Up
         Case Ctrl_GotFocus
         Case Ctrl_LostFocus
         Case Timer_Event
         Case Menu_Select
         Case Combo_Select
      End Select
   End With

   Return 0

End Function
