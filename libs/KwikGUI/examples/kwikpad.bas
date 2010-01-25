'***************************************
'    KwikPad - by Vincent DeCampo 2008   '
'***************************************
#Include      "file.bi"
#Include      "fbgfx.bi"
#Include      "KwikGUI.bi"

Declare Function MessageHandler(EventMsg As _EventMsg, EventData As _EventData) As Integer
Declare Function GetFileName(mGUI As _kGUI Ptr) As String

Dim Shared selection    As Integer

ScreenRes 640,480,32

Var GUI      = _kGUI(0,0,640,480)

WindowTitle "KwikPad"

Dim cMenu    As _kGUI_Control = _kGUI_Control(ctrlMENU,     @MessageHandler)
Dim cTextBox As _kGUI_Control = _kGUI_Control(ctrlMULTITEXT,@MessageHandler)

With cMenu
   .text =  "&File&&Open&&Save&&---------&&Quit"
   .tagname="MainMenu"
   .backcolor = &h707010
   GUI.AddControl(@cMenu)
End With

With cTextBox
   .x=4     'Relative to upper left of GUI not Screen
   .y=30    'Relative to upper left of GUI not Screen
   .dx=630  'Width
   .dy=440  'Height
   .backcolor = &he0e0e0
   .tagname="MultiText"
   .wrap = 1
   .border=0
   .text = ""
   .scrollbar=1
   .value=0
   GUI.AddControl(@cTextBox)
End With

Do

   GUI.ProcessEvents
   ScreenLock
   GUI.Render
   ScreenUnLock

   Select Case Selection
      Case 1   'Open File
         cTextBox.text = GUI.LoadTextFile(GetFileName(@GUI))
      Case 2   'Exit
         Exit Do
   End Select
   Selection = 0

   Sleep 3

Loop Until MultiKey(Fb.SC_ESCAPE)

Function GetFileName(mGUI As _kGUI Ptr) As String

   Var GUI      = _kGUI(160,120,320,120)

   Dim cTitle    As _kGUI_Control = _kGUI_Control(ctrlTITLEBAR, @MessageHandler)
   Dim btnOK     As _kGUI_Control = _kGUI_Control(ctrlBUTTON,   @MessageHandler)
   Dim btnCANCEL As _kGUI_Control = _kGUI_Control(ctrlBUTTON,   @MessageHandler)
   Dim cText     As _kGUI_Control = _kGUI_Control(ctrlTEXTBOX,  @MessageHandler)

   With cTitle
      .text =  "File Open"
      .backcolor = &h707010
      .forecolor = 0
      GUI.AddControl(@cTitle)
   End With

   With btnOK
      .x = 220
      .y = 40
      .dx = 75
      .dy = 25
      .justify = 2
      .text = "OK"
      .tagname = "OK"
      GUI.AddControl(@btnOK)
   End With

   With btnCANCEL
      .x = 220
      .y = 75
      .dx = 75
      .dy = 25
      .justify = 2
      .text = "Cancel"
      .tagname = "CANCEL"
      GUI.AddControl(@btnCANCEL)
   End With

   With cText
      .x = 20
      .y = 60
      .dx = 175
      .dy = 20
      .text = "kwikpad.bas"
      GUI.AddControl(@cText)
   End With

   GUI.border = 1
   GUI.alpha  = 200
   GUI.moveable = 1

   Do

      GUI.ProcessEvents
      ScreenLock
         mGUI->Render
         GUI.Render
      ScreenUnLock

      Select Case selection
         Case 3
            Return cText.text
         case 4
            Exit Do
      End Select

      Selection = 0

      Sleep 3

   Loop

End Function

Function MessageHandler(EventMsg As _EventMsg, EventData As _EventData) As Integer
   Dim As _kGUI_Control Ptr Ctrl = Cast(_kGUI_Control Ptr, EventData.Ctrl)

   With *Ctrl
      Select Case EventMsg
         Case Mouse_Over
         Case Mouse_Click
            Select Case .tagname
               Case "OK"
                  selection = 3
               Case "CANCEL"
                  selection = 4
            End Select
         Case Mouse_DblClick
         Case Mouse_Down
         Case Mouse_Up
         Case Ctrl_GotFocus
         Case Ctrl_LostFocus
         Case Timer_Event
         Case Menu_Select
            Select Case .seltext
               Case "Open"
                  selection = 1
               Case "Quit"
                  selection = 2
            End Select
         Case Combo_Select
      End Select
   End With

   Return 0

End Function
