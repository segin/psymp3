/* This code was used to test the ParseCommandLine and WideCharToUTF8 code 
 * on Windows. 
 */

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
// Convert wide-character string to UTF-8
std::string WideCharToUTF8(const wchar_t* wideStr) {
    int wideStrLen = wcslen(wideStr);
    int utf8StrLen = WideCharToMultiByte(CP_UTF8, 0, wideStr, wideStrLen, NULL, 0, NULL, NULL);
    if (utf8StrLen == 0) {
        return ""; // Conversion failed
    }
    std::vector<char> buffer(utf8StrLen + 1); // +1 for null terminator
    WideCharToMultiByte(CP_UTF8, 0, wideStr, wideStrLen, buffer.data(), utf8StrLen, NULL, NULL);
    return std::string(buffer.data());
}

// Parse command line arguments on Windows
std::vector<std::string> ParseCommandLine(int argc, char *argv[]) {
    std::vector<std::string> args;

    LPWSTR* wideArgv;
    int wideArgc;

    wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);
    if (wideArgv == NULL) {
        std::cerr << "Failed to parse command line" << std::endl;
        exit(1);
    }

    // Convert wide-character argv to UTF-8
    for (int i = 0; i < wideArgc; ++i) {
        args.push_back(WideCharToUTF8(wideArgv[i]));
    }

    LocalFree(wideArgv); // Free memory allocated by CommandLineToArgvW
    return args;
}

#else
// Parse command line arguments on Linux
std::vector<std::string> ParseCommandLine(int argc, char *argv[]) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return args;
}
#endif


int main(int argc, char *argv[]) {
    const auto args = ParseCommandLine(argc, argv);
    auto index = 0;
    for (const auto& arg : args) {
        std::cout << "[" << index++ << "]: " << arg << std::endl;
        for (char c : arg) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(c) << " ";
        }
        std::cout << std::endl;
    }
    return 0;
}
