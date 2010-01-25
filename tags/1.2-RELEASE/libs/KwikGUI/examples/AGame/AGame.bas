/'
         (c) Vincent DeCampo 2008
      
           A Game using KwikGUI
'/

#Include "AGame.bi"
   
   ScreenRes ScrW,ScrH,32
   
   Dim Shared GUISelection As Integer  
   Dim MyGUI As _kGUI_Design = _kGUI_Design("game.kwk",@MsgHandler)
   
   MyGUI.GUI->x=0
   MyGUI.GUI->y=0
   MyGUI.GUI->backcolor = 0
   
   Var Menu    = MyGUI.GUI->FindControl("menu")
   Var Display = MyGUI.GUI->FindControl("picture")
   
   Menu->text = "&File&&New Game&&Options&&Exit&Help&&About"
   Menu->Refresh

   Display->picture = ImageCreate(400,275)

   Dim As _thegame   TheGame
   Dim As Double     game_tick = 1/60
   Dim As Double     tick_time
   
   Do
      ScreenLock
         Cls
         MyGUI.GUI->ProcessEvents
         MyGUI.GUI->Render
      ScreenUnlock

      If Timer-tick_time>game_tick Then
         ProcessTheGame (Display->picture,@TheGame)
         tick_time=Timer
      End if
               
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
         Select Case Ctrl->seltext
            Case "Exit" 
               GUISelection = 1               
         End Select
      Case Combo_Select
      Case Slider_Scroll
      Case Ctrl_Refresh
      Case Key_Press
   End Select

   Return 0
   
End Function
   