/*
 * system.cpp - Operating system interfaces and misc. functions.
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
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
#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/mman.h>
#include <cstring>
#include <cerrno>
#endif

#ifdef _WIN32
// For setting thread name in the Visual Studio debugger
[[maybe_unused]] const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;     // Must be 0x1000.
  LPCSTR szName;    // Pointer to name (in user addr space).
  DWORD dwThreadID; // Thread ID (-1=caller thread).
  DWORD dwFlags;    // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
#endif

#ifdef _WIN32
SDL_Window* System::s_main_window = nullptr;
#endif

/**
 * @brief Percent-encodes a wide string.
 *
 * This function is used for the KVIrc IPC interface, which expects file paths
 * to be percent-encoded. It preserves unreserved characters (alphanumeric, '-',
 * '.', '_', '~') and encodes all others according to RFC 3986.
 *
 * @param input The wide string to encode.
 * @return The percent-encoded wide string.
 */
[[maybe_unused]] static std::wstring percentEncodeW(const std::wstring &input) {
  std::wstringstream encoded;
  for (wchar_t wc : input) {
    if (iswalnum(wc) || wc == L'-' || wc == L'.' || wc == L'_' || wc == L'~') {
      encoded << wc;
    } else {
      encoded << L'%' << std::hex << std::uppercase << std::setw(2)
              << std::setfill(L'0') << static_cast<unsigned int>(wc);
    }
  }
  return encoded.str();
}

/**
 * @brief Constructs the System object.
 *
 * On Windows, initialises the taskbar progress interface via
 * `InitializeTaskbar()`. No-op on other platforms.
 */
System::System() {
#ifdef _WIN32
  // Initialize m_taskbar to nullptr only if it's a member (i.e., on Windows)
  m_taskbar = nullptr;  // For taskbar features
  m_ipc_hwnd = nullptr; // For Winamp IPC
  InitializeTaskbar();
#endif
}

/**
 * @brief Destroys the System object.
 *
 * On Windows, destroys the Winamp IPC window and releases the
 * `ITaskbarList3` COM object if they were created.
 */
System::~System() {
#if defined(_WIN32)
  if (m_ipc_hwnd) {
    DestroyWindow(m_ipc_hwnd);
  }
  if (m_taskbar) {
    m_taskbar->Release();
    m_taskbar = nullptr;
  }
  if (m_thumb_icon_prev)  DestroyIcon(m_thumb_icon_prev);
  if (m_thumb_icon_play)  DestroyIcon(m_thumb_icon_play);
  if (m_thumb_icon_pause) DestroyIcon(m_thumb_icon_pause);
  if (m_thumb_icon_next)  DestroyIcon(m_thumb_icon_next);
#endif
}

#ifdef _WIN32
void System::setMainWindow(SDL_Window* window) { s_main_window = window; }

/**
 * @brief Initialises the Winamp-compatible IPC window for inter-process communication (Windows only).
 *
 * Registers a hidden window class `"Winamp v1.x"` and creates a hidden window.
 * Other applications (e.g., KVIrc, Windows Messenger) can send `WM_USER` or
 * `WM_COMMAND` messages to this window to control playback or retrieve metadata.
 *
 * @param player Pointer to the `Player` instance, stored in the window's user data.
 */
void System::InitializeIPC(Player *player) {
  WNDCLASSEXW wcx{};
  wcx.cbSize = sizeof(WNDCLASSEXW);
  wcx.lpfnWndProc = System::ipcWndProc;
  wcx.hInstance = GetModuleHandleW(nullptr);
  wcx.lpszClassName = L"Winamp v1.x";

  if (!RegisterClassExW(&wcx)) {
    Debug::log("system", "Failed to register Winamp IPC window class. Error: ", GetLastError());
    return;
  }

  // Create a hidden window to listen for Winamp IPC messages.
  // We pass the 'player' pointer as the creation parameter.
  // This allows the static ipcWndProc to retrieve it and interact with the
  // player instance.
  m_ipc_hwnd =
      CreateWindowExW(0,                          // Optional window styles.
                      L"Winamp v1.x",             // Window class
                      L"PsyMP3 Winamp Interface", // Window text
                      0,                          // Window style (not visible)
                      0, 0, 0, 0,                 // Position and size (hidden)
                      nullptr,                    // Parent window
                      nullptr,                    // Menu
                      GetModuleHandleW(nullptr),  // Instance handle
                      player // Additional application data (the Player pointer)
      );

  if (!m_ipc_hwnd) {
    Debug::log("system", "Failed to create Winamp IPC window. Error: ", GetLastError());
  }
}
#endif

#ifdef _WIN32
/**
 * @brief Finds all MsnMsgrUIManager windows and sends them a WM_COPYDATA
 * message.
 * @param message The wide-character string payload, with fields separated by
 * nulls.
 */
void System::broadcastMsnMessage(const std::wstring &message) {
  COPYDATASTRUCT cds;
  cds.dwData = 1351; // Magic number for MSN Now Playing IPC
  // The size is the total number of bytes, including the final null terminator.
  cds.cbData = (message.length() + 1) * sizeof(wchar_t);
  cds.lpData =
      const_cast<PVOID>(reinterpret_cast<const void *>(message.c_str()));

  HWND msgr_hwnd = nullptr;
  // Loop through all top-level windows to find all messenger instances.
  do {
    msgr_hwnd = FindWindowExW(nullptr, msgr_hwnd, L"MsnMsgrUIManager", nullptr);
    if (msgr_hwnd) {
      // Using SendMessage for synchronous delivery.
      SendMessage(msgr_hwnd, WM_COPYDATA, (WPARAM)getHwnd(), (LPARAM)&cds);
    }
  } while (msgr_hwnd != nullptr);
}

/**
 * @brief Announces the currently playing track to MSN-compatible messengers.
 * @param artist The artist of the track.
 * @param title The title of the track.
 * @param album The album of the track.
 */
void System::announceNowPlaying(const TagLib::String &artist,
                                const TagLib::String &title,
                                const TagLib::String &album) {
  std::wstringstream wss;
  // Format string with embedded nulls:
  // {PlayerName}\0{MessageType}\0{Enabled}\0{FormatString}\0{Title}\0{Artist}\0{Album}\0{ContentID}\0
  wss << L"PsyMP3" << L"\\0" << L"Music" << L"\\0" << L"1"
      << L"\\0"                         // 1 = Enabled
      << L"PsyMP3: {1} - {0}" << L"\\0" // Format: {1} is artist, {0} is title
      << title.toWString() << L"\\0" << artist.toWString() << L"\\0"
      << album.toWString() << L"\\0" << L"WMContentID" << L"\\0";

  broadcastMsnMessage(wss.str());
}

/**
 * @brief Clears the "Now Playing" status from MSN-compatible messengers.
 */
void System::clearNowPlaying() {
  // To clear, we send a message with the "Enabled" field set to 0.
  std::wstring clear_message = L"PsyMP3\\0Music\\00\\0\\0\\0\\0\\0\\0";
  broadcastMsnMessage(clear_message);
}
#endif

#ifdef _WIN32
/**
 * @brief Win32 window procedure for the Winamp-compatible IPC window (Windows only).
 *
 * Handles `WM_COMMAND` (transport controls), `WM_USER` (Winamp IPC queries
 * and KVIrc metadata transfer), and forwards all other messages to
 * `DefWindowProc`.
 *
 * @param hWnd  Handle to the IPC window.
 * @param uMsg  Windows message identifier.
 * @param wParam Message-specific parameter.
 * @param lParam Message-specific parameter.
 * @return Message-specific return value.
 */
LRESULT CALLBACK System::ipcWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam) {
  // On first creation, store the Player pointer passed in CreateWindowEx
  if (uMsg == WM_NCCREATE) {
    CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
    SetWindowLongPtr(hWnd, GWLP_USERDATA,
                     reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
  }

  // Retrieve the Player pointer for subsequent messages
  Player *player =
      reinterpret_cast<Player *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  if (!player) {
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
  }

  // These static buffers are needed because some legacy IPC calls return a
  // direct pointer to internal data. This is an unsafe pattern required for
  // compatibility.
  static std::string currentFileA;
  static std::wstring currentFileW;
  static std::string currentTitleA;
  static std::wstring currentTitleW;
  static std::wstring kvircBufferW;

  switch (uMsg) {
  case WM_COMMAND:
    switch (wParam) {
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

  case WM_USER: {
    // KVIrc-specific IPC interface. This is checked before the standard Winamp
    // IPC.
    if (wParam == KVIRC_WM_USER) {
      switch (lParam) {
      case KVIRC_WM_USER_CHECK:
        return KVIRC_WM_USER_CHECK_REPLY;

      case KVIRC_WM_USER_GETTITLE:
        if (player->stream) {
          currentTitleW = player->stream->getArtist().toWString() + L" - " +
                          player->stream->getTitle().toWString();
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
        if (lParam >= KVIRC_WM_USER_TRANSFER &&
            lParam < KVIRC_WM_USER_TRANSFER + 4096) {
          size_t index = lParam - KVIRC_WM_USER_TRANSFER;
          if (index < kvircBufferW.length()) {
            return kvircBufferW[index];
          }
        }
        return 0;
      }
    }

    // Standard Winamp IPC messages
    switch (lParam) {
    case IPC_GETVERSION:
      return 0x2091; // Report as Winamp 2.91
    case IPC_ISPLAYING:
      switch (player->state) {
      case PlayerState::Playing:
        return 1;
      case PlayerState::Paused:
        return 3;
      default:
        return 0; // PlayerState::Stopped
      }
    case IPC_GETOUTPUTTIME:
      if (player->stream) {
        if (wParam == 0)
          return player->stream->getPosition(); // Position in ms
        if (wParam == 1)
          return player->stream->getLength() / 1000; // Length in seconds
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
        currentTitleA = player->stream->getArtist().to8Bit(true) + " - " +
                        player->stream->getTitle().to8Bit(true);
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

/**
 * @brief Initialises the Windows Taskbar progress overlay (Windows only).
 *
 * Attempts to create an `ITaskbarList3` COM object. If successful, calls
 * `HrInit()` to initialise it. Does nothing if `WIN_OPTIONAL` is not defined.
 */
void System::InitializeTaskbar() {
#if defined(_WIN32)
  // SDL's video init already puts the main thread into an STA, but request it
  // defensively -- CoCreateInstance needs an initialized apartment. A benign
  // RPC_E_CHANGED_MODE just means SDL got there first, which is fine.
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  HRESULT hr =
      CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                       IID_ITaskbarList3, reinterpret_cast<void **>(&m_taskbar));



  if (SUCCEEDED(hr)) {
    Debug::log("system", "ITaskbarList3 COM object: ", std::hex, m_taskbar);
    hr = m_taskbar->HrInit();

    if (FAILED(hr)) {
      Debug::log("system", "Error initializing ITaskbarList3 COM object!");
      m_taskbar->Release();
      m_taskbar = nullptr;
    }
  } else
    Debug::log("system", "Error initializing ITaskbarList3 COM object!");
#endif
}

/**
 * @brief Returns the current OS username.
 *
 * On Windows uses `GetUserNameW`; on POSIX reads the `USER` environment variable.
 *
 * @return Username as a `TagLib::String`.
 */
TagLib::String System::getUser() {
#ifdef _WIN32
  WCHAR user[48];
  DWORD bufsize = 48;
  GetUserNameW(user, &bufsize);
  return user;
#else
  // getenv may return nullptr (e.g. USER unset under cron/daemons);
  // constructing a TagLib::String from nullptr would crash.
  const char *user = getenv("USER");
  return user ? TagLib::String(user, TagLib::String::UTF8) : TagLib::String();
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

/**
 * @brief Returns the current user's home directory path.
 *
 * On Windows, tries (in order): the `HOME` env var, `USERPROFILE`, a
 * registry query for the "My Documents" path, and finally falls back to
 * `C:\My Documents`. On POSIX, reads the `HOME` environment variable.
 *
 * @return Home directory path as a `TagLib::String`.
 */
TagLib::String System::getHome() {
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
  // 3. Query the registry for the "My Documents" path (most reliable for
  // Win9x+).
  HKEY hKey;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\S"
                    L"hell Folders",
                    0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    DWORD buffer_size = sizeof(env_buffer);
    if (RegQueryValueExW(hKey, L"Personal", nullptr, nullptr, (LPBYTE)env_buffer,
                         &buffer_size) == ERROR_SUCCESS) {
      RegCloseKey(hKey);
      return TagLib::String(env_buffer);
    }
    RegCloseKey(hKey);
  }
  // 4. Final fallback for very old systems or unusual configurations.
  return TagLib::String("C:\\My Documents", TagLib::String::Latin1);
#else
  const char *home = getenv("HOME");
  return home ? TagLib::String(home, TagLib::String::UTF8) : TagLib::String();
#endif
}

/* This function returns the storage path. This is determined based on
 * the %AppData% environment variable on Windows, and the $HOME
 * environment variable on Unix.
 */

/**
 * @brief Returns the path where PsyMP3 stores its configuration and data.
 *
 * On Windows: `%%APPDATA%%\PsyMP3`. On POSIX: `$XDG_CONFIG_HOME/psymp3`
 * (or `~/.config/psymp3` if `XDG_CONFIG_HOME` is unset).
 *
 * @return Storage path as a `TagLib::String`.
 */
TagLib::String System::getStoragePath() {
#ifdef _WIN32
  TagLib::String spath;
  WCHAR env[1024];
  GetEnvironmentVariableW(L"APPDATA", env, 1024);
  spath = env; // Changed this line, it was not returning from the right spot.
  spath += L"\\PsyMP3";
  return spath;
#else
  // Use XDG config directory for unified storage
  const char *xdg_config = getenv("XDG_CONFIG_HOME");
  if (xdg_config && strlen(xdg_config) > 0) {
    return TagLib::String(xdg_config, TagLib::String::UTF8) + "/psymp3";
  } else {
    return getHome() + "/.config/psymp3";
  }
#endif
}

/* Creates the storage path specified by getStoragePath() if it does
 * not already exist.
 */

/**
 * @brief Creates the storage path directory if it does not already exist.
 *
 * Uses `CreateDirectoryW` on Windows and `mkdir` on POSIX. Returns `true`
 * both on successful creation and when the directory already exists.
 *
 * @return `true` if the directory exists or was created, `false` on other errors.
 */
bool System::createStoragePath() {
  TagLib::String path = getStoragePath();
#ifdef _WIN32
  // CreateDirectoryW returns non-zero on success.
  return CreateDirectoryW(path.toWString().c_str(), nullptr) ||
         (GetLastError() == ERROR_ALREADY_EXISTS);
#else
  // mkdir returns 0 on success.
  return mkdir(path.toCString(true), 0700) == 0 || (errno == EEXIST);
#endif
}

std::filesystem::path System::pathFromUtf8(const std::string& utf8) {
#ifdef _WIN32
  return std::filesystem::path(TagLib::String(utf8, TagLib::String::UTF8).toWString());
#else
  return std::filesystem::path(utf8);
#endif
}

#ifdef _WIN32
/**
 * @brief Returns the Win32 `HWND` of the SDL application window (Windows only).
 * @return The window handle, or 0 if it cannot be retrieved.
 */
HWND System::getHwnd() {
  if (!s_main_window) {
    return 0;
  }

  SDL_SysWMinfo wmi{};
  SDL_VERSION(&wmi.version);

  if (!SDL_GetWindowWMInfo(s_main_window, &wmi))
    return 0;

  return wmi.info.win.window;
}

/**
 * @brief Updates the Windows taskbar progress bar value (Windows only).
 * @param now Current progress value.
 * @param max Maximum progress value.
 */
void System::updateProgress(ULONGLONG now, ULONGLONG max) {
  if (m_taskbar)
    m_taskbar->SetProgressValue(getHwnd(), now, max);
  else
    Debug::log("system", "System::updateProgress(): No ITaskbarList3 OLE interface!");
}
/**
 * @brief Sets the Windows taskbar button progress indicator state (Windows only).
 * @param status One of the `TBPFLAG` values (e.g., `TBPF_NORMAL`, `TBPF_INDETERMINATE`).
 */
void System::progressState(TBPFLAG status) {
  Debug::log("system", "System::updateProgress(): Called.");
  if (m_taskbar)
    m_taskbar->SetProgressState(getHwnd(), status);
  else
    Debug::log("system", "System::updateProgress(): No ITaskbarList3 OLE interface!");
}

// --- Windows 7+ taskbar thumbnail-toolbar (window-preview) transport buttons --
//
// libseven (libs/libseven) wrapped ITaskbarList3 but never finished the thumb
// buttons -- it allocated a THUMBBUTTON array yet never set the count, assigned
// no icons/IDs, and, crucially, never called ThumbBarAddButtons, so no buttons
// ever appeared. This implements the piece it was missing, against the
// ITaskbarList3 the System object already owns.
//
// The buttons need HICONs. Rather than ship .ico assets (which would break the
// single-file exe), the Prev / Play / Pause / Next glyphs are rasterized at
// runtime into a 32-bit BGRA top-down DIB and wrapped as an alpha icon. The
// shapes (a filled rectangle and a filled triangle) are filled by hand so every
// pixel's alpha is set explicitly -- no reliance on GDI's alpha behaviour.
//
// Named (not anonymous) namespace so the helper symbols stay unique in the
// --enable-final unity TU, where this file is #included alongside every other.
namespace psymp3_win_thumbbar {

enum GlyphKind { GLYPH_PREV, GLYPH_PLAY, GLYPH_PAUSE, GLYPH_NEXT };

static inline void putPixel(uint8_t* bgra, int W, int x, int y) {
    uint8_t* p = bgra + (static_cast<size_t>(y) * W + x) * 4;
    p[0] = 255; p[1] = 255; p[2] = 255; p[3] = 255; // opaque white, BGRA
}

static void fillRect(uint8_t* bgra, int W, int H, int x0, int y0, int x1, int y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            putPixel(bgra, W, x, y);
}

// Fill triangle (ax,ay)-(bx,by)-(cx,cy) via a bounding-box half-plane test.
// Trivial cost at icon sizes (<= a few hundred pixels).
static void fillTriangle(uint8_t* bgra, int W, int H,
                         float ax, float ay, float bx, float by, float cx, float cy) {
    float lo = ax < bx ? ax : bx; if (cx < lo) lo = cx;
    float hi = ax > bx ? ax : bx; if (cx > hi) hi = cx;
    float to = ay < by ? ay : by; if (cy < to) to = cy;
    float bo = ay > by ? ay : by; if (cy > bo) bo = cy;
    int minx = (int)lo,      maxx = (int)hi + 1;
    int miny = (int)to,      maxy = (int)bo + 1;
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx > W) maxx = W;
    if (maxy > H) maxy = H;
    auto edge = [](float x0, float y0, float x1, float y1, float px, float py) {
        return (px - x0) * (y1 - y0) - (py - y0) * (x1 - x0);
    };
    for (int y = miny; y < maxy; ++y) {
        for (int x = minx; x < maxx; ++x) {
            float px = x + 0.5f, py = y + 0.5f;
            float w0 = edge(ax, ay, bx, by, px, py);
            float w1 = edge(bx, by, cx, cy, px, py);
            float w2 = edge(cx, cy, ax, ay, px, py);
            if ((w0 <= 0 && w1 <= 0 && w2 <= 0) || (w0 >= 0 && w1 >= 0 && w2 >= 0))
                putPixel(bgra, W, x, y);
        }
    }
}

static HICON makeGlyphIcon(GlyphKind kind) {
    int sz = GetSystemMetrics(SM_CXSMICON);
    const int W = sz > 0 ? sz : 16;
    const int H = W;

    BITMAPINFO bi;
    std::memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!color || !bits) {
        if (color) DeleteObject(color);
        return nullptr;
    }

    uint8_t* buf = static_cast<uint8_t*>(bits);
    std::memset(buf, 0, static_cast<size_t>(W) * H * 4); // fully transparent

    const float pad   = W * 0.22f;
    const float bar   = (W * 0.16f < 2.0f) ? 2.0f : W * 0.16f; // vertical-bar width
    const float top   = pad;
    const float bot   = H - pad;
    const float left  = pad;
    const float right = W - pad;
    const float midY  = H * 0.5f;

    switch (kind) {
        case GLYPH_PLAY:
            fillTriangle(buf, W, H, left, top, left, bot, right, midY);
            break;
        case GLYPH_PAUSE:
            fillRect(buf, W, H, (int)left, (int)top, (int)(left + bar), (int)bot);
            fillRect(buf, W, H, (int)(right - bar), (int)top, (int)right, (int)bot);
            break;
        case GLYPH_PREV:
            fillRect(buf, W, H, (int)left, (int)top, (int)(left + bar), (int)bot);
            fillTriangle(buf, W, H, right, top, right, bot, left + bar, midY);
            break;
        case GLYPH_NEXT:
            fillTriangle(buf, W, H, left, top, left, bot, right - bar, midY);
            fillRect(buf, W, H, (int)(right - bar), (int)top, (int)right, (int)bot);
            break;
    }

    // Monochrome AND mask, explicitly zeroed: transparency comes from the color
    // bitmap's per-pixel alpha, so the mask must be all 0 (draw color anywhere).
    int mask_stride = ((W + 15) / 16) * 2; // 1bpp scanlines are WORD-aligned
    std::vector<uint8_t> mask_bits(static_cast<size_t>(mask_stride) * H, 0);
    HBITMAP mask = CreateBitmap(W, H, 1, 1, mask_bits.data());

    ICONINFO ii;
    std::memset(&ii, 0, sizeof(ii));
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = mask;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(color);
    if (mask) DeleteObject(mask);
    return icon;
}

} // namespace psymp3_win_thumbbar

void System::fillThumbButtons(THUMBBUTTON* out, bool playing) {
    std::memset(out, 0, sizeof(THUMBBUTTON) * 3);

    out[0].dwMask  = (THUMBBUTTONMASK)(THB_ICON | THB_TOOLTIP | THB_FLAGS);
    out[0].iId     = PSYMP3_THUMB_PREV;
    out[0].hIcon   = m_thumb_icon_prev;
    out[0].dwFlags = THBF_ENABLED;
    lstrcpynW(out[0].szTip, L"Previous", 260);

    out[1].dwMask  = (THUMBBUTTONMASK)(THB_ICON | THB_TOOLTIP | THB_FLAGS);
    out[1].iId     = PSYMP3_THUMB_PLAYPAUSE;
    out[1].hIcon   = playing ? m_thumb_icon_pause : m_thumb_icon_play;
    out[1].dwFlags = THBF_ENABLED;
    lstrcpynW(out[1].szTip, playing ? L"Pause" : L"Play", 260);

    out[2].dwMask  = (THUMBBUTTONMASK)(THB_ICON | THB_TOOLTIP | THB_FLAGS);
    out[2].iId     = PSYMP3_THUMB_NEXT;
    out[2].hIcon   = m_thumb_icon_next;
    out[2].dwFlags = THBF_ENABLED;
    lstrcpynW(out[2].szTip, L"Next", 260);
}

void System::setupThumbBar() {
    if (!m_taskbar)
        return;
    HWND hwnd = getHwnd();
    if (!hwnd)
        return;

    using namespace psymp3_win_thumbbar;
    if (!m_thumb_icon_prev)  m_thumb_icon_prev  = makeGlyphIcon(GLYPH_PREV);
    if (!m_thumb_icon_play)  m_thumb_icon_play  = makeGlyphIcon(GLYPH_PLAY);
    if (!m_thumb_icon_pause) m_thumb_icon_pause = makeGlyphIcon(GLYPH_PAUSE);
    if (!m_thumb_icon_next)  m_thumb_icon_next  = makeGlyphIcon(GLYPH_NEXT);

    THUMBBUTTON btns[3];
    fillThumbButtons(btns, /*playing=*/false);

    HRESULT hr;
    if (!m_thumb_added) {
        hr = m_taskbar->ThumbBarAddButtons(hwnd, 3, btns);
        if (SUCCEEDED(hr))
            m_thumb_added = true;
    } else {
        hr = m_taskbar->ThumbBarUpdateButtons(hwnd, 3, btns);
    }
    Debug::log("system", "System::setupThumbBar(): added=", m_thumb_added,
               " hr=0x", std::hex, static_cast<unsigned>(hr));
}

void System::updateThumbBarPlayState(bool playing) {
    if (!m_taskbar || !m_thumb_added)
        return;
    HWND hwnd = getHwnd();
    if (!hwnd)
        return;
    THUMBBUTTON btns[3];
    fillThumbButtons(btns, playing);
    m_taskbar->ThumbBarUpdateButtons(hwnd, 3, btns);
}

UINT System::taskbarButtonCreatedMessage() {
    // Registered once by the shell name; the same value process-wide. Thread-safe
    // static init means concurrent callers get one consistent message id.
    static const UINT msg = RegisterWindowMessageW(L"TaskbarButtonCreated");
    return msg;
}

#endif

/**
 * @brief Sets the OS-level name of the calling thread.
 *
 * On Linux, uses `prctl(PR_SET_NAME)`; on FreeBSD, `pthread_set_name_np()`;
 * on Windows 10+, `SetThreadDescription` (loaded dynamically for compatibility),
 * with an MSVC debugger fallback. Names are truncated to 15 characters on
 * Linux and FreeBSD. No-op on unrecognised platforms.
 *
 * @param name Desired thread name (UTF-8).
 */
void System::setThisThreadName([[maybe_unused]] const std::string &name) {
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
  using SetThreadDescription_t = HRESULT(WINAPI *)(HANDLE, PCWSTR);
  auto pSetThreadDescription = reinterpret_cast<SetThreadDescription_t>(GetProcAddress(
      GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription"));

  if (pSetThreadDescription) {
    // Convert std::string (UTF-8) to std::wstring (UTF-16) for the Unicode API
    int size_needed =
        MultiByteToWideChar(CP_UTF8, 0, &name[0], static_cast<int>(name.size()), nullptr, 0);
    std::wstring wname(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &name[0], static_cast<int>(name.size()), &wname[0],
                        size_needed);

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

    __try {
      RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
                     (ULONG_PTR *)&info);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
#endif // _MSC_VER
  }
#else
  // No-op for other platforms.
#endif
}

/**
 * @brief Sets the priority of the calling thread.
 *
 * This is a cross-platform wrapper around SDL_SetThreadPriority.
 *
 * @param priority The desired thread priority.
 */
void System::setThreadPriority(ThreadPriority priority) {
  SDL_ThreadPriority sdl_priority = SDL_THREAD_PRIORITY_NORMAL;

  switch (priority) {
  case ThreadPriority::Low:
    sdl_priority = SDL_THREAD_PRIORITY_LOW;
    break;
  case ThreadPriority::Normal:
    sdl_priority = SDL_THREAD_PRIORITY_NORMAL;
    break;
  case ThreadPriority::High:
    sdl_priority = SDL_THREAD_PRIORITY_HIGH;
    break;
  case ThreadPriority::TimeCritical:
    sdl_priority = SDL_THREAD_PRIORITY_TIME_CRITICAL;
    break;
  }

  if (SDL_SetThreadPriority(sdl_priority) < 0) {
    Debug::log("system", "Warning: Failed to set thread priority: ", SDL_GetError());
  } else {
    Debug::log("system", "Thread priority set successfully");
  }
}

/**
 * @brief Locks the process's virtual address space into RAM.
 *
 * On POSIX systems, this uses `mlockall(MCL_CURRENT | MCL_FUTURE)` to prevent
 * the OS from swapping the process memory to disk, which helps avoid latency
 * spikes in real-time audio code.
 *
 * @return `true` if successful or not supported, `false` if it failed (e.g., due to permissions).
 */
bool System::lockMemory() {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
  if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0) {
    Debug::log("system", "Memory locked successfully");
    return true;
  } else {
    Debug::log("system", "Warning: Failed to lock memory: ", strerror(errno));
    // Often fails due to lack of CAP_IPC_LOCK or rlimit restrictions.
    return false;
  }
#else
  // Not implemented for other platforms
  return true;
#endif
}

// TagLib::String overloads for std::to_string
// NOTE: These are disabled in FINAL_BUILD mode to avoid ambiguity with
// standard library std::to_string in the single compilation unit.
#ifndef FINAL_BUILD
namespace std {
/// @brief `std::to_string` overload returning a `TagLib::String` for `int`.
TagLib::String to_string(int value) { return TagLib::String::number(value); }

/// @brief `std::to_string` overload returning a `TagLib::String` for `long`.
TagLib::String to_string(long value) { return TagLib::String::number(value); }

/// @brief `std::to_string` overload returning a `TagLib::String` for `long long`.
/// Formatted via ostringstream so values outside `long`'s range are not
/// truncated (long is 32-bit on the i386/ARM32 build targets).
TagLib::String to_string(long long value) {
  std::ostringstream oss;
  oss << value;
  return TagLib::String(oss.str());
}

/// @brief `std::to_string` overload returning a `TagLib::String` for `unsigned`.
TagLib::String to_string(unsigned value) {
  std::ostringstream oss;
  oss << value;
  return TagLib::String(oss.str());
}

/// @brief `std::to_string` overload returning a `TagLib::String` for `unsigned long`.
TagLib::String to_string(unsigned long value) {
  std::ostringstream oss;
  oss << value;
  return TagLib::String(oss.str());
}

/// @brief `std::to_string` overload returning a `TagLib::String` for `unsigned long long`.
TagLib::String to_string(unsigned long long value) {
  std::ostringstream oss;
  oss << value;
  return TagLib::String(oss.str());
}

/// @brief `std::to_string` overload returning a `TagLib::String` for `float`.
TagLib::String to_string(float value) {
  std::ostringstream oss;
  oss << value;
  return TagLib::String(oss.str());
}

/// @brief `std::to_string` overload returning a `TagLib::String` for `double`.
TagLib::String to_string(double value) {
  std::ostringstream oss;
  oss << value;
  return TagLib::String(oss.str());
}

/// @brief `std::to_string` overload returning a `TagLib::String` for `long double`.
TagLib::String to_string(long double value) {
  std::ostringstream oss;
  oss << value;
  return TagLib::String(oss.str());
}
} // namespace std
#endif // !FINAL_BUILD
