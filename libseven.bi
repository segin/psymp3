/'
 ' libseven.bi - Header for libseven
 '/

#ifndef   __LIBSEVEN_BI__
#define  __LIBSEVEN_BI__
#Ifdef __FB_WIN32__
#Inclib "libseven"

Extern "C"
Declare Function InitializeTaskbar Alias "InitializeTaskbar" () As Integer
Declare Sub UpdateProgressBar Alias "UpdateProgressBar" (ByVal As Integer, ByVal As Integer)
Declare Sub AssociateHwnd Alias "AssociateHwnd" (ByVal As HWND)
Declare Sub SetProgressType Alias "SetProgressType" (ByVal As Integer)
#Ifdef UNICODE
Declare Function SetThumbButtonTooltip Alias "SetThumbButtonTooltipW" (ByVal As Integer, ByVal As WString Ptr) As Integer
#Else
Declare Function SetThumbButtonTooltip Alias "SetThumbButtonTooltipA" (ByVal As Integer, ByVal As ZString Ptr) As Integer
#endif
End Extern

Enum 
	TASKBAR_PROGRESS = 1
	TASKBAR_PAUSED = 2
	TASKBAR_NORMAL = 3
	TASKBAR_INDETERMINATE = 4	
End Enum

#endif
#endif 