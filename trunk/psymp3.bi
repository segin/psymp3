/' psymp3.bi: #define switches for PsyMP3 '/

#Ifndef __PSYMP3_BI__
#Define __PSYMP3_BI__

' #Define USE_ASM 1 

#If Defined(__FB_LINUX__) And Not Defined(I_UNDERSTAND_PSYMP3_IS_BROKEN_ON_LINUX) 
#error PSYMP3 IS KNOWN TO BE BROKEN ON LINUX. I REFUSE TO HELP YOU IF IT BREAKS.
#error YOU HAVE BEEN WARNED. DANGER LIES AHEAD. TURN BACK NOW AND USE PSYMP3 ON WINDOWS.
#Error Define the symbol I_UNDERSTAND_PSYMP3_IS_BROKEN_ON_LINUX to disable this checkpoint.
#endif

#EndIf