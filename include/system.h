/*
 * system.h - Header for system-level functionality.
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef SYSTEM_H
#define SYSTEM_H

#if defined(_WIN32) && defined(MISSING_WIN7API)
#define CMIC_MASK_ASYNCOK SEE_MASK_ASYNCOK

// Structs types and enums definitions for Windows 7 taskbar

#define DEFINE_GUID_(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) const GUID n GUID_SECT = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

typedef enum STPFLAG
{
    STPF_NONE = 0,
    STPF_USEAPPTHUMBNAILALWAYS = 0x1,
    STPF_USEAPPTHUMBNAILWHENACTIVE = 0x2,
    STPF_USEAPPPEEKALWAYS = 0x4,
    STPF_USEAPPPEEKWHENACTIVE = 0x8
} STPFLAG;

typedef enum THUMBBUTTONMASK
{
    THB_BITMAP = 0x1,
    THB_ICON = 0x2,
    THB_TOOLTIP	= 0x4,
    THB_FLAGS = 0x8
} THUMBBUTTONMASK;

typedef enum THUMBBUTTONFLAGS
{
    THBF_ENABLED = 0,
    THBF_DISABLED = 0x1,
    THBF_DISMISSONCLICK	= 0x2,
    THBF_NOBACKGROUND = 0x4,
    THBF_HIDDEN	= 0x8,
    THBF_NONINTERACTIVE	= 0x10
} THUMBBUTTONFLAGS;

typedef struct THUMBBUTTON
{
    THUMBBUTTONMASK dwMask;
    UINT iId;
    UINT iBitmap;
    HICON hIcon;
    WCHAR szTip[260];
    THUMBBUTTONFLAGS dwFlags;
} THUMBBUTTON;
typedef struct THUMBBUTTON *LPTHUMBBUTTON;

typedef enum TBPFLAG
{
    TBPF_NOPROGRESS = 0,
    TBPF_INDETERMINATE = 0x1,
    TBPF_NORMAL = 0x2,
    TBPF_ERROR = 0x4,
    TBPF_PAUSED = 0x8
} TBPFLAG;

//MIDL_INTERFACE("56FDF342-FD6D-11d0-958A-006097C9A090")
DECLARE_INTERFACE_(ITaskbarList, IUnknown)
{
    STDMETHOD (HrInit) (THIS) PURE;
    STDMETHOD (AddTab) (THIS_ HWND hwnd) PURE;
    STDMETHOD (DeleteTab) (THIS_ HWND hwnd) PURE;
    STDMETHOD (ActivateTab) (THIS_ HWND hwnd) PURE;
    STDMETHOD (SetActiveAlt) (THIS_ HWND hwnd) PURE;
};
typedef ITaskbarList *LPITaskbarList;

//MIDL_INTERFACE("602D4995-B13A-429b-A66E-1935E44F4317")
DECLARE_INTERFACE_(ITaskbarList2, ITaskbarList)
{
    STDMETHOD (MarkFullscreenWindow) (THIS_ HWND hwnd, int fFullscreen) PURE;
};
typedef ITaskbarList2 *LPITaskbarList2;

//MIDL_INTERFACE("ea1afb91-9e28-4b86-90e9-9e9f8a5eefaf")
DECLARE_INTERFACE_(ITaskbarList3, ITaskbarList2)
{
    STDMETHOD (SetProgressValue) (THIS_ HWND hwnd, ULONGLONG ullCompleted, ULONGLONG ullTotal) PURE;
    STDMETHOD (SetProgressState) (THIS_ HWND hwnd, TBPFLAG tbpFlags) PURE;
    STDMETHOD (RegisterTab) (THIS_ HWND hwndTab,HWND hwndMDI) PURE;
    STDMETHOD (UnregisterTab) (THIS_ HWND hwndTab) PURE;
    STDMETHOD (SetTabOrder) (THIS_ HWND hwndTab, HWND hwndInsertBefore) PURE;
    STDMETHOD (SetTabActive) (THIS_ HWND hwndTab, HWND hwndMDI, DWORD dwReserved) PURE;
    STDMETHOD (ThumbBarAddButtons) (THIS_ HWND hwnd, UINT cButtons, LPTHUMBBUTTON pButton) PURE;
    STDMETHOD (ThumbBarUpdateButtons) (THIS_ HWND hwnd, UINT cButtons, LPTHUMBBUTTON pButton) PURE;
    STDMETHOD (ThumbBarSetImageList) (THIS_ HWND hwnd, HIMAGELIST himl) PURE;
    STDMETHOD (SetOverlayIcon) (THIS_ HWND hwnd, HICON hIcon, LPCWSTR pszDescription) PURE;
    STDMETHOD (SetThumbnailTooltip) (THIS_ HWND hwnd, LPCWSTR pszTip) PURE;
    STDMETHOD (SetThumbnailClip) (THIS_ HWND hwnd, RECT *prcClip) PURE;
};
typedef ITaskbarList3 *LPITaskbarList3;

//MIDL_INTERFACE("c43dc798-95d1-4bea-9030-bb99e2983a1a")
DECLARE_INTERFACE_(ITaskbarList4, ITaskbarList3)
{
    STDMETHOD (SetTabProperties) (HWND hwndTab, STPFLAG stpFlags) PURE;
};
typedef ITaskbarList4 *LPITaskbarList4;

DEFINE_GUID_(CLSID_TaskbarList,0x56fdf344,0xfd6d,0x11d0,0x95,0x8a,0x0,0x60,0x97,0xc9,0xa0,0x90);

DEFINE_GUID_(IID_ITaskbarList,0x56FDF342,0xFD6D,0x11d0,0x95,0x8A,0x00,0x60,0x97,0xC9,0xA0,0x90);
DEFINE_GUID_(IID_ITaskbarList2,0x602D4995,0xB13A,0x429b,0xA6,0x6E,0x19,0x35,0xE4,0x4F,0x43,0x17);
DEFINE_GUID_(IID_ITaskbarList3,0xea1afb91,0x9e28,0x4b86,0x90,0xE9,0x9e,0x9f,0x8a,0x5e,0xef,0xaf);
DEFINE_GUID_(IID_ITaskbarList4,0xc43dc798,0x95d1,0x4bea,0x90,0x30,0xbb,0x99,0xe2,0x98,0x3a,0x1a);

#endif /* _WIN32 && MISSING_WIN7API*/

#ifdef _WIN32
// Winamp IPC message constants
#define IPC_GETVERSION 0
#define IPC_PLAYFILE 100
#define IPC_ISPLAYING 104
#define IPC_GETOUTPUTTIME 105
#define IPC_JUMPTOTIME 106
#define IPC_WRITEPLAYLIST 120
#define IPC_SETPLAYLISTPOS 121
#define IPC_SETVOLUME 122
#define IPC_GETLISTLENGTH 124
#define IPC_GETLISTPOS 125
#define IPC_GETINFO 126
#define IPC_GETPLAYLISTFILE 211
#define IPC_GETPLAYLISTTITLE 212
#define IPC_GETPLAYLISTFILEW 214
#define IPC_GET_REPEAT 251
#define IPC_SET_REPEAT 253
#define IPC_GETWND 260
#define IPC_IS_PLAYING_VIDEO 501

// Winamp remote control commands (sent via WM_COMMAND)
#define WINAMP_BUTTON1 40044 // Prev
#define WINAMP_BUTTON2 40045 // Play
#define WINAMP_BUTTON3 40046 // Pause
#define WINAMP_BUTTON4 40047 // Stop
#define WINAMP_BUTTON5 40048 // Next

// Windows 7+ taskbar thumbnail-toolbar (window-preview) transport buttons.
// A click arrives as WM_COMMAND with HIWORD(wParam)==THBN_CLICKED and
// LOWORD(wParam)==the button's iId below. IDs sit above the Winamp block so
// they never collide with it or the native menu's IDM_* range.
#ifndef THBN_CLICKED
#define THBN_CLICKED 0x1800
#endif
#define PSYMP3_THUMB_PREV      41000
#define PSYMP3_THUMB_PLAYPAUSE 41001
#define PSYMP3_THUMB_NEXT      41002

// KVIrc custom interface constants
#define KVIRC_WM_USER 63112
#define KVIRC_WM_USER_CHECK 13123
#define KVIRC_WM_USER_CHECK_REPLY 13124
#define KVIRC_WM_USER_GETTITLE 5000
#define KVIRC_WM_USER_GETFILE 10000
#define KVIRC_WM_USER_TRANSFER 15000

// For IPC_GETINFO
#pragma pack(push, 1)
typedef struct {
    char* filename;
    char* metadata;
    char* ret;
    int retlen;
} extendedFileInfoStruct;

typedef struct {
    wchar_t* filename;
    wchar_t* metadata;
    wchar_t* ret;
    int retlen;
} extendedFileInfoStructW;
#pragma pack(pop)
#endif
#if defined(_WIN32) && defined(WIN_OPTIONAL)

// Pointerizing this Vista-and-later call for XP/2000 compat, etc.
typedef HRESULT (WINAPI* lpSHGetKnownFolderPath)(
    REFKNOWNFOLDERID rfid,
    DWORD dwFlags,
    HANDLE hToken,
    PWSTR *ppszPath
);

#endif


class System
{
    public:
        /** Default constructor */
        System();
        ~System();

        // Prevent copying and moving to avoid issues with the raw COM pointer m_taskbar.
        System(const System&) = delete;
        System& operator=(const System&) = delete;
        System(System&&) = delete;
        System& operator=(System&&) = delete;

#ifdef _WIN32
        void InitializeIPC(class Player* player);
#endif
        void InitializeTaskbar();
        #if defined(_WIN32)
        static void setMainWindow(SDL_Window* window);
        void announceNowPlaying(const TagLib::String& artist, const TagLib::String& title, const TagLib::String& album);
        void clearNowPlaying();
        #endif
        static TagLib::String getUser(); // where applicable
        static TagLib::String getHome();
        static TagLib::String getStoragePath();
        static bool createStoragePath(); // directory name is implicit.

        /**
         * @brief Build a std::filesystem::path from a UTF-8 string with correct
         *        Windows Unicode semantics.
         *
         * Narrow fstream/remove paths go through the ANSI codepage on Windows,
         * which breaks non-ASCII %APPDATA%; construct from the wide string
         * there and from the UTF-8 bytes elsewhere (the POSIX native encoding).
         */
        static std::filesystem::path pathFromUtf8(const std::string& utf8);
        enum class ThreadPriority {
            Low,
            Normal,
            High,
            TimeCritical
        };

        static void setThisThreadName(const std::string& name);
        static void setThreadPriority(ThreadPriority priority);
        static bool lockMemory();

        // The three latency-sensitive roles that get a dedicated CPU core when
        // the machine has enough of them (see pinThreadToRole).
        enum class CpuRole {
            Gui,       // main/event-loop thread (FFT + rendering)
            Decoder,   // Audio::decoderThreadLoop
            Playback   // SDL audio-callback thread
        };
        // Pin the CALLING thread to a core dedicated to its role, keeping the
        // decoder and playback threads off the (CPU-hungry) GUI core. Only acts
        // when there are >= 3 logical cores; otherwise it leaves scheduling to
        // the OS. Windows and Linux only; a no-op elsewhere.
        static void pinThreadToRole(CpuRole role);
        #if defined(_WIN32) // This block is for static methods
        static HWND getHwnd();
        void updateProgress(ULONGLONG now, ULONGLONG max);
        void progressState(TBPFLAG status);
        // Windows 7+ taskbar thumbnail-toolbar transport controls (Prev /
        // Play-Pause / Next on the window-preview flyout). setupThumbBar() must
        // be (re)called after the shell sends the TaskbarButtonCreated message;
        // it adds the buttons the first time and refreshes them thereafter.
        void setupThumbBar();
        void updateThumbBarPlayState(bool playing);
        // The registered "TaskbarButtonCreated" window message (cached). The
        // shell posts it to the main window once its taskbar button exists and
        // again if Explorer restarts -- the only valid time to add thumb buttons.
        static UINT taskbarButtonCreatedMessage();
        #endif
    private:
    #if defined(_WIN32)
        static LRESULT CALLBACK ipcWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        static SDL_Window* s_main_window;
        void broadcastMsnMessage(const std::wstring& message);
        // Using a smart pointer for the COM object automates Release() calls,
        // making resource management safer and simpler (RAII).
        // This requires #include <wrl/client.h>
        // Microsoft::WRL::ComPtr<ITaskbarList3> m_taskbar;
        ITaskbarList3 *m_taskbar; // Keeping original for now, but ComPtr is recommended.
        HWND m_ipc_hwnd = nullptr;
        // Thumbnail-toolbar transport buttons: runtime-drawn glyph icons (no
        // external assets, so the exe stays self-contained) and a latch marking
        // whether ThumbBarAddButtons has run (so later calls use Update, not Add).
        HICON m_thumb_icon_prev = nullptr;
        HICON m_thumb_icon_play = nullptr;
        HICON m_thumb_icon_pause = nullptr;
        HICON m_thumb_icon_next = nullptr;
        bool m_thumb_added = false;
        void fillThumbButtons(THUMBBUTTON* out, bool playing);
    #endif

};

#endif // SYSTEM_H
