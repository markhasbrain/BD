/**
 * BD Lexer - Tokenizes .bd source files
 *
 * Supports TWO formats:
 *   1. Text mode: opcodes with quoted strings (human-readable)
 *      11000001 "Hello World"
 *
 *   2. Binary mode: pure binary, strings as 8-bit ASCII terminated by 00000000
 *      11000001 01001000 01100101 01101100 01101100 01101111 00000000
 *
 * Comments start with # in both modes.
 * The lexer auto-detects which format is used per-line.
 */

#pragma once
#include <string>
#include <vector>
#include <sstream>

namespace bd {

struct Token {
    std::string opcode;              // 8-bit binary string
    std::vector<std::string> params; // parameter values
    int line;                        // source line number
};

class Lexer {
public:
    std::vector<Token> tokens;
    std::vector<std::string> errors;

    void tokenize(const std::string& source) {
        tokens.clear();
        errors.clear();

        std::istringstream stream(source);
        std::string rawLine;
        int lineNum = 0;

        while (std::getline(stream, rawLine)) {
            lineNum++;
            std::string line = trim(rawLine);

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;

            // Need at least 8 chars for opcode
            if (line.size() < 8) continue;

            std::string opcode = line.substr(0, 8);

            // Validate it's binary
            if (!isBinary(opcode)) continue;

            // Parse params - auto-detect mode
            std::string rest = (line.size() > 8) ? trim(line.substr(8)) : "";
            std::vector<std::string> params;

            if (rest.empty()) {
                // No params
            } else if (rest[0] == '"' || (!isBinary8(rest) && rest[0] != '0' && rest[0] != '1')) {
                // Text mode: has quotes or non-binary content
                params = parseTextParams(rest);
            } else {
                // Check if the rest is all binary octets (pure binary mode)
                // or mixed with bare values like "1" or "center"
                if (isPureBinaryLine(rest)) {
                    params = parseBinaryParams(rest);
                } else {
                    params = parseTextParams(rest);
                }
            }

            tokens.push_back({opcode, params, lineNum});
        }
    }

private:
    // Check if string is exactly 8 binary digits
    static bool isBinary8(const std::string& s) {
        if (s.size() < 8) return false;
        for (int i = 0; i < 8; i++) {
            if (s[i] != '0' && s[i] != '1') return false;
        }
        return true;
    }

    static bool isBinary(const std::string& s) {
        if (s.size() != 8) return false;
        for (char c : s) {
            if (c != '0' && c != '1') return false;
        }
        return true;
    }

    // Check if the entire parameter section is pure binary octets
    static bool isPureBinaryLine(const std::string& rest) {
        size_t i = 0;
        bool foundOctet = false;
        while (i < rest.size()) {
            if (rest[i] == ' ' || rest[i] == '\t') { i++; continue; }
            // Must be 8 binary digits
            if (i + 8 > rest.size()) return false;
            for (size_t j = i; j < i + 8; j++) {
                if (rest[j] != '0' && rest[j] != '1') return false;
            }
            foundOctet = true;
            i += 8;
            // After 8 digits, must be space or end
            if (i < rest.size() && rest[i] != ' ' && rest[i] != '\t') return false;
        }
        return foundOctet;
    }

    // Parse binary params: sequences of 8-bit bytes, 00000000 = null terminator (string separator)
    std::vector<std::string> parseBinaryParams(const std::string& rest) {
        std::vector<std::string> params;
        std::string current;
        size_t i = 0;

        while (i < rest.size()) {
            if (rest[i] == ' ' || rest[i] == '\t') { i++; continue; }

            if (i + 8 > rest.size()) break;
            std::string octet = rest.substr(i, 8);
            i += 8;

            // Convert binary to char value
            int val = 0;
            for (int b = 0; b < 8; b++) {
                val = (val << 1) | (octet[b] - '0');
            }

            if (val == 0) {
                // Null terminator - end of this string param
                params.push_back(current);
                current.clear();
            } else {
                current += (char)val;
            }
        }

        // Remaining content (no trailing null) is still a param
        if (!current.empty()) {
            params.push_back(current);
        }

        return params;
    }

    // Parse text params: quoted strings and bare values (original format)
    std::vector<std::string> parseTextParams(const std::string& raw) {
        std::vector<std::string> params;
        if (raw.empty()) return params;

        size_t i = 0;
        while (i < raw.size()) {
            char ch = raw[i];

            if (ch == ' ' || ch == '\t') {
                i++;
                continue;
            }

            if (ch == '"') {
                // Quoted string
                std::string str;
                i++; // skip opening quote
                while (i < raw.size() && raw[i] != '"') {
                    if (raw[i] == '\\' && i + 1 < raw.size()) {
                        char next = raw[i + 1];
                        if (next == '"')  { str += '"'; i += 2; continue; }
                        if (next == 'n')  { str += '\n'; i += 2; continue; }
                        if (next == 't')  { str += '\t'; i += 2; continue; }
                        if (next == '\\') { str += '\\'; i += 2; continue; }
                    }
                    str += raw[i];
                    i++;
                }
                if (i < raw.size()) i++; // skip closing quote
                params.push_back(str);
            } else {
                // Bare value
                std::string val;
                while (i < raw.size() && raw[i] != ' ' && raw[i] != '\t' && raw[i] != '"') {
                    val += raw[i];
                    i++;
                }
                params.push_back(val);
            }
        }

        return params;
    }

    static std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }
};

} // namespace bd
