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
                std::cout << color::yellow << pattern << color::reset;
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
        const auto path = CommandManager::readArguments(args, "You forgot to specify directory name!");

        if (!path) return;

        std::error_code ec;
        fs::current_path(*path, ec);

        if (ec) {
            CommandManager::printError("Failed to change directory: " + ec.message());
        }
    });

    cmd.registerCommand("rm", [](std::istringstream& args) {
        const auto target = CommandManager::readArguments(args, "You forgot to specify directory or file name!");

        if (!target) return;

        if (std::error_code ec; fs::remove(*target, ec)) {
            CommandManager::printSuccess("Item successfully removed!");
        } else if (ec) {
            CommandManager::printError("Failed to remove item: " + ec.message());
        } else {
            CommandManager::printError("Target item was not found!");
        }
    });

    cmd.registerCommand("touch", [](std::istringstream& args) {
        const auto result = CommandManager::readArguments(args, "You forgot to specify file name!");

        if (!result) return;

        const std::string& fileName = *result;
        std::error_code ec;

        if (fs::exists(fileName, ec)) {
            fs::last_write_time(fileName, fs::file_time_type::clock::now(), ec);
        } else if (const std::ofstream file(fileName); !file) {
            CommandManager::printError("Failed to create file: " + fileName);
            return;
        }

        if (ec) {
            CommandManager::printError("touch failed: " + ec.message());
        } else {
            CommandManager::printSuccess("File touched successfully!");
        }
    });

    cmd.registerCommand("cat", [](std::istringstream& args) {
        const auto result = CommandManager::readArguments(args, "You forgot to specify file name!");

        if (!result) return;

        const std::string& fileName = *result;

        std::ifstream file(fileName);

        if (!file) {
            CommandManager::printError("Could not open file: " + fileName);
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::cout << line << "\n";
        }
    });

    cmd.registerCommand("grep", [](std::istringstream& args) {
        const auto pattern = CommandManager::readArguments(args, "Usage: grep <pattern> <file>");
        if (!pattern) return;

        const auto fileName = CommandManager::readArguments(args, "Usage: grep <pattern> <file>");
        if (!fileName) return;

        std::ifstream file(*fileName);
        if (!file) {
            CommandManager::printError("Could not open file: " + *fileName);
            return;
        }

        std::string line;
        int lineNumber = 0;

        while (std::getline(file, line)) {
            lineNumber++;
            if (line.find(*pattern) != std::string::npos) {
                std::cout << "\033[90m" << lineNumber << ": \033[0m";
                CommandManager::printHighlighted(line, *pattern);
            }
        }
    });

    cmd.registerCommand("cp", [](std::istringstream& args) {
        const auto source = CommandManager::readArguments(args, "Usage: cp <source> <destination>");
        if (!source) return;

        const auto destination = CommandManager::readArguments(args, "Usage: cp <source> <destination>");
        if (!destination) return;

        std::error_code ec;
        fs::copy(*source, *destination, fs::copy_options::overwrite_existing, ec);

        if (ec) {
            CommandManager::printError("Failed to copy: " + ec.message());
        } else {
            CommandManager::printSuccess("Item copied successfully!");
        }
    });

    cmd.registerCommand("find", [](std::istringstream& args) {
        const auto search = CommandManager::readArguments(args, "You forgot to specify search query!");

        if (!search) return;

        for (const auto& entry : fs::recursive_directory_iterator(fs::current_path(), fs::directory_options::skip_permission_denied)) {
            if (entry.path().filename().string().find(*search) != std::string::npos) {
                std::cout << color::cyan << entry.path().string() << color::reset << "\n";
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

                const std::string_view nameColor = entry.is_directory() ? color::magenta : color::blue;

                std::cout << color::gray << prefix << (isLast ? "└── " : "├── ") << color::reset
                            << nameColor << entry.path().filename().string() << color::reset << "\n";

                if (entry.is_directory()) {
                    std::string newPrefix = prefix + (isLast ? "    " : "│   ");
                    printTree(entry.path(), newPrefix);
                }
            }
        };

        printTree(fs::current_path(), "");
    });

    cmd.registerCommand("wc", [](std::istringstream& args) {
        const auto result = CommandManager::readArguments(args, "You forgot to specify file name!");

        if (!result) return;

        const std::string& fileName = *result;

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

        CommandManager::printInfo(std::format("{} lines, {} words, {} chars", lineCount, wordCount, charCount));
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
            CommandManager::printError("Usage: echo <text> > <file>");
            return;
        }

        std::ofstream file(fileName, append ? std::ios::app : std::ios::trunc);

        if (!file) {
            CommandManager::printError("Could not open file: " + fileName);
            return;
        }

        file << text << "\n";
    });

    cmd.registerCommand("stat", [](std::istringstream& args) {
        const auto result = CommandManager::readArguments(args, "You forgot to specify file name!");

        if (!result) return;

        const std::string& path = *result;
        std::error_code ec;
        const auto status = fs::status(path, ec);

        if (ec || !fs::exists(status)) {
            CommandManager::printError("File doesn't exist: " + path);
            return;
        }

        std::cout << color::cyan;
        std::cout << "Path:        " << fs::absolute(path).string() << "\n";
        std::cout << "Type:        " << CommandManager::fileTypeToString(status.type()) << "\n";

        if (fs::is_regular_file(status)) {
            std::cout << "Size:        " << fs::file_size(path, ec) << " bytes\n";
        }

        std::cout << "Permissions: " << CommandManager::permissionsToString(status.permissions()) << "\n";
        std::cout << "Modified:    " << std::format("{:%Y-%m-%d %H:%M:%S}", fs::last_write_time(path, ec)) << "\n";
        std::cout << color::reset;
    });

    cmd.registerCommand("pwd", [](std::istringstream&) {
        CommandManager::printInfo(fs::current_path().string());
    });

    cmd.registerCommand("secret", [](std::istringstream&) {
        const std::vector<std::string> lines = {
            "I", std::string(color::red) + "<3" + std::string(color::reset), "you"
        };

        for (const auto& line : lines) {
            std::cout << line << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    cmd.registerCommand("help", [](std::istringstream&) {
        const std::vector<std::pair<std::string_view, std::string_view>> commands = {
            {"ls", "List contents of current directory"},
            {"pwd", "Print working directory path"},
            {"cd <path>", "Change current directory"},
            {"mkdir <dir_name>", "Create a new directory"},
            {"rm <name>", "Remove file or directory"},
            {"cp <src> <dest>", "Copy file or directory"},
            {"cat <file_name>", "Display file contents"},
            {"echo <text> [>/>> <file>]", "Print text or redirect to a file"},
            {"grep <pattern> <file>", "Search text pattern in file"},
            {"find <name>", "Search files by name recursively"},
            {"tree", "Show directories and files as a tree"},
            {"touch <file_name>", "Create an empty file or update its last write time"},
            {"wc <file_name>", "Count lines, words and characters in a file"},
            {"stat <path>", "Show type, size, permissions and last modification time"},
            {"secret", "Secret command"},
            {"exit", "Exit the application"},
        };

        std::cout << "\n" << color::yellow << "=== Available commands ===" << color::reset << "\n";
        for (const auto& [usage, description] : commands) {
            std::cout << std::format("  {:<27}- {}\n", usage, description);
        }
        std::cout << color::yellow << "==========================" << color::reset << "\n\n";
    });

    cmd.registerCommand("exit", [&running](std::istringstream&) {
        CommandManager::printSuccess("Bye!");
        running = false;
    });

    std::string input;

    std::cout << color::yellow;
    std::cout << "  __\n";
    std::cout << ">(o )___  Welcome to file system!\n";
    std::cout << " ( ._> /  Type \"help\" for more info\n";
    std::cout << "  `---'\n\n";
    std::cout << color::reset;

    while (running) {
        std::cout << color::green << fs::current_path().string() << color::yellow << " $ " << color::reset;
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;

        std::istringstream ss(input);
        std::string command;
        ss >> command;

        try {
            cmd.execute(command, ss);
        } catch (std::exception& e) {
            CommandManager::printError(e.what());
        }
    }

    return 0;
}