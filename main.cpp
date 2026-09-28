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

namespace fs = std::filesystem;

namespace {
    class CommandManager {
    public:
        using CommandFunc = std::function<void(std::istringstream&)>;

        void registerCommand(const std::string& name, const CommandFunc& func) {
            commands[name] = func;
        }

        bool execute(const std::string& name, std::istringstream& input) {
            const auto it = commands.find(name);

            if (it == commands.end()) {
                std::cout << "\033[1;31m[ERROR] Unknown command: " << name << "\033[0m\n";
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
    private:
        std::unordered_map<std::string, CommandFunc> commands;
    };
}

int main() {
    CommandManager cmd;
    bool running = true;

    cmd.registerCommand("ls", [](std::istringstream&) {
        for (const auto& entry : fs::directory_iterator(fs::current_path())) {
            std::string nameColor = entry.is_directory() ? "\033[1;35m" : "\033[1;34m";
            std::string type = entry.is_directory() ? "[DIR] " : "[FILE] ";
            std::string reset = "\033[0m";
            std::string gray = "\033[90m";

            std::cout << nameColor << type << ": " << entry.path().filename().string() << reset;

            if (entry.is_regular_file()) {
                std::cout << gray << " (" << entry.file_size() << " bytes)" << reset;
            }

            std::cout << gray << " [" << std::format("{:%Y-%m-%d %H:%M}", entry.last_write_time()) << "]" << reset << "\n";
        }
    });

    cmd.registerCommand("mkdir", [](std::istringstream& args) {
        std::string folderName;
        args >> folderName;

        if (folderName.empty()) {
            std::cout << "\033[1;31m[ERROR] You forgot to specify directory name!\033[0m\n";
            return;
        }

        if (std::error_code ec; fs::create_directory(folderName, ec)) {
            std::cout << "\033[1;32m[OK] Directory successfully created!\033[0m\n";
        } else if (ec) {
            std::cout << "\033[1;31m[ERROR] Directory creation failed: " << ec.message() << "\033[0m\n";
        } else {
            std::cout << "\033[1;31m[ERROR] Directory already exists!\033[0m\n";
        }
    });

    cmd.registerCommand("cd", [](std::istringstream& args) {
        std::string path;
        args >> path;

        if (path.empty()) {
            std::cout << "\033[1;31m[ERROR] You forgot to specify directory name!\033[0m\n";
            return;
        }

        std::error_code ec;
        fs::current_path(path, ec);

        if (ec) {
            std::cout << "\033[1;31m[ERROR] Failed to change directory: " << ec.message() << "\033[0m\n";
        }
    });

    cmd.registerCommand("rm", [](std::istringstream& args) {
        std::string target;
        args >> target;

        if (target.empty()) {
            std::cout << "\033[1;31m[ERROR] You forgot to specify directory or file name!\033[0m\n";
            return;
        }

        if (std::error_code ec; fs::remove(target, ec)) {
            std::cout << "\033[1;32m[OK] Item successfully removed!\033[0m\n";
        } else if (ec) {
            std::cout << "\033[1;31m[ERROR] Failed to remove item: " << ec.message() << "\033[0m\n";
        } else {
            std::cout << "\033[1;31m[ERROR] Target item was not found!\033[0m\n";
        }
    });

    cmd.registerCommand("touch", [](std::istringstream& args) {
        std::string fileName;
        args >> fileName;

        if (fileName.empty()) {
            std::cout << "\033[1;31m[ERROR] You forgot to specify file name!\033[0m\n";
            return;
        }

        std::error_code ec;

        if (fs::exists(fileName, ec)) {
            fs::last_write_time(fileName, fs::file_time_type::clock::now(), ec);
        } else {
            std::ofstream file(fileName);

            if (!file) {
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
        std::string fileName;
        args >> fileName;

        if (fileName.empty()) {
            std::cout << "\033[1;31m[ERROR] You forgot to specify file name!\033[0m\n";
            return;
        }

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
        std::string search;
        args >> search;

        if (search.empty()) {
            std::cout << "\033[1;31m[ERROR] You forgot to specify search query!\033[0m\n";
            return;
        }

        for (const auto& entry : fs::recursive_directory_iterator(fs::current_path(), fs::directory_options::skip_permission_denied)) {
            if (entry.path().filename().string().find(search) != std::string::npos) {
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
        std::string fileName;
        args >> fileName;

        if (fileName.empty()) {
            std::cout << "\033[1;31m[ERROR] You forgot to specify file name!\033[0m\n";
            return;
        }

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