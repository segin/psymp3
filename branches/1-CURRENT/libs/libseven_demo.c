/* libseven demo application */

#include <windows.h>
#include <libseven.h>

HWND WINAPI GetConsoleWindow(void);

/* This code requires a bare minimum of Windows 2000 to run */

HWND GetConsoleHwnd(void)
   {
       #define MY_BUFSIZE 1024 // Buffer size for console window titles.
       HWND hwndFound;         // This is what is returned to the caller.
       char pszNewWindowTitle[MY_BUFSIZE]; // Contains fabricated
                                           // WindowTitle.
       char pszOldWindowTitle[MY_BUFSIZE]; // Contains original
                                           // WindowTitle.

       // Fetch current window title.

       GetConsoleTitle(pszOldWindowTitle, MY_BUFSIZE);

       // Format a "unique" NewWindowTitle.

       wsprintf(pszNewWindowTitle,"%d/%d",
                   GetTickCount(),
                   GetCurrentProcessId());

       // Change current window title.

       SetConsoleTitle(pszNewWindowTitle);

       // Ensure window title has been updated.

       Sleep(40);

       // Look for NewWindowTitle.

       hwndFound=FindWindow(NULL, pszNewWindowTitle);

       // Restore original window title.

       SetConsoleTitle(pszOldWindowTitle);

       return(hwndFound);
   }


int main()
{
	int j;
	char buf[61];
	HWND hConsole;
	
	buf[60] = 0;
	hConsole = GetConsoleHwnd();
	InitializeTaskbar();
	AssociateHwnd(hConsole);
	SetProgressType(TASKBAR_PROGRESS);
	for(j=0;j<60;j++) { 
		memset(buf, '*', j);
		memset(buf + j, ' ', 60 - j);
		printf("Progress: [%02d/%02d] |%s|\r", j, 60, buf);
		UpdateProgressBar(j, 60);
		Sleep(100);
	}
	SetProgressType(TASKBAR_NORMAL);
	printf("\n");
}
