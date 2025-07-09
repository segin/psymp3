/*
 * system.cpp - Operating system interfaces and misc. functions.
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

#include "psymp3.h"

#ifdef _WIN32
// For setting thread name in the Visual Studio debugger
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
#endif

/**
 * @brief Percent-encodes a wide string.
 *
 * This function is used for the KVIrc IPC interface, which expects file paths
 * to be percent-encoded. It preserves unreserved characters (alphanumeric, '-', '.', '_', '~')
 * and encodes all others according to RFC 3986.
 *
 * @param input The wide string to encode.
 * @return The percent-encoded wide string.
 */
static std::wstring percentEncodeW(const std::wstring& input) __attribute__((unused)); 
static std::wstring percentEncodeW(const std::wstring& input) {
    std::wstringstream encoded;
    for (wchar_t wc : input) {
        if (iswalnum(wc) || wc == L'-' || wc == L'.' || wc == L'_' || wc == L'~') {
            encoded << wc;
        } else {
            encoded << L'%' << std::hex << std::uppercase << std::setw(2) << std::setfill(L'0') << static_cast<unsigned int>(wc);
        }
    }
    return encoded.str();
}

System::System()
{
#ifdef _WIN32
    // Initialize m_taskbar to nullptr only if it's a member (i.e., on Windows)
    m_taskbar = nullptr; // For taskbar features
    m_ipc_hwnd = nullptr; // For Winamp IPC
    InitializeTaskbar();
#endif
}

System::~System()
{
#if defined(_WIN32)
    if (m_ipc_hwnd) {
        DestroyWindow(m_ipc_hwnd);
    }
    if (m_taskbar) {
        m_taskbar->Release();
        m_taskbar = nullptr;
    }
#endif
}

#ifdef _WIN32
void System::InitializeIPC(Player* player)
{
    WNDCLASSEXW wcx = {0};
    wcx.cbSize = sizeof(WNDCLASSEXW);
    wcx.lpfnWndProc = System::ipcWndProc;
    wcx.hInstance = GetModuleHandleW(NULL);
    wcx.lpszClassName = L"Winamp v1.x";

    if (!RegisterClassExW(&wcx)) {
        std::cerr << "Failed to register Winamp IPC window class. Error: " << GetLastError() << std::endl;
        return;
    }

    // Create a hidden window to listen for Winamp IPC messages.
    // We pass the 'player' pointer as the creation parameter.
    // This allows the static ipcWndProc to retrieve it and interact with the player instance.
    m_ipc_hwnd = CreateWindowExW(
        0,                              // Optional window styles.
        L"Winamp v1.x",                 // Window class
        L"PsyMP3 Winamp Interface",     // Window text
        0,                              // Window style (not visible)
        0, 0, 0, 0,                     // Position and size (hidden)
        NULL,                           // Parent window
        NULL,                           // Menu
        GetModuleHandleW(NULL),          // Instance handle
        player                          // Additional application data (the Player pointer)
    );

    if (!m_ipc_hwnd) {
        std::cerr << "Failed to create Winamp IPC window. Error: " << GetLastError() << std::endl;
    }
}
#endif

#ifdef _WIN32
/**
 * @brief Finds all MsnMsgrUIManager windows and sends them a WM_COPYDATA message.
 * @param message The wide-character string payload, with fields separated by nulls.
 */
void System::broadcastMsnMessage(const std::wstring& message)
{
    COPYDATASTRUCT cds;
    cds.dwData = 1351; // Magic number for MSN Now Playing IPC
    // The size is the total number of bytes, including the final null terminator.
    cds.cbData = (message.length() + 1) * sizeof(wchar_t);
    cds.lpData = (PVOID)message.c_str();

    HWND msgr_hwnd = NULL;
    // Loop through all top-level windows to find all messenger instances.
    do {
        msgr_hwnd = FindWindowExW(NULL, msgr_hwnd, L"MsnMsgrUIManager", NULL);
        if (msgr_hwnd) {
            // Using SendMessage for synchronous delivery.
            SendMessage(msgr_hwnd, WM_COPYDATA, (WPARAM)getHwnd(), (LPARAM)&cds);
        }
    } while (msgr_hwnd != NULL);
}

/**
 * @brief Announces the currently playing track to MSN-compatible messengers.
 * @param artist The artist of the track.
 * @param title The title of the track.
 * @param album The album of the track.
 */
void System::announceNowPlaying(const TagLib::String& artist, const TagLib::String& title, const TagLib::String& album)
{
    std::wstringstream wss;
    // Format string with embedded nulls:
    // {PlayerName}\0{MessageType}\0{Enabled}\0{FormatString}\0{Title}\0{Artist}\0{Album}\0{ContentID}\0
    wss << L"PsyMP3" << L"\\0"
        << L"Music" << L"\\0"
        << L"1" << L"\\0" // 1 = Enabled
        << L"PsyMP3: {1} - {0}" << L"\\0" // Format: {1} is artist, {0} is title
        << title.toWString() << L"\\0"
        << artist.toWString() << L"\\0"
        << album.toWString() << L"\\0"
        << L"WMContentID" << L"\\0";

    broadcastMsnMessage(wss.str());
}

/**
 * @brief Clears the "Now Playing" status from MSN-compatible messengers.
 */
void System::clearNowPlaying()
{
    // To clear, we send a message with the "Enabled" field set to 0.
    std::wstring clear_message = L"PsyMP3\\0Music\\00\\0\\0\\0\\0\\0\\0";
    broadcastMsnMessage(clear_message);
}
#endif

#ifdef _WIN32
LRESULT CALLBACK System::ipcWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // On first creation, store the Player pointer passed in CreateWindowEx
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    // Retrieve the Player pointer for subsequent messages
    Player* player = reinterpret_cast<Player*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (!player) {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    // These static buffers are needed because some legacy IPC calls return a direct pointer
    // to internal data. This is an unsafe pattern required for compatibility.
    static std::string currentFileA;
    static std::wstring currentFileW;
    static std::string currentTitleA;
    static std::wstring currentTitleW;
    static std::wstring kvircBufferW;

    switch (uMsg)
    {
        case WM_COMMAND:
            switch (wParam)
            {
                case WINAMP_BUTTON1: // Prev
                    player->synthesizeUserEvent(DO_PREV_TRACK, nullptr, nullptr);
                    break;
                case WINAMP_BUTTON2: // Play
                    player->play();
                    break;
                case WINAMP_BUTTON3: // Pause
                    player->playPause();
                    break;
                case WINAMP_BUTTON4: // Stop
                    player->stop();
                    break;
                case WINAMP_BUTTON5: // Next
                    player->synthesizeUserEvent(DO_NEXT_TRACK, nullptr, nullptr);
                    break;
            }
            return 0;

        case WM_USER:
        {
            // KVIrc-specific IPC interface. This is checked before the standard Winamp IPC.
            if (wParam == KVIRC_WM_USER) {
                switch (lParam)
                {
                    case KVIRC_WM_USER_CHECK:
                        return KVIRC_WM_USER_CHECK_REPLY;

                    case KVIRC_WM_USER_GETTITLE:
                        if (player->stream) {
                            currentTitleW = player->stream->getArtist().toWString() + L" - " + player->stream->getTitle().toWString();
                            kvircBufferW = currentTitleW;
                            return kvircBufferW.length();
                        }
                        return 0;

                    case KVIRC_WM_USER_GETFILE:
                        if (player->stream) {
                            std::wstring path = player->stream->getFilePath().toWString();
                            kvircBufferW = percentEncodeW(path);
                            return kvircBufferW.length();
                        }
                        return 0;

                    default:
                        // Handle character-by-character transfer requests
                        if (lParam >= KVIRC_WM_USER_TRANSFER && lParam < KVIRC_WM_USER_TRANSFER + 4096) {
                            size_t index = lParam - KVIRC_WM_USER_TRANSFER;
                            if (index < kvircBufferW.length()) {
                                return kvircBufferW[index];
                            }
                        }
                        return 0;
                }
            }

            // Standard Winamp IPC messages
            switch (lParam)
            {
                case IPC_GETVERSION:
                    return 0x2091; // Report as Winamp 2.91
                case IPC_ISPLAYING:
                    switch (player->state) {
                        case PlayerState::Playing: return 1;
                        case PlayerState::Paused:  return 3;
                        default:                   return 0; // PlayerState::Stopped
                    }
                case IPC_GETOUTPUTTIME:
                    if (player->stream) {
                        if (wParam == 0) return player->stream->getPosition(); // Position in ms
                        if (wParam == 1) return player->stream->getLength() / 1000; // Length in seconds
                    }
                    return 0;
                case IPC_JUMPTOTIME:
                    if (player->stream) {
                        player->seekTo(wParam);
                    }
                    break;
                case IPC_GETLISTLENGTH:
                    if (player->playlist) {
                        return player->playlist->entries();
                    }
                    return 0;
                case IPC_GETLISTPOS:
                    if (player->playlist) {
                        return player->playlist->getPosition();
                    }
                    return 0;
                case IPC_GETPLAYLISTTITLE:
                    if (player->stream) {
                        currentTitleA = player->stream->getArtist().to8Bit(true) + " - " + player->stream->getTitle().to8Bit(true);
                        return (LRESULT)currentTitleA.c_str();
                    }
                    return 0;
                case IPC_GETPLAYLISTFILE:
                    if (player->stream) {
                        currentFileA = player->stream->getFilePath().to8Bit(true);
                        return (LRESULT)currentFileA.c_str();
                    }
                    return 0;
                case IPC_GETPLAYLISTFILEW:
                    if (player->stream) {
                        currentFileW = player->stream->getFilePath().toWString();
                        return (LRESULT)currentFileW.c_str();
                    }
                    return 0;
            }
            return 0; // WM_USER handled
        }
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}
#endif

void System::InitializeTaskbar()
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
    GetUserNameW(user, &bufsize);
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
    WCHAR env_buffer[1024];
    // 1. Check for HOME, common in development environments like MSYS2.
    if (GetEnvironmentVariableW(L"HOME", env_buffer, 1024) > 0) {
        return TagLib::String(env_buffer);
    }
    // 2. Check for USERPROFILE, the standard on Windows.
    if (GetEnvironmentVariableW(L"USERPROFILE", env_buffer, 1024) > 0) {
        return TagLib::String(env_buffer);
    }
    // 3. Query the registry for the "My Documents" path (most reliable for Win9x+).
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD buffer_size = sizeof(env_buffer);
        if (RegQueryValueExW(hKey, L"Personal", NULL, NULL, (LPBYTE)env_buffer, &buffer_size) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return TagLib::String(env_buffer);
        }
        RegCloseKey(hKey);
    }
    // 4. Final fallback for very old systems or unusual configurations.
    return TagLib::String("C:\\My Documents", TagLib::String::Latin1);
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
        GetEnvironmentVariableW(L"APPDATA", env, 1024);
        spath = env; // Changed this line, it was not returning from the right spot.
        spath += L"\\PsyMP3";
        return spath;
    #else
        // Use XDG config directory for unified storage
        const char* xdg_config = getenv("XDG_CONFIG_HOME");
        if (xdg_config && strlen(xdg_config) > 0) {
            return TagLib::String(xdg_config) + "/psymp3";
        } else {
            return getHome() + "/.config/psymp3";
        }
    #endif
}

/* Creates the storage path specified by getStoragePath() if it does
 * not already exist.
 */

bool System::createStoragePath()
{
    TagLib::String path = getStoragePath();
#ifdef _WIN32
    // CreateDirectoryW returns non-zero on success.
    return CreateDirectoryW(path.toWString().c_str(), NULL) || (GetLastError() == ERROR_ALREADY_EXISTS);
#else
    // mkdir returns 0 on success.
    return mkdir(path.toCString(true), 0755) == 0 || (errno == EEXIST);
#endif
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

void System::setThisThreadName(const std::string& name)
{
#if defined(__linux__)
    // Linux limits thread names to 16 bytes (including null terminator).
    std::string truncated_name = name.substr(0, 15);
    prctl(PR_SET_NAME, truncated_name.c_str(), 0, 0, 0);
#elif defined(__FreeBSD__)
    // FreeBSD also has a limit, typically MAXCOMLEN + 1 (16 bytes).
    std::string truncated_name = name.substr(0, 15);
    pthread_set_name_np(pthread_self(), truncated_name.c_str());
#elif defined(_WIN32)
    // The SetThreadDescription API is available on Windows 10 1607+
    // We dynamically load it to maintain compatibility with older systems.
    using SetThreadDescription_t = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    auto pSetThreadDescription = (SetThreadDescription_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription");

    if (pSetThreadDescription) {
        // Convert std::string (UTF-8) to std::wstring (UTF-16) for the Unicode API
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &name[0], (int)name.size(), NULL, 0);
        std::wstring wname(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &name[0], (int)name.size(), &wname[0], size_needed);
        
        pSetThreadDescription(GetCurrentThread(), wname.c_str());
    } else {
        // Fallback to the old MSVC debugger-specific method.
        // This is only supported by the MSVC compiler and its debugger.
#if defined(_MSC_VER)
        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = name.c_str();
        info.dwThreadID = (DWORD)-1; // -1=caller thread
        info.dwFlags = 0;

        __try { RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info); }
        __except(EXCEPTION_EXECUTE_HANDLER) { }
#endif // _MSC_VER
    }
#else
    // No-op for other platforms.
    (void)name;
#endif
}

// TagLib::String overloads for std::to_string
namespace std {
    TagLib::String to_string(int value) {
        return TagLib::String::number(value);
    }
    
    TagLib::String to_string(long value) {
        return TagLib::String::number(value);
    }
    
    TagLib::String to_string(long long value) {
        return TagLib::String::number(static_cast<long>(value));
    }
    
    TagLib::String to_string(unsigned value) {
        return TagLib::String::number(static_cast<int>(value));
    }
    
    TagLib::String to_string(unsigned long value) {
        return TagLib::String::number(static_cast<long>(value));
    }
    
    TagLib::String to_string(unsigned long long value) {
        return TagLib::String::number(static_cast<long>(value));
    }
    
    TagLib::String to_string(float value) {
        std::ostringstream oss;
        oss << value;
        return TagLib::String(oss.str());
    }
    
    TagLib::String to_string(double value) {
        std::ostringstream oss;
        oss << value;
        return TagLib::String(oss.str());
    }
    
    TagLib::String to_string(long double value) {
        std::ostringstream oss;
        oss << value;
        return TagLib::String(oss.str());
    }
}
