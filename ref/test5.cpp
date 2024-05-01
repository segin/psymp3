#include <iostream>
#include <string>
#include <vector>
#include <getopt.h>

// Custom getopt_long implementation using std::vector<std::string>
int custom_getopt_long(std::vector<std::string>& args, const char* short_opts,
                       const struct option* long_opts, int& opt_index) {
    if (opt_index >= static_cast<int>(args.size())) {
        // No more arguments
        return -1;
    }

    const std::string& current_arg = args[opt_index];
    if (current_arg.empty() || current_arg[0] != '-') {
        // Not an option
        return -1;
    }

    if (current_arg == "--") {
        // End of argument processing
        opt_index += 1;
        return -1;
    }

    if (current_arg[1] == '-') {
        // Long option
        const std::string long_opt = current_arg.substr(2);
        for (int i = 0; long_opts[i].name != nullptr; ++i) {
            if (long_opts[i].name == long_opt) {
                if (long_opts[i].has_arg == required_argument) {
                    if (opt_index + 1 < static_cast<int>(args.size())) {
                        optarg = args[opt_index + 1]; // Store as std::string
                        opt_index += 2;
                        return long_opts[i].val;
                    } else {
                        // Missing argument
                        return '?';
                    }
                } else {
                    opt_index += 1;
                    return long_opts[i].val;
                }
            }
        }
        // Unknown long option
        return '?';
    } else {
        // Short option
        const char short_opt = current_arg[1];
        for (int i = 0; long_opts[i].name != nullptr; ++i) {
            if (long_opts[i].val == short_opt) {
                if (long_opts[i].has_arg == required_argument) {
                    if (opt_index + 1 < static_cast<int>(args.size())) {
                        optarg = args[opt_index + 1]; // Store as std::string
                        opt_index += 2;
                        return short_opt;
                    } else {
                        // Missing argument
                        return '?';
                    }
                } else {
                    opt_index += 1;
                    return short_opt;
                }
            }
        }
        // Unknown short option
        return '?';
    }
}

std::vector<std::string> ParseCommandLine(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return args;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> commandLineArgs = ParseCommandLine(argc, argv);

    const char* short_opts = "i:o:";
    struct option long_opts[] = {
        {"input", required_argument, nullptr, 'i'},
        {"output", required_argument, nullptr, 'o'},
        {nullptr, 0, nullptr, 0} // End of options
    };

    int opt;
    int opt_index = 0;
    while ((opt = custom_getopt_long(commandLineArgs, short_opts, long_opts, opt_index)) != -1) {
        switch (opt) {
            case 'i':
                std::cout << "Input file: " << optarg << std::endl;
                break;
            case 'o':
                std::cout << "Output file: " << optarg << std::endl;
                break;
            case '?':
                std::cerr << "Invalid option or missing argument." << std::endl;
                break;
            default:
                // Handle other options
                break;
        }
    }

    return 0;
}
