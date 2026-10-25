#pragma once

#include <string>

namespace keybinder {

// The caller expands variables first and routes the result as chat or command.
// This adds the visible test marker without changing a command's first token.
inline bool BuildSimulatedKillMessage(const std::string& expandedText,
                                      std::string& message, std::string& error) {
    message.clear();
    error.clear();
    const auto first = expandedText.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        error = "Bitte zuerst einen Killtext eintragen.";
        return false;
    }
    const auto last = expandedText.find_last_not_of(" \t\r\n");
    const std::string text = expandedText.substr(first, last - first + 1);
    if (text.find_first_of("\r\n") != std::string::npos || text.find('\0') != std::string::npos) {
        error = "Der Killtext muss aus einer einzelnen Chatzeile bestehen.";
        return false;
    }

    if (text.front() == '/') {
        const auto separator = text.find_first_of(" \t");
        const std::string command = text.substr(0, separator);
        if (command.size() == 1) {
            error = "Nach dem Schrägstrich fehlt ein Chatbefehl.";
            return false;
        }
        message = command + " [Testkill]";
        if (separator != std::string::npos) {
            const auto payload = text.find_first_not_of(" \t", separator);
            if (payload != std::string::npos) message += " " + text.substr(payload);
        }
    } else {
        message = "[Testkill] " + text;
    }

    constexpr std::size_t maximumLength = 144;
    if (message.size() > maximumLength) {
        if (text.front() == '/' && message.find(" [Testkill]") + 11 > maximumLength) {
            message.clear();
            error = "Der Chatbefehl ist für eine Testnachricht zu lang.";
            return false;
        }
        std::size_t length = maximumLength;
        // Do not split a multibyte UTF-8 character at the chat length limit.
        while (length > 0 && (static_cast<unsigned char>(message[length]) & 0xC0) == 0x80) --length;
        message.resize(length);
    }
    return true;
}

} // namespace keybinder
