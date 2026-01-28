#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

namespace {

std::string trim(const std::string& input)
{
    const auto begin = std::find_if_not(input.begin(), input.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto end = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

} // namespace

int main()
{
    std::cout << "Terminal App Example" << '\n';
    std::cout << "Type a message and press Enter." << '\n';
    std::cout << "Commands: help, clear, exit" << '\n' << '\n';

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            std::cout << "\nInput closed. Exiting." << '\n';
            break;
        }

        const std::string trimmed = trim(line);
        const std::string command = toLower(trimmed);

        if (command.empty()) {
            continue;
        }
        if (command == "exit" || command == "quit") {
            std::cout << "Goodbye." << '\n';
            break;
        }
        if (command == "help") {
            std::cout << "Commands:" << '\n';
            std::cout << "  help  - show this message" << '\n';
            std::cout << "  clear - clear the console (prints spacer)" << '\n';
            std::cout << "  exit  - quit the app" << '\n';
            continue;
        }
        if (command == "clear") {
            std::cout << std::string(40, '\n');
            continue;
        }

        std::cout << "Echo: " << trimmed << '\n';
    }

    return 0;
}
