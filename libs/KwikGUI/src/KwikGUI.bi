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

#Define _LibVersion_ "0.6.9"

#Define MAX_CTRL_INDEX    1000

Enum _EventMsg
   Mouse_Over
   Mouse_Click
   Mouse_DblClick
   Mouse_Down
   Mouse_Up
   Ctrl_GotFocus
   Ctrl_LostFocus
   Ctrl_Captured  'Design Mode Only
   Timer_Event
   Menu_Select
   Combo_Select
   Slider_Scroll
   Ctrl_Refresh
   Key_Press   
   Form_Click
   Custom_Event  = &hFFFF
End Enum

'--------------------------------------------------------------------------------

Type _EventData
   GUI     As Any Ptr
   Ctrl    As Any Ptr
   Button  As Integer
   Mx      As Integer
   My      As Integer
   KeyCode As Integer
   ExtCode As Integer
   param   As Any Ptr
End Type

'---------------------------------------
'   Extended Control Property Types
'---------------------------------------
Type _ItemList
   Text     As String Ptr
   ItemData As Integer
End Type

Enum _CTRL_TYPE
   ctrlLABEL
   ctrlTEXTBOX
   ctrlCHECKBOX
   ctrlTITLEBAR
   ctrlBUTTON
   ctrlPICTURE
   ctrlTIMER
   ctrlMENU
   ctrlNONE      
   ctrlCOMBO 
   ctrlFILELIST   
   ctrlDIRLIST    
   ctrlTEXTBLOCK 
   ctrlHOTSPOT
   ctrlHSLIDER
   ctrlVSLIDER
   ctrlMULTITEXT
   ctrlPOPUP
   ctrlPROGRESS
   ctrlLISTBOX
   ctrlCOLORPICK
   ctrlRADIO
   ctrlSHAPE
End Enum

Type _worker_vars
'------------------------------------------------
'Internal working vars
   curx        As Integer
   cury        As Integer   
   mouseX      As Integer
   mouseY      As Integer
   setflag     As Integer = 0
   dblclick    As Integer
   mouseover   As Integer
   timecount   As Double
   clicktime   As Double = Timer
   scrolltime  As Double = Timer
'------------------------------------------------   
End Type

Type _gsLUT
   a(255) As Byte
   r(255) As Byte
   g(255) As Byte
   b(255) As Byte   
End Type

/' 
      Primary Control Structure
'/
Type _kGUI_Control
   Declare Constructor  (ByVal ctrlTYPE As _CTRL_TYPE, ByVal pMH As Any Ptr)
   Declare Destructor   ()
   ControlType As _CTRL_TYPE
   GUI         As Any Ptr
'---------------------------------
   alpha       As Integer  = 255
   x           As Integer
   y           As Integer
   dx          As Integer
   dy          As Integer
   border      As Integer  = 1
   backColor   As UInteger = &hC0C0C0
   captured    As Integer  = 0
   enabled     As Integer  = 1
   foreColor   As UInteger
   index       As Integer
   interval    As Double
   justify     As Integer  = 0
   listindex   As Integer  = -1
   locked      As Integer  = 0
   min         As Integer  = 0
   max         As Integer  = 100
   path        As String
   pattern     As String   = "*.*"
   picture     As Any Ptr
   maskimage   As Any Ptr
   scrollbar   As Integer
   selStart    As Integer
   selEnd      As Integer 
   selText     As String 
   showfocus   As Integer
   size        As Integer
   state       As Integer  = 0
   sticky      As Integer  = 0
   style       As Integer  = 0
   tag         As Integer
   tagName     As String
   text        As String
   trans       As Integer  = 0
   value       As Integer  = 0
   visible     As Integer  = 1
   wrap        As Integer  = 0
   '
   _internal   As _worker_vars   'Internal working vars
'------------------------------------------------
   MessageHandler As Function    (EventMsg As _EventMsg, EventData As _EventData) As Integer
   Declare Function AddItem      (ByVal Text As String, ItemData As Integer=0) As Integer
   Declare Function GetItem      (Index As Integer)   As String
   Declare Function GetItemData  (Index As Integer)   As Integer  
   Declare Function GetItemCount ()                   As Integer
   Declare Function RemoveItem   (Index As Integer)   As Integer
   Declare Function SetMaskImage (Mask As Any Ptr)    As Integer   
   Declare Function RaiseEvent   (EventMsg As _EventMsg,As Any Ptr=0) As Integer
   Declare Sub Refresh           ()  
   Declare Sub Clear             ()
Private:
   Declare Sub LoadDirList       ()
   Declare Sub LoadFileList      ()
   datalist    As _ItemList Ptr = 0
   datacount   As Integer
End Type

/' 
      Primary GUI Structure
'/
Type _kGUI
   Declare Constructor  (x As Integer, y As Integer, dx As Integer, dy As Integer)
   Declare Destructor   ()
   Declare Function  AddControl      (As _kGUI_Control Ptr) As Integer
   Declare Function  Duplicate       (As _kGUI_Control Ptr) As _kGUI_Control Ptr 
   Declare Sub       CopyProperties  (src As _kGUI_Control Ptr,dest As _kGUI_Control Ptr)
   Declare Sub       Render          (target As Any Ptr=0)
   Declare Sub       ProcessEvents   ()
   Declare Sub       ClearControls   ()
   Declare Sub       RemoveControl   (As _kGUI_Control Ptr)
   Declare Sub       SetControlFocus (As _kGUI_Control Ptr)
   Declare Sub       ActivatePopup   (index As Integer, x As Integer,y As Integer)
   Declare Function  LoadTextFile    (ByVal As String) As String
   Declare Function  LoadBitmap      (ByVal As String) As Any Ptr
   Declare Function  GetGray         (ByVal src As UInteger) As UInteger
   Declare Sub       Console         (ByVal As String)
   Declare Function  FindControl     (ByVal As String) As _kGUI_Control Ptr
   Declare Sub       GetLocalMouse   (As _kGUI Ptr,As _kGUI_Control Ptr, _   
                                      ByRef As Integer=0,ByRef As Integer=0, _    
                                      ByRef As Integer=0,ByRef As Integer=0)
   Declare Sub       CenterMe        ()
   Declare Sub       Resize          (As Integer, As Integer)
   '
   MessageHandler As Function        (EventMsg As _EventMsg, EventData As _EventData) As Integer
   '
   Alpha             As Integer  = 255
   BackColor         As UInteger = &hC0C0C0
   Border            As Integer  = 1
   CloseBtn          As _kGUI_Control Ptr
   ForeColor         As UInteger = &hFFFFFF
   GotFocus          As _kGUI_Control Ptr
   KeyPeek           As Integer = 0
   Moveable          As Integer = 0          
   Picture           As Any Ptr 
   Surface           As Any Ptr
   Trans             As Integer = 0
   Version           As Const String = _LibVersion_
   x                 As Integer
   y                 As Integer   
   dx                As Integer
   dy                As Integer
   designmode        As Integer
   clipboard         As String
   '
   Menus(10,20)      As String
   '     
   '
   Control(MAX_CTRL_INDEX) As _kGUI_Control Ptr   
Private:
   Declare Sub       RenderControl     (As _kGUI_Control Ptr)
   Declare Sub       RenderLabel       (As _kGUI_Control Ptr)
   Declare Sub       RenderTextBox     (As _kGUI_Control Ptr)
   Declare Sub       RenderCheckBox    (As _kGUI_Control Ptr)
   Declare Sub       RenderTitleBar    (As _kGUI_Control Ptr)
   Declare Sub       RenderButton      (As _kGUI_Control Ptr)
   Declare Sub       RenderPicture     (As _kGUI_Control Ptr)
   Declare Sub       RenderMenu        (As _kGUI_Control Ptr)   
   Declare Sub       RenderTextBlock   (As _kGUI_Control Ptr)
   Declare Sub       RenderHotSpot     (As _kGUI_Control Ptr)
   Declare Sub       RenderVSlider     (As _kGUI_Control Ptr)
   Declare Sub       RenderHSlider     (As _kGUI_Control Ptr)
   Declare Sub       RenderCombo       (As _kGUI_Control Ptr)
   Declare Sub       RenderMultiText   (As _kGUI_Control Ptr)
   Declare Sub       RenderPopupMenu   (As _kGUI_Control Ptr)
   Declare Sub       RenderProgress    (As _kGUI_Control Ptr)
   Declare Sub       RenderDirList     (As _kGUI_Control Ptr)   
   Declare Sub       RenderFileList    (As _kGUI_Control Ptr)
   Declare Sub       RenderListBox     (As _kGUI_Control Ptr)
   Declare Sub       RenderColorPick   (As _kGUI_Control Ptr)
   Declare Sub       RenderRadio       (As _kGUI_Control Ptr)
   Declare Sub       RenderShape       (As _kGUI_Control Ptr)
   Declare Sub       InitGrayScale_LUT ()
   Declare Function  ProcessScrollbar  (As _kGUI_Control Ptr, img As Any Ptr,posval As Integer,maxval As Integer) As Integer
   Declare Function  KeyboardProc      (As _kGUI_Control Ptr, ByVal As String) As Integer
   Declare Function  ProcessKey        (As _kGUI_Control Ptr, As String, As _EventData) As Integer
   Declare Function  CheckKey          () As String
   Declare Sub       CheckRadios       (As _kGUI_Control Ptr)
   ' 
   CursorBlink       As Double
   xCapture          As Integer
   yCapture          As Integer
   nullctrl          As _kGUI_Control Ptr
   '
   gsLUT             As _gsLUT   'Grayscale lookup table
   imgClrPick        As Any Ptr
End Type

Type _kGUI_Design
   Declare Constructor  (ByVal filename As String, ByVal pMH As Any Ptr)
   Declare Destructor   ()  
   GUI                  As _kGUI Ptr
   Ctrl(MAX_CTRL_INDEX) As _kGUI_Control Ptr
Private:
   dx  As Integer
   dy  As Integer
   pMH As Any Ptr
   Declare Function ReadGUI          (As Integer) As Integer
   Declare Function ReadControls     (As Integer) As Integer
   Declare Function CreateNewDesigner() As Integer
   Declare Function AddControl       (cType As _CTRL_TYPE) As _kGUI_Control Ptr
   Declare Sub      DestroyDesign    ()
End Type

#Ifndef _MakeLib_
   #Inclib "KwikGUI"
#EndIf

/'
      Support Function Declarations
'/
Declare Function _Dir(ByRef filespec As String = "",ByVal need  As Integer = 0, _
                      ByVal accept As Integer = -1, ByRef attrout As Integer = 0) As String

Declare Sub SetControlFocus      (As _kGUI_Control Ptr)
Declare Sub ParseMenuItems       (Menus() As String, temp As String)
Declare Sub GetLocalMouse        (GUI As _kGUI Ptr, ctrl As _kGUI_Control Ptr, _
                                  ByRef x As Integer=0,ByRef y As Integer=0, _
                                  ByRef wheel As Integer=0,ByRef buttons As Integer=0)
'
Declare Function KeyboardProc    (As _kGUI_Control Ptr) As Integer
Declare Function AddClr          (As UInteger, As UByte) As UInteger
Declare Function SubClr          (As UInteger, As UByte) As UInteger
Declare Function CustomButton    (ByVal src As UInteger, ByVal dest As UInteger, ByVal param As Any Ptr ) As UInteger

Type _RGB
  R As Integer
  G As Integer
  B As Integer
End Type

#Ifndef pi
   Const pi =  Atn(1)*4
#EndIf 
