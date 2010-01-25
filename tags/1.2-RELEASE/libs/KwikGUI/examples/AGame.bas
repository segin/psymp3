/'
         (c) Vincent DeCampo 2008
      
          Game demo using KwikGUI
'/

#Include "fbgfx.bi"
#Include "KwikGUI.bi"

#Define MAX_STARS  300
#Define ScrW       400
#Define ScrH       300
#Define RndClr     (&hFFFFFF*Rnd)

Type _star
   As Single x = (Rnd*ScrW)
   As Single y = (Rnd*ScrH)
End Type

Type _starfield
   As _star Star(MAX_STARS)
End Type

Declare Function MsgHandler(EventMsg As _EventMsg, EventData As _EventData) As Integer

Sub RenderStars (dst As Any Ptr, SF As _starfield)
   
   For i As Integer = 0 To MAX_STARS
      With sf.Star(i)
         .y += 1
         If .y>ScrH Then .y=0
         PSet dst,(.x,.y),RndClr
      End With
   Next i
   
End Sub

   Dim Shared GUISelection As Integer

   ScreenRes ScrW,ScrH,32
   
   Dim MyGUI As _kGUI_Design = _kGUI_Design("game.kwk",@MsgHandler)
   
   MyGUI.GUI->x=0
   MyGUI.GUI->y=0
   MyGUI.GUI->backcolor = 0
   
   Var Menu    = MyGUI.GUI->FindControl("menu")
   Var Display = MyGUI.GUI->FindControl("picture")
   
   Menu->text = "&File&&New Game&&Exit&Help&&About"
   Menu->Refresh

   Display->picture = ImageCreate(400,275)


   Dim SF As _starfield
   
   Do
      ScreenLock
         Cls
         MyGUI.GUI->ProcessEvents
         MyGUI.GUI->Render
      ScreenUnLock

      Line Display->picture,(0,0)-(ScrW,ScrH),0,BF
      RenderStars (Display->picture, SF)
               
      Sleep 3,1
      
   Loop Until MultiKey(fb.SC_ESCAPE) Or (GUISelection=1)

Function MsgHandler(EventMsg As _EventMsg, EventData As _EventData) As Integer
Dim As _kGUI_Control Ptr Ctrl = Cast(_kGUI_Control Ptr, EventData.Ctrl)
Dim As _kGUI Ptr         GUI  = Cast(_kGUI Ptr, EventData.GUI)

   Select Case EventMsg
      Case Mouse_Over
      Case Mouse_Click
      Case Mouse_DblClick
      Case Mouse_Down
      Case Mouse_Up
      Case Ctrl_GotFocus
      Case Ctrl_LostFocus
      Case Timer_Event
      Case Menu_Select
         If Ctrl->seltext = "Exit" Then
            GUISelection = 1
         EndIf
      Case Combo_Select
      Case Slider_Scroll
      Case Ctrl_Refresh
      Case Key_Press
   End Select

   Return 0
   
End Function
   