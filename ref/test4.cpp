#include <iostream>
#include <locale>
#include <string>

int main() {
    // Get the current global locale
    std::locale currentLocale("");

    // Check if the current locale is UTF-8
    std::string localeName = currentLocale.name();
    if (localeName.find("UTF-8") == std::string::npos) {
        // Not UTF-8, so set the global locale to UTF-8
        std::locale::global(std::locale("en_US.UTF-8"));
    }

    // Now, all I/O operations will use UTF-8 encoding
    std::cout << "Hello, こんにちは, مرحبا\n";

    return 0;
}
