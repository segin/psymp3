'***************************************
'    KwikGUI - by Vincent DeCampo 2008   '        
'***************************************

#Include      "fbgfx.bi"
#Include      "KwikGUI.bi"

Declare Function MessageHandler(EventMsg As _EventMsg, EventData As _EventData) As Integer

Dim Shared selection As Integer
Dim Shared progress  As Integer Ptr
   
   ScreenRes 800,600,32      

   #Include "gui_setup.bas" '<--This is where we declare
                               'all the gui elements
      
   Do

      selection = 0
                  
      GUI.ProcessEvents
      ScreenLock
         GUI.Render 
      ScreenUnlock   

      'Update time on dialog            
      cLabel.text = cTimer.text 
      
      'Update progress bar text
      cProgress.text = "Loading "+Str(cProgress.value)+"%" 
      
      'Set for exit if ESC pressed
      If (MultiKey(FB.SC_ESCAPE)=-1) Then
         selection = 1
      EndIf
      
      'Process our selection
      '
      Select Case selection
         Case 1   'Exit
            Exit Do   
         Case 2   'Update File control
            cFile.Path = cDir.Path
            cFile.Refresh()
         Case 3
            If cCheck.value = 1 Then
               cbutton.SetMaskImage(0)
            Else
               cButton.SetMaskImage(mask)
            EndIf
         Case 4
            cPicture.alpha = cVSlider.value
         Case 5
            cListBox.AddItem cFile.text
      End Select
      
      Sleep 3
           
   Loop

Function MessageHandler(EventMsg As _EventMsg, EventData As _EventData) As Integer
Dim As _kGUI_Control Ptr Ctrl = Cast(_kGUI_Control Ptr, EventData.Ctrl)
Dim As _kGUI Ptr GUI = Cast(_kGUI Ptr, EventData.GUI)

   With *Ctrl
      Select Case EventMsg
         Case Mouse_Over
         Case Mouse_Click
            GUI->Console ("Mouse_Click->value="+Str(.value))
            If .tagname="CheckBox" Then
               selection = 3
            EndIf
         Case Mouse_DblClick
            If .tagname = "FILELIST" Then
               GUI->Console ("File Selected="+.text)
               selection = 5
            EndIf            
         Case Mouse_Down
            If EventData.Button = 2 Then
               'Activate 1st popup added to GUI @ x,y coords
               GUI->ActivatePopup(0,EventData.mx,EventData.my) 
            EndIf
         Case Mouse_Up
            If .tagname="VSLIDER" Or .tagname="HSLIDER" Then
               GUI->Console ("Slider Value="+Str(.value))
            EndIf
         Case Ctrl_GotFocus
         Case Ctrl_LostFocus
         Case Timer_Event
            .text  = Time
            *progress += 1
            If *progress>100 Then *progress=0
         Case Menu_Select
            GUI->Console ("Menu_Select ->" & .seltext)
            If .seltext="Quit" Then
               selection = 1
            EndIf
         Case Combo_Select
            GUI->Console ("Combo Selection=" & .text & "," & .listindex)
         Case Slider_Scroll
            GUI->Console ("Slider Value="+Str(.value))
            selection = 4
         Case Ctrl_Refresh
            If .tagname = "DIRLIST" Then
               selection = 2
            EndIf
         Case Key_Press
            GUI->Console ("Key_Press "+Str(EventData.keycode))
      End Select
   End With
   
   Return 0
   
End Function
