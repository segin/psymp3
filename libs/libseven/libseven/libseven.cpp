/* libseven - Windows 7 Taskbar API wrapper
 * 
 * Copyright © 2009 Kirn Gill <segin2005@gmail.com>
 * 
 */

#include "stdafx.h"
#include "libseven.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tState state = {
	420, 
	NULL,
	NULL,
	NULL
};

LIBSEVEN_API void InitializeTaskbar(void)
{
	if (state.taskbar != NULL) {
		return;
	}
	CoCreateInstance(
		CLSID_TaskbarList, NULL, CLSCTX_ALL,
		IID_ITaskbarList3, (void **) &(state.taskbar)
	);
}

LIBSEVEN_API void UpdateProgressBar(int current, int maximum)
{
	if (state.taskbar == NULL) {
		return;
	}
	state.taskbar->SetProgressValue(state.hWnd, current, maximum);
}

LIBSEVEN_API void AssociateHwnd(HWND win)
{
	if (state.taskbar == NULL) {
		return;
	}
	state.hWnd = win;
}

LIBSEVEN_API void SetProgressType(int type)
{
	if (state.taskbar == NULL) {
		return;
	}
	switch (type) {
		case TASKBAR_NORMAL:
			state.taskbar->SetProgressState(state.hWnd, TBPF_NOPROGRESS);
		case TASKBAR_PAUSED:
			state.taskbar->SetProgressState(state.hWnd, TBPF_PAUSED);
		case TASKBAR_PROGRESS:
			state.taskbar->SetProgressState(state.hWnd, TBPF_NORMAL);
		case TASKBAR_INDETERMINATE:
			state.taskbar->SetProgressState(state.hWnd, TBPF_INDETERMINATE);
	}
}

// Thumbnail Button wrappers

LIBSEVEN_API void InitThumbButtons(int buttons)
{
	THUMBBUTTON *thbButtons;

	if (state.hWnd == NULL || buttons == 0 || buttons > 7) { 
		return;
	}
	
	state.buttons = (struct tButtons *) malloc(sizeof(struct tButtons));
	
	if (state.buttons == NULL) {
		return;
	}
	
	state.buttons->magic = 420;
	thbButtons = (THUMBBUTTON *) calloc(buttons, sizeof(THUMBBUTTON));
	
	if (thbButtons == NULL) {
		return;
	}

	state.buttons->buttons = thbButtons;
}

LIBSEVEN_API int SetThumbButtonDefaultMask(int index)
{
	if (state.buttons == NULL) return -1;
	if (state.buttons->count < index) return -2;
	state.buttons->buttons[index - 1].dwMask = THB_ICON|THB_TOOLTIP|THB_FLAGS;
	return 0;
}

LIBSEVEN_API int SetThumbButtonTooltipA(int index, char *tooltip)
{
	if (state.buttons == NULL) return -1;
	if (state.buttons->count < index) return -2;
	MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED, tooltip, 
		strlen(tooltip) > 260 ? 260 : strlen(tooltip),
		state.buttons->buttons[index - 1].szTip, 260);
	return(0);
}

LIBSEVEN_API int SetThumbButtonTooltipW(int index, wchar_t* tooltip)
{
	if (state.buttons == NULL) return(-1);
	if (state.buttons->count < index) return(-2);
	wcsncpy(state.buttons->buttons[index - 1].szTip, tooltip, 260);
	return(0);
}

LIBSEVEN_API int SetThumbButtonMask(int index, int mask)
{
	if (state.buttons == NULL) return -1;
	if (state.buttons->count < index) return -2;
	state.buttons->buttons[index - 1].dwMask = (THUMBBUTTONMASK) mask;
	return 0;
}

LIBSEVEN_API void FreeThumbButtons(void)
{
	if (state.buttons == NULL) {
		return;
	}
	free(state.buttons->buttons);
	free(state.buttons);
}

#ifdef __cplusplus
} /* extern "C" */
#endif