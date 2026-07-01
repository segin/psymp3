/*
 * FileDialog.cpp - Native "open file" chooser backends (PsyMP3::Core)
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

// This TU intentionally does NOT include psymp3.h: it is the only place a GUI
// toolkit (Qt/GTK) is pulled in, and mixing those with SDL/X11/TagLib headers
// invites macro and type collisions. The backend family + major version are
// selected by configure (config.h), which this file includes directly.

#include "config.h"
#include "core/FileDialog.h"

// ===========================================================================
// Qt (3 / 4 / 5 / 6)
// ===========================================================================
#if defined(PSYMP3_FILEDIALOG_QT)

#if PSYMP3_QT_MAJOR >= 4
#include <QApplication>
#include <QFileDialog>
#include <QString>
#include <QStringList>
#include <QByteArray>
#else // Qt 3
#include <qapplication.h>
#include <qfiledialog.h>
#include <qstring.h>
#include <qstringlist.h>
#endif

namespace {

std::string qstr_to_utf8(const QString& s)
{
#if PSYMP3_QT_MAJOR >= 4
    QByteArray b = s.toUtf8();
    return std::string(b.constData(), static_cast<size_t>(b.size()));
#else
    // Qt3: QString::utf8() returns a QCString convertible to const char*.
    const char* c = s.utf8();
    return c ? std::string(c) : std::string();
#endif
}

QString build_filter(const std::vector<std::string>& exts)
{
    if (exts.empty()) {
        return QString();
    }
    std::string pats;
    for (size_t i = 0; i < exts.size(); ++i) {
        if (i) pats += " ";
        pats += "*.";
        pats += exts[i];
    }
    std::string f = "Audio files (" + pats + ");;All files (*)";
    return QString::fromUtf8(f.c_str());
}

void ensure_qapp()
{
    if (!qApp) {
        // Leaked on purpose: lives for the process. Destroying a QApplication
        // during SDL teardown is asking for trouble.
        static int s_argc = 1;
        static char s_arg0[] = "psymp3";
        static char* s_argv[] = { s_arg0, nullptr };
        new QApplication(s_argc, s_argv);
    }
}

} // namespace

namespace PsyMP3 {
namespace Core {

std::vector<std::string> FileDialog::openFiles(bool allow_multiple,
                                               const std::string& title,
                                               const std::vector<std::string>& extensions)
{
    std::vector<std::string> result;
    ensure_qapp();
    const QString caption = QString::fromUtf8(title.c_str());
    const QString filter = build_filter(extensions);

    if (allow_multiple) {
#if PSYMP3_QT_MAJOR >= 4
        QStringList files = QFileDialog::getOpenFileNames(nullptr, caption, QString(), filter);
#else
        QStringList files = QFileDialog::getOpenFileNames(filter, QString::null, 0, 0, caption);
#endif
        for (QStringList::const_iterator it = files.begin(); it != files.end(); ++it) {
            result.push_back(qstr_to_utf8(*it));
        }
    } else {
#if PSYMP3_QT_MAJOR >= 4
        QString file = QFileDialog::getOpenFileName(nullptr, caption, QString(), filter);
#else
        QString file = QFileDialog::getOpenFileName(QString::null, filter, 0, 0, caption);
#endif
        if (!file.isEmpty()) {
            result.push_back(qstr_to_utf8(file));
        }
    }
    return result;
}

} // namespace Core
} // namespace PsyMP3

// ===========================================================================
// GTK+ (2 / 3 / 4)
// ===========================================================================
#elif defined(PSYMP3_FILEDIALOG_GTK)

#include <gtk/gtk.h>

namespace {

void add_filters(GtkFileChooser* chooser, const std::vector<std::string>& exts)
{
    if (!exts.empty()) {
        GtkFileFilter* audio = gtk_file_filter_new();
        gtk_file_filter_set_name(audio, "Audio files");
        for (const auto& e : exts) {
            std::string pat = "*." + e;
            gtk_file_filter_add_pattern(audio, pat.c_str());
        }
        gtk_file_chooser_add_filter(chooser, audio);
    }
    GtkFileFilter* all = gtk_file_filter_new();
    gtk_file_filter_set_name(all, "All files");
    gtk_file_filter_add_pattern(all, "*");
    gtk_file_chooser_add_filter(chooser, all);
}

} // namespace

namespace PsyMP3 {
namespace Core {

std::vector<std::string> FileDialog::openFiles(bool allow_multiple,
                                               const std::string& title,
                                               const std::vector<std::string>& extensions)
{
    std::vector<std::string> result;

#if PSYMP3_GTK_MAJOR >= 4
    // GTK4 removed gtk_dialog_run(); drive a GtkFileChooserNative with a nested
    // main loop and collect the GFiles afterwards.
    if (!gtk_init_check()) {
        return result;
    }
    GtkFileChooserNative* native = gtk_file_chooser_native_new(
        title.c_str(), nullptr, GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", "_Cancel");
    GtkFileChooser* chooser = GTK_FILE_CHOOSER(native);
    gtk_file_chooser_set_select_multiple(chooser, allow_multiple ? TRUE : FALSE);
    add_filters(chooser, extensions);

    struct RunState { GMainLoop* loop; int response; };
    RunState state{ g_main_loop_new(nullptr, FALSE), GTK_RESPONSE_CANCEL };
    g_signal_connect(native, "response",
        G_CALLBACK(+[](GtkNativeDialog*, int response, gpointer data) {
            RunState* st = static_cast<RunState*>(data);
            st->response = response;
            g_main_loop_quit(st->loop);
        }), &state);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
    g_main_loop_run(state.loop);
    g_main_loop_unref(state.loop);

    if (state.response == GTK_RESPONSE_ACCEPT) {
        GListModel* files = gtk_file_chooser_get_files(chooser);
        guint n = files ? g_list_model_get_n_items(files) : 0;
        for (guint i = 0; i < n; ++i) {
            GFile* gf = static_cast<GFile*>(g_list_model_get_item(files, i));
            char* path = gf ? g_file_get_path(gf) : nullptr;
            if (path) {
                result.emplace_back(path);
                g_free(path);
            }
            if (gf) g_object_unref(gf);
        }
        if (files) g_object_unref(files);
    }
    g_object_unref(native);
#else
    // GTK2/GTK3: classic modal gtk_dialog_run().
    if (!gtk_init_check(nullptr, nullptr)) {
        return result;
    }
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        title.c_str(), nullptr, GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        static_cast<const char*>(nullptr));
    GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_select_multiple(chooser, allow_multiple ? TRUE : FALSE);
    add_filters(chooser, extensions);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GSList* files = gtk_file_chooser_get_filenames(chooser);
        for (GSList* it = files; it; it = it->next) {
            char* path = static_cast<char*>(it->data);
            if (path) {
                result.emplace_back(path);
                g_free(path);
            }
        }
        g_slist_free(files);
    }
    gtk_widget_destroy(dialog);
    // Let the dialog window actually disappear before we return to SDL.
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
#endif
    return result;
}

} // namespace Core
} // namespace PsyMP3

// ===========================================================================
// Win32 (comdlg32)
// ===========================================================================
#elif defined(PSYMP3_FILEDIALOG_WIN32)

#include <windows.h>
#include <commdlg.h>

namespace {

std::wstring utf8_to_wide(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string wide_to_utf8(const wchar_t* w)
{
    if (!w || !*w) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

} // namespace

namespace PsyMP3 {
namespace Core {

std::vector<std::string> FileDialog::openFiles(bool allow_multiple,
                                               const std::string& title,
                                               const std::vector<std::string>& extensions)
{
    std::vector<std::string> result;
    std::wstring wtitle = utf8_to_wide(title);

    // Build the (double-NUL terminated) filter string.
    std::wstring pats;
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (i) pats += L";";
        pats += L"*." + utf8_to_wide(extensions[i]);
    }
    if (pats.empty()) pats = L"*.*";
    std::wstring filter;
    filter += L"Audio files"; filter.push_back(L'\0'); filter += pats;     filter.push_back(L'\0');
    filter += L"All Files";   filter.push_back(L'\0'); filter += L"*.*";   filter.push_back(L'\0');
    filter.push_back(L'\0');

    std::vector<wchar_t> buf(64 * 1024, L'\0');
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.lpstrTitle = wtitle.empty() ? nullptr : wtitle.c_str();
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (allow_multiple) ofn.Flags |= OFN_ALLOWMULTISELECT;

    if (GetOpenFileNameW(&ofn)) {
        const wchar_t* p = buf.data();
        std::wstring first = p;
        p += first.size() + 1;
        if (*p == L'\0') {
            // Single file: 'first' is the full path.
            result.push_back(wide_to_utf8(first.c_str()));
        } else {
            // Multi: 'first' is the directory, the rest are bare filenames.
            while (*p) {
                std::wstring name = p;
                p += name.size() + 1;
                std::wstring full = first;
                if (!full.empty() && full.back() != L'\\') full += L"\\";
                full += name;
                result.push_back(wide_to_utf8(full.c_str()));
            }
        }
    }
    return result;
}

} // namespace Core
} // namespace PsyMP3

// ===========================================================================
// No backend
// ===========================================================================
#else

namespace PsyMP3 {
namespace Core {

std::vector<std::string> FileDialog::openFiles(bool, const std::string&,
                                               const std::vector<std::string>&)
{
    return {};
}

} // namespace Core
} // namespace PsyMP3

#endif
