/**
 * BD Lexer - Tokenizes .bd source files
 *
 * BD format:
 *   - Every line is 8-bit binary octets separated by spaces
 *   - First octet on a line is the opcode
 *   - Remaining octets are parameters (strings as ASCII bytes, 00000000 = separator)
 *   - Lines starting with # are comments (notes)
 *   - Empty lines are ignored
 *
 * That's it. Pure binary + notes.
 */

#pragma once
#include <string>
#include <vector>
#include <sstream>

namespace bd {

struct Token {
    std::string opcode;              // 8-bit binary string
    std::vector<std::string> params; // decoded parameter strings
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

            // Validate opcode is binary
            if (!isBinary8(opcode)) continue;

            // Rest of line: binary octets = parameters
            std::string rest = (line.size() > 8) ? trim(line.substr(8)) : "";

            std::vector<std::string> params;
            if (!rest.empty()) {
                params = decodeBinaryParams(rest);
            }

            tokens.push_back({opcode, params, lineNum});
        }
    }

private:
    static bool isBinary8(const std::string& s) {
        if (s.size() < 8) return false;
        for (int i = 0; i < 8; i++) {
            if (s[i] != '0' && s[i] != '1') return false;
        }
        return true;
    }

    // Decode binary octets into string parameters
    // Each 8-bit group = one ASCII char, 00000000 = param separator
    static std::vector<std::string> decodeBinaryParams(const std::string& rest) {
        std::vector<std::string> params;

        // Extract all 8-bit octets
        std::vector<int> bytes;
        size_t i = 0;
        while (i < rest.size()) {
            if (rest[i] == ' ' || rest[i] == '\t') { i++; continue; }

            // Must have 8 binary digits
            if (i + 8 > rest.size()) break;

            bool valid = true;
            int val = 0;
            for (int b = 0; b < 8; b++) {
                char c = rest[i + b];
                if (c != '0' && c != '1') { valid = false; break; }
                val = (val << 1) | (c - '0');
            }

            if (!valid) break;
            bytes.push_back(val);
            i += 8;
        }

        // Split by null bytes (0) into string params
        std::string current;
        for (int byte : bytes) {
            if (byte == 0) {
                params.push_back(current);
                current.clear();
            } else {
                current += (char)byte;
            }
        }
        // Trailing param without null terminator
        if (!current.empty()) {
            params.push_back(current);
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
