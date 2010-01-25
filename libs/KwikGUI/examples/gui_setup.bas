/' 
   #Include file for Gui_Test.bas

   Setup and Initialize GUI Elements
'/

                        'x ,y  ,dx ,dy
   Var GUI       = _kGUI(0,0,800,600)

   WindowTitle "KwikGUI ver " + GUI.version   

   GUI.ForeColor = &hAA
   GUI.BackColor = &hc0c0c0
                                                '  Ctrl Type,      Msg Handler 
   'Dim cTitle    As _kGUI_Control = _kGUI_Control(ctrlTITLEBAR, @MessageHandler)
   Dim cHotSpot  As _kGUI_Control = _kGUI_Control(ctrlHotSpot,  @MessageHandler)
   Dim cButton   As _kGUI_Control = _kGUI_Control(ctrlBUTTON,   @MessageHandler)
   Dim cLabel    As _kGUI_Control = _kGUI_Control(ctrlLABEL,    @MessageHandler)
   Dim cCheck    As _kGUI_Control = _kGUI_Control(ctrlCHECKBOX, @MessageHandler)
   Dim cText     As _kGUI_Control = _kGUI_Control(ctrlTEXTBOX,  @MessageHandler)
   Dim cPicture  As _kGUI_Control = _kGUI_Control(ctrlPICTURE,  @MessageHandler)
   Dim cTimer    As _kGUI_Control = _kGUI_Control(ctrlTIMER,    @MessageHandler)
   Dim cMenu     As _kGUI_Control = _kGUI_Control(ctrlMENU,     @MessageHandler)
   Dim cTextBox  As _kGUI_Control = _kGUI_Control(ctrlMULTITEXT,@MessageHandler)
   Dim cVSlider  As _kGUI_Control = _kGUI_Control(ctrlVSLIDER,  @MessageHandler)
   Dim cHSlider  As _kGUI_Control = _kGUI_Control(ctrlHSLIDER,  @MessageHandler)
   Dim cCombo    As _kGUI_Control = _kGUI_Control(ctrlCOMBO,    @MessageHandler)
   Dim cPopup    As _kGUI_Control = _kGUI_Control(ctrlPOPUP,    @MessageHandler)
   Dim cProgress As _kGUI_Control = _kGUI_Control(ctrlPROGRESS, @MessageHandler)
   Dim cDir      As _kGUI_Control = _kGUI_Control(ctrlDIRLIST,  @MessageHandler)
   Dim cFile     As _kGUI_Control = _kGUI_Control(ctrlFILELIST, @MessageHandler)
   Dim cListBox  As _kGUI_Control = _kGUI_Control(ctrlLISTBOX,  @MessageHandler)
   Dim cRadio1   As _kGUI_Control = _kGUI_Control(ctrlRADIO,  @MessageHandler)
   Dim cRadio2   As _kGUI_Control = _kGUI_Control(ctrlRADIO,  @MessageHandler)
   Dim cRadio3   As _kGUI_Control = _kGUI_Control(ctrlRADIO,  @MessageHandler)
   
/'
   Adding this hotspot lets the entire GUI return
   event messages, like mouse_over or key_press 
'/
   With cHotSpot
      .x=0
      .y=0
      .dx=800
      .dy=600
      'GUI.AddControl(@cHotSpot) 
   End With
   
   With cButton
      Var mask = GUI.LoadBitmap ("btnmask.bmp")
      .x=40    'Relative to upper left of GUI not Screen
      .y=50    'Relative to upper left of GUI not Screen
      .dx=48   '<-not needed when using SetMaskImage
      .dy=48   '<-not needed when using SetMaskImage
      .SetMaskImage(mask)      
      .forecolor = &hFFFFFE
      .backcolor = &hff1122
      .justify = 2
      .text = "Btn"
      .tagname="Command Btn"
      .trans = 0
      '.enabled=0
      '.sticky = 1
      GUI.AddControl(@cButton)
   End With

   With cCheck
      .x=20    'Relative to upper left of GUI not Screen
      .y=120    'Relative to upper left of GUI not Screen
      .size=10
      .text = "Button Style"
      .backcolor = &hFFFFFF
      .forecolor = &hFF
      .tagname="CheckBox"
      GUI.AddControl(@cCheck)
   End With

   With cLabel
      .x=30    'Relative to upper left of GUI not Screen
      .y=145    'Relative to upper left of GUI not Screen
      .dx=75
      .dy=20
      .text = "Label"
      .border = 1
      .justify = 2
      .trans = 1
      .backcolor = &hC0C0C0
      .forecolor = &h0
      .tagname="Text Label"
      GUI.AddControl(@cLabel)
   End With

   With cText
      .x=30   'Relative to upper left of GUI not Screen
      .y=190   'Relative to upper left of GUI not Screen
      .dx=100
      .dy=16
      .justify = 2   '0=left/1=right/2=center
      .text = "TextBox"
      .backcolor = &hFFFFFF
      .forecolor = &h0
      .tagname="Text Box"
      GUI.AddControl(@cText)
   End With

   With cPicture
      Var TestPic = GUI.LoadBitmap("test.bmp")
      .x=45    'Relative to upper left of GUI not Screen
      .y=230    'Relative to upper left of GUI not Screen
      .dx=48
      .dy=48
      .border = 0
      .picture = testpic
      .trans = 1
      .tagname="Picture Box"
      GUI.AddControl(@cPicture)
   End With

   With cTimer
      .interval=.01
      .tagname="Timer Control"
      '.enabled = 0
      GUI.AddControl(@cTimer)
   End With

   With cMenu
      .text =  "&File&&Open&&Print&&Save&&---------&&Quit"+ _
               "&Edit&&Undo"+ _
               "&Format&&Bold"
               
      .tagname="Drop Down Menu"
      .backcolor = &h661111
      .forecolor = &hFFFFFF
      .sticky = 0
      GUI.AddControl(@cMenu)
   End With

   With cTextBox
      .x=150    'Relative to upper left of GUI not Screen
      .y=50    'Relative to upper left of GUI not Screen
      .dx=80*8  'Width
      .dy=250   'Height
      .backcolor = &he0e0e0
      .justify = 2
      .text = GUI.LoadTextFile("GUI_Test.bas")
      .tagname="MultiText"
      '.wrap = 1
      .border=0
      .scrollbar=1
      GUI.AddControl(@cTextBox)
   End With

   With cVSlider
      .x=100    'Relative to upper left of GUI not Screen
      .y=220    'Relative to upper left of GUI not Screen
      .dx=15  'Width
      .dy=100   'Height
      .min = 0
      .max = 255
      .value = 255
      .border = 1
      .size = 5   'Size of slider knob
      .forecolor = &hFF00
      .tagname="VSLIDER"
      GUI.AddControl(@cVSlider)
   End With

   With cHSlider
      .x=20    'Relative to upper left of GUI not Screen
      .y=340    'Relative to upper left of GUI not Screen
      .dx=100  'Width
      .dy=15   'Height
      .min = 0
      .max = 100
      .border = 1
      .size = 5   'Size of slider knob
      .forecolor = &hFFFF00
      .tagname="HSLIDER"
      GUI.AddControl(@cHSlider)
   End With

   With cCombo
      .x=20    'Relative to upper left of GUI not Screen
      .y=370    'Relative to upper left of GUI not Screen
      .dx=120
      .dy=12
      .text = "Combo"
      .border = 1
      .justify = 0
      .trans = 0
      .backcolor = &he0e0e0
      .forecolor = &h0
      .tagname="Combo"
      .sticky = 1
      .scrollbar=1
      GUI.AddControl(@cCombo)
   End With
   
   'This control is only visible when
   'you give it focus by setting
   'GUI.SetControlFocus(@cPopUp)
   With cPopUp
      .dx = 120    'Specify width
                   'Height determined by # of entries
      .justify = 0 '0=left/1=right/2=center justify
      .backcolor = &hFFFF00
      .AddItem("This One")
      .AddItem("Another One")
      .AddItem("Third One")
      .AddItem("Last One")
      GUI.AddControl(@cPopUp)      
   End With   
   
   With cProgress
      .x=250
      .y=560
      .dx=300
      .dy=15
      .justify = 2
      .forecolor=&h9900
      .backcolor=&h0
      progress=@cProgress.value
      GUI.AddControl(@cProgress)
   End With
   
   With cDir
      .x=200
      .y=350
      .dx=150
      .dy=150
      .backcolor = &hffff00
      .path=CurDir
      .tagname = "DIRLIST"
      .Refresh()
      GUI.AddControl(@cDir)
   End With

   With cFile
      .x=400
      .y=350
      .dx=150
      .dy=150
      .backcolor = &hffff00
      .path=CurDir
      .pattern ="*.*" 
      .tagname = "FILELIST"
      .Refresh()
      GUI.AddControl(@cFile)
   End With

   With cListBox
      .x=600
      .y=350
      .dx=150
      .dy=150
      .backcolor = &hffffff
      .tagname = "LISTBOX"
      .Refresh()
      GUI.AddControl(@cListBox)
   End With

   With cRadio1
      .x=25
      .y=500
      '.size = 35
      .tagname = "RADIO"
      .text = "Selection #1"
      GUI.CopyProperties (@cRadio1,@cRadio2)
      GUI.CopyProperties (@cRadio1,@cRadio3)
      cRadio2.y = 520
      cRadio3.y = 540
      cRadio2.text = "Selection #2"
      cRadio3.text = "Selection #3"
      GUI.AddControl(@cRadio1)
      GUI.AddControl(@cRadio2)
      GUI.AddControl(@cRadio3)
   End With
   
   'Add Combo Items
   Scope
      Dim fn As String
      fn = Dir("*.*")
      While Len(fn)
         cCombo.AddItem(fn)
         fn=Dir
      Wend
   End Scope

   'Remove an Item
   'cCombo.RemoveItem(0)

   'Get Total # of items   
   GUI.Console ("Total Items="+Str(cCombo.GetItemCount()))         
   
   cMenu.text = "&Vince"
   cMenu.Refresh
   