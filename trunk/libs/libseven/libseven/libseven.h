#ifndef __LIBSEVEN_H__
#define __LIBSEVEN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdafx.h"

#ifdef LIBSEVEN_EXPORTS
#define LIBSEVEN_API __declspec(dllexport)
#else
#define LIBSEVEN_API __declspec(dllimport)
#endif

enum {
	TASKBAR_PROGRESS = 1,
	TASKBAR_PAUSED = 2, 
	TASKBAR_NORMAL = 3,
	TASKBAR_INDETERMINATE = 4
};

struct tButtons { 
	int magic;
	int count;
	THUMBBUTTON *buttons;
};

struct tState {
	int magic;
	HWND hWnd;
	struct tButtons *buttons;
	ITaskbarList3 *taskbar;
};

LIBSEVEN_API void InitializeTaskbar(void);
LIBSEVEN_API void UpdateProgressBar(int current, int maximum);
LIBSEVEN_API void AssociateHwnd(HWND win);
LIBSEVEN_API void SetProgressType(int type);
LIBSEVEN_API void InitThumbButtons(int buttons);
LIBSEVEN_API int SetThumbButtonDefaultMask(int index);
LIBSEVEN_API int SetThumbButtonTooltipA(int index, char *tooltip);
LIBSEVEN_API int SetThumbButtonTooltipW(int index, wchar_t* tooltip);
#ifdef UNICODE
#define SetThumbButtonTooltip SetThumbButtonTooltipW
#else
#define SetThumbButtonTooltip SetThumbButtonTooltipA
#endif
LIBSEVEN_API int SetThumbButtonMask(int index, int mask);
LIBSEVEN_API void FreeThumbButtons(void);

#ifdef __cplusplus
} /* extern "C" */
#endif 

#endif /* __LIBSEVEN_H__ */
