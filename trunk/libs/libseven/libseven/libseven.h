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

#ifdef __cplusplus
} /* extern "C" */
#endif 

#endif /* __LIBSEVEN_H__ */