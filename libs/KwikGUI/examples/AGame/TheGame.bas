/'
   The Game
   
   This is an include file for AGame.bas
'/
Sub RenderStars (dst As Any Ptr, SF As _starfield)
   For i As Integer = 0 To MAX_STARS
      With sf.Star(i)
         .y += 1+i/100
         If .y>ScrH Then .y=0
         PSet dst,(.x,.y),RGB(i,i,0) Or &h101010
      End With
   Next i   
End Sub

Sub ProcessTheGame(dst As Any Ptr, TheGame As _thegame Ptr)

   Line Dst,(0,0)-(ScrW,ScrH),0,BF
   RenderStars (Dst, TheGame->SF)
   
   Draw String Dst,(100,90),"This is a skeleton for a"
   Draw String Dst,(100,115),"game using KwikGUI as the"
   Draw String Dst,(100,140),"GUI and rendering engine."
   
End Sub
