/*
 * system.cpp - Operating system interfaces and misc. functions.
 * This file is part of PsyMP3.
 * Copyright © 2011-2024 Kirn Gill <segin2005@gmail.com>
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

#include "psymp3.h"

System::System()
{
#ifdef _WIN32
    InitalizeTaskbar();
#endif
}

void System::InitalizeTaskbar()
{
#if defined(_WIN32) && defined(WIN_OPTIONAL)
    HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (void **) &m_taskbar);

    if (SUCCEEDED(hr)) {
        std::cout << "ITaskbarList3 COM object: " << std::hex << m_taskbar << std::endl;
        hr = m_taskbar->HrInit();

        if (FAILED(hr)) {
            std::cerr << "Error initializing ITaskbarList3 COM object!" << std::endl;
            m_taskbar->Release();
            m_taskbar = nullptr;
        }
    } else std::cerr << "Error initializing ITaskbarList3 COM object!" << std::endl;
#endif
}

TagLib::String System::getUser()
{
#ifdef _WIN32
    WCHAR user[48];
    DWORD bufsize = 48;
    GetUserName(user, &bufsize);
    return user;
#else
    return getenv("USER");
#endif
}

/* Get the current user's home directory. This is determined via
 * environment variables. On Windows, one could alternatively use
 * GetUserProfileDirectory(), along with OpenProcessToken() and
 * GetCurrentProcess(), but that seems a roundabout way of acomplishing
 * something so trivial when there's an environment variable that
 * does the same thing.
 * Note: Seems I could also do SHGetFolderPath(), except then I need
 * to check whether we're on Vista and later, or XP and earlier, and
 * SHGetKnownFolderPath() is the one to use on Vista - and it doesn't
 * exist on earlier Windows releases. Which makes using it non-trivial
 * as one must then LoadLibrary(), etc., to get the procedure entry point
 * to avoid having a static reference in the symbol table, as that would
 * break binary compatibility with anything earlier than Vista.
 */

TagLib::String System::getHome()
{
#ifdef _WIN32
    // GetEnvironmentVariable() method.
    TagLib::String spath;
    WCHAR env[1024];
    GetEnvironmentVariable(L"USERPROFILE", env, 1024);
    spath = env;
    return spath;
    // GetUserProfileDirectory() method.

#ifdef WIN_OPTIONAL
    // SHGet(Known)FolderPath() method.
    HMODULE hndl_shell32;
    lpSHGetKnownFolderPath pSHGetKnownFolderPath;
    hndl_shell32 = LoadLibrary("shell32");
    pSHGetKnownFolderPath = GetProcAddress(hndl_shell32, "SHGetKnownFolderPathW");
    if(pSHGetKnownFolderPath != nullptr) {

    } else {

    }

    // NetUserGetInfo() method.
    WCHAR user[48], path[256];
    DWORD bufsize = 48;
    BYTE *uinfo;
    GetUserName(user, &bufsize);
    if(NetUserGetInfo(nullptr, user, 11, &uinfo)) {
        std::cout << "NetUserGetInfo() failed!" << std::endl;
    }
    return ((LPUSER_INFO_11)uinfo)->usri11_home_dir;
#endif
#else
    return getenv("HOME");
#endif
}

/* This function returns the storage path. This is determined based on
 * the %AppData% environment variable on Windows, and the $HOME
 * environment variable on Unix.
 */

TagLib::String System::getStoragePath()
{
    #ifdef _WIN32
        TagLib::String spath;
        WCHAR env[1024];
        GetEnvironmentVariable(L"APPDATA", env, 1024);
        spath = env;
        spath += L"\\PsyMP3";
        return spath;
    #else
        return getHome() + (std::string) "/.psymp3";
    #endif
}

/* Creates the storage path specified by getStoragePath() if it does
 * not already exist.
 */

bool System::createStoragePath()
{
    return false;
}

#ifdef _WIN32
HWND System::getHwnd()
{
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);

    if(!SDL_GetWMInfo(&wmi)) return 0;

    return wmi.window;
}

void System::updateProgress(ULONGLONG now, ULONGLONG max)
{
    if(m_taskbar)
        m_taskbar->SetProgressValue(getHwnd(), now, max);
    else
        std::cerr << "System::updateProgress(): No ITaskbarList3 OLE interface!" << std::endl;
}
void System::progressState(TBPFLAG status)
{
    std::cout << "System::updateProgress(): Called." << std::endl;
    if(m_taskbar)
        m_taskbar->SetProgressState(getHwnd(), status);
    else
        std::cerr << "System::updateProgress(): No ITaskbarList3 OLE interface!" << std::endl;
}

#endif
