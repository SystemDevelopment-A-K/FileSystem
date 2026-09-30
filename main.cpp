#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <fstream>
#include <format>
#include <vector>
#include <string_view>
#include <optional>
#include <utility>

namespace fs = std::filesystem;

namespace {
    namespace color {
        constexpr std::string_view reset = "\033[0m";
        constexpr std::string_view red = "\033[1;31m";
        constexpr std::string_view green = "\033[1;32m";
        constexpr std::string_view yellow = "\033[1;33m";
        constexpr std::string_view blue = "\033[1;34m";
        constexpr std::string_view magenta = "\033[1;35m";
        constexpr std::string_view cyan = "\033[1;36m";
        constexpr std::string_view gray = "\033[90m";
    }

    class CommandManager {
    public:
        using CommandFunc = std::function<void(std::istringstream&)>;

        void registerCommand(const std::string& name, const CommandFunc& func) {
            commands[name] = func;
        }

        bool execute(const std::string& name, std::istringstream& input) {
            const auto it = commands.find(name);

            if (it == commands.end()) {
                printError("Unknown command: " + name);
                return false;
            }
            it->second(input);
            return true;
        }

        static void printHighlighted(const std::string& line, const std::string& pattern) {
            size_t pos = 0;
            size_t found;

            while ((found = line.find(pattern, pos)) != std::string::npos) {
                std::cout << line.substr(pos, found - pos);
                std::cout << "\033[1;33m" << pattern << "\033[0m";
                pos = found + pattern.length();
            }

            std::cout << line.substr(pos) << "\n";
        }

        static void printError(const std::string& msg) {
            std::cout << color::red << "[ERROR] " << msg << color::reset << "\n";
        }

        static void printSuccess(const std::string& msg) {
            std::cout << color::green << "[OK] " << msg << color::reset << "\n";
        }

        static void printInfo(const std::string& msg) {
            std::cout << color::cyan << msg << color::reset << "\n";
        }

        static std::optional<std::string> readArguments(std::istringstream& args, const std::string& errorMessage) {
            std::string value;
            if (!(args >> value)) {
                printError(errorMessage);
                return std::nullopt;
            }
            return value;
        }

        static std::string permissionsToString(fs::perms p) {
            auto bit = [p](const fs::perms flag, char c) {
                return (p & flag) != fs::perms::none ? c : '-';
            };

            std::string result;

            result += bit(fs::perms::owner_read,  'r');
            result += bit(fs::perms::owner_write, 'w');
            result += bit(fs::perms::owner_exec,  'x');

            result += bit(fs::perms::group_read,  'r');
            result += bit(fs::perms::group_write, 'w');
            result += bit(fs::perms::group_exec,  'x');

            result += bit(fs::perms::others_read,  'r');
            result += bit(fs::perms::others_write, 'w');
            result += bit(fs::perms::others_exec,  'x');

            return result;
        }

        static std::string fileTypeToString(const fs::file_type type) {
            switch (type) {
                case fs::file_type::regular: return "regular file";
                case fs::file_type::directory: return "directory";
                case fs::file_type::symlink: return "symlink";
                case fs::file_type::block: return "block device";
                case fs::file_type::character: return "character device";
                case fs::file_type::fifo: return "fifo";
                case fs::file_type::socket: return "socket";
                case fs::file_type::not_found: return "not found";
                default: return "unknown";
            }
        }
    private:
        std::unordered_map<std::string, CommandFunc> commands;
    };
}

int main() {
    CommandManager cmd;
    bool running = true;

    cmd.registerCommand("ls", [](std::istringstream&) {
        for (const auto& entry : fs::directory_iterator(fs::current_path())) {
            const bool isDir = entry.is_directory();
            const std::string_view nameColor = isDir ? color::magenta : color::blue;

            std::cout << nameColor << (isDir ? "[DIR]  " : "[FILE] ")
                        << entry.path().filename().string() << color::reset;

            if (entry.is_regular_file()) {
                std::cout << color::gray << " (" << entry.file_size() << " bytes)" << color::reset;
            }

            std::cout << color::gray << " [" << std::format("{:%Y-%m-%d %H:%M}", entry.last_write_time()) << "]"
                        << color::reset << "\n";
        }
    });

    cmd.registerCommand("mkdir", [](std::istringstream& args) {
        const auto folderName = CommandManager::readArguments(args, "You forgot to specify directory name!");

        if (!folderName) return;

        if (std::error_code ec; fs::create_directory(*folderName, ec)) {
            CommandManager::printSuccess("Directory successfully created!");
        } else if (ec) {
            CommandManager::printError("Directory creation failed: " + ec.message());
        } else {
            CommandManager::printError("Directory already exists!");
        }
    });

    cmd.registerCommand("cd", [](std::istringstream& args) {
        const auto optional = CommandManager::readArguments(args, "You forgot to specify directory name!");

        if (!optional) return;

        std::error_code ec;
        fs::current_path(*optional, ec);

        if (ec) {
            CommandManager::printError("Failed to change directory: " + ec.message());
        }
    });

    cmd.registerCommand("rm", [](std::istringstream& args) {
        const auto target = CommandManager::readArguments(args, "You forgot to specify directory or file name!");

        if (!target) return;

        if (std::error_code ec; fs::remove(*target, ec)) {
            std::cout << "\033[1;32m[OK] Item successfully removed!\033[0m\n";
        } else if (ec) {
            std::cout << "\033[1;31m[ERROR] Failed to remove item: " << ec.message() << "\033[0m\n";
        } else {
            std::cout << "\033[1;31m[ERROR] Target item was not found!\033[0m\n";
        }
    });

    cmd.registerCommand("touch", [](std::istringstream& args) {
        const auto optional = CommandManager::readArguments(args, "You forgot to specify file name!");

        if (!optional) return;

        const std::string& fileName = *optional;
        std::error_code ec;

        if (fs::exists(fileName, ec)) {
            fs::last_write_time(fileName, fs::file_time_type::clock::now(), ec);
        } else {
            if (const std::ofstream file(fileName); !file) {
                std::cout << "\033[1;31m[ERROR] Failed to create file: " << fileName << "\033[0m\n";
                return;
            }
        }

        if (ec) {
            std::cout << "\033[1;31m[ERROR] touch failed: " << ec.message() << "\033[0m\n";
        } else {
            std::cout << "\033[1;32m[OK] File touched successfully!\033[0m\n";
        }
    });

    cmd.registerCommand("cat", [](std::istringstream& args) {
        const auto optional = CommandManager::readArguments(args, "You forgot to specify file name!");

        if (!optional) return;

        const std::string& fileName = *optional;

        std::ifstream file(fileName);

        if (!file) {
            std::cout << "\033[1;31m[ERROR] Could not open file: " << fileName << "\033[0m\n";
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::cout << line << "\n";
        }
    });

    cmd.registerCommand("grep", [](std::istringstream& args) {
        std::string pattern, fileName;
        args >> pattern >> fileName;

        if (pattern.empty() || fileName.empty()) {
            std::cout << "\033[1;31m[ERROR] Usage: grep <pattern> <file>\033[0m\n";
            return;
        }

        std::ifstream file(fileName);
        if (!file) {
            std::cout << "\033[1;31m[ERROR] Could not open file: " << fileName << "\033[0m\n";
            return;
        }

        std::string line;
        int lineNumber = 0;

        while (std::getline(file, line)) {
            lineNumber++;
            if (line.find(pattern) != std::string::npos) {
                std::cout << "\033[90m" << lineNumber << ": \033[0m";
                CommandManager::printHighlighted(line, pattern);
            }
        }
    });

    cmd.registerCommand("cp", [](std::istringstream& args) {
        std::string source, destination;
        args >> source >> destination;

        if (source.empty() || destination.empty()) {
            std::cout << "\033[1;31m[ERROR] You forgot to specify source or destination!\033[0m\n";
            return;
        }

        std::error_code ec;
        fs::copy(source, destination, fs::copy_options::overwrite_existing, ec);

        if (ec) {
            std::cout << "\033[1;31m[ERROR] Failed to copy: " << ec.message() << "\033[0m\n";
        } else {
            std::cout << "\033[1;32m[OK] Item copied successfully!\033[0m\n";
        }
    });

    cmd.registerCommand("find", [](std::istringstream& args) {
        const auto optional = CommandManager::readArguments(args, "You forgot to specify search query!");

        if (!optional) return;

        for (const auto& entry : fs::recursive_directory_iterator(fs::current_path(), fs::directory_options::skip_permission_denied)) {
            if (entry.path().filename().string().find(*optional) != std::string::npos) {
                std::cout << "\033[1;36m" << entry.path() << "\033[0m\n";
            }
        }
    });

    cmd.registerCommand("tree", [](std::istringstream&) {
        std::function<void(const fs::path&, const std::string&)> printTree =
        [&](const fs::path& dir, const std::string& prefix) {
            std::vector<fs::directory_entry> entries;

            for (std::error_code ec; const auto& entry : fs::directory_iterator(dir, ec)) {
                entries.push_back(entry);
            }

            for (size_t i = 0; i < entries.size(); ++i) {
                const bool isLast = i == entries.size() - 1;
                const auto& entry = entries[i];

                std::string nameColor = entry.is_directory() ? "\033[1;35m" : "\033[1;34m";

                std::cout << "\033[90m" << prefix << (isLast ? "└── " : "├── ") << "\033[0m"
                            << nameColor << entry.path().filename().string() << "\033[0m\n";

                if (entry.is_directory()) {
                    std::string newPrefix = prefix + (isLast ? "    " : "│   ");
                    printTree(entry.path(), newPrefix);
                }
            }
        };

        printTree(fs::current_path(), "");
    });

    cmd.registerCommand("wc", [](std::istringstream& args) {
        const auto optional = CommandManager::readArguments(args, "You forgot to specify file name!");

        if (!optional) return;

        const std::string& fileName = *optional;

        std::ifstream file(fileName);

        if (!file) {
            std::cout << "\033[1;31m[ERROR] Could not open file: " << fileName << "\033[0m\n";
            return;
        }

        std::string line;
        size_t lineCount = 0, wordCount = 0, charCount = 0;

        while (std::getline(file, line)) {
            lineCount++;
            charCount += line.size() + 1;

            std::istringstream words(line);
            std::string word;
            while (words >> word) {
                wordCount++;
            }
        }

        std::cout << "\033[1;36m" << lineCount << " lines, "
                    << wordCount << " words, "
                    << charCount << " chars\033[0m\n";
    });

    cmd.registerCommand("echo", [](std::istringstream& args) {
        std::string rest;
        std::getline(args, rest);

        auto trim = [](std::string& s) {
            const size_t start = s.find_first_not_of(" \t");
            const size_t end = s.find_last_not_of(" \t");
            s = start == std::string::npos ? "" : s.substr(start, end - start + 1);
        };

        bool append = false;
        size_t pos = rest.find(">>");

        if (pos != std::string::npos) {
            append = true;
        } else {
            pos = rest.find('>');
        }

        if (pos == std::string::npos) {
            trim(rest);
            std::cout << rest << "\n";
            return;
        }

        std::string text = rest.substr(0, pos);
        std::string fileName = rest.substr(pos + (append ? 2 : 1));
        trim(text);
        trim(fileName);

        if (fileName.empty()) {
            std::cout << "\033[1;31m[ERROR] Usage: echo <text> > <file>\033[0m\n";
            return;
        }

        std::ofstream file(fileName, append ? std::ios::app : std::ios::trunc);

        if (!file) {
            std::cout << "\033[1;31m[ERROR] Could not open file: " << fileName << "\033[0m\n";
            return;
        }

        file << text << "\n";
    });

    cmd.registerCommand("stat", [](std::istringstream& args) {
        const auto optional = CommandManager::readArguments(args, "You forgot to specify file name!");

        if (!optional) return;

        const std::string& path = *optional;

        if (!fs::exists(path)) {
            CommandManager::printError("File doesn't exist: " + path);
            return;
        }

        std::error_code ec;

        const auto status = fs::status(path, ec);

        std::cout << "\033[1;36m";
        std::cout << "Path:        " << fs::absolute(path).string() << "\n";
        std::cout << "Type:        " << CommandManager::fileTypeToString(status.type()) << "\n";

        if (fs::is_regular_file(status)) {
            std::cout << "Size:        " << fs::file_size(path, ec) << " bytes\n";
        }

        std::cout << "Permissions: " << CommandManager::permissionsToString(status.permissions()) << "\n";
        std::cout << "Modified:    " << std::format("{:%Y-%m-%d %H:%M:%S}", fs::last_write_time(path, ec)) << "\n";
        std::cout << "\033[0m";
    });

    cmd.registerCommand("pwd", [](std::istringstream&) {
        std::cout << "\033[1;36m" << fs::current_path() << "\033[0m\n";
    });

    cmd.registerCommand("secret", [](std::istringstream&) {
        const std::vector<std::string> lines = {
            "I", "\033[1;31m<3\033[0m", "you"
        };

        for (const auto& line : lines) {
            std::cout << line << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    cmd.registerCommand("help", [](std::istringstream&) {
        std::cout << "\n\033[1;33m=== Available commands ===\033[0m\n";
        std::cout << "  ls                       - List contents of current directory\n";
        std::cout << "  pwd                      - Print working directory path\n";
        std::cout << "  cd <path>                - Change current directory\n";
        std::cout << "  mkdir <dir_name>         - Create a new directory\n";
        std::cout << "  rm <name>                - Remove file or directory\n";
        std::cout << "  cp <src> <dest>          - Copy file or directory\n";
        std::cout << "  cat <file_name>          - Display file contents\n";
        std::cout << "  echo <text> [>/>> <file>] - Print text or redirect to a file\n";
        std::cout << "  grep <pattern> <file>    - Search text pattern in file\n";
        std::cout << "  find <name>              - Search files by name recursively\n";
        std::cout << "  tree                     - Show directories and files as a tree\n";
        std::cout << "  touch <file_name>        - Create an empty file or update its last write time\n";
        std::cout << "  wc <file_name>           - Count the number of lines, words and characters in a file\n";
        std::cout << "  secret                   - Secret command\n";
        std::cout << "  exit                     - Exit the application\n";
        std::cout << "\033[1;33m==========================\033[0m\n\n";
    });

    cmd.registerCommand("exit", [&running](std::istringstream&) {
        std::cout << "\033[1;32mBye!\033[0m\n";
        running = false;
    });

    std::string input;

    std::cout << "\033[1;33m";
    std::cout << "  __\n";
    std::cout << ">(o )___  Welcome to file system!\n";
    std::cout << " ( ._> /  Type \"help\" for more info\n";
    std::cout << "  `---'\n\n";
    std::cout << "\033[0m";

    while (running) {
        std::cout << "\033[1;32m" << fs::current_path() << "\033[1;33m $ \033[0m";
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;

        std::istringstream ss(input);
        std::string command;
        ss >> command;

        try {
            cmd.execute(command, ss);
        } catch (std::exception& e) {
            std::cout << "\033[1;31m[ERROR] " << e.what() << "\033[0m\n";
        }
    }

    return 0;
}