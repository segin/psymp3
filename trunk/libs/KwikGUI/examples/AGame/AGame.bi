#Include "fbgfx.bi"
#Include "KwikGUI.bi"

#Define MAX_STARS  255
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

Type _thegame
   As _starfield  SF
   As Integer     GameState
   As Integer     Score
   As Integer     GameBoard(2,2)
End Type

Declare Function MsgHandler(EventMsg As _EventMsg, EventData As _EventData) As Integer
Declare Sub      ProcessTheGame(dst As Any Ptr,TheGame As _thegame Ptr)

#Include "TheGame.bas"