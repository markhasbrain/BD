/**
 * BD Lexer - Tokenizes .bd source files
 *
 * Reads raw .bd source and produces a stream of tokens.
 * Each line: 8-bit binary opcode followed by parameters.
 * Comments start with #. Strings use double quotes.
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
            bool valid = true;
            for (char c : opcode) {
                if (c != '0' && c != '1') { valid = false; break; }
            }
            if (!valid) continue;

            // Parse params
            std::string rest = (line.size() > 8) ? trim(line.substr(8)) : "";
            std::vector<std::string> params = parseParams(rest);

            tokens.push_back({opcode, params, lineNum});
        }
    }

private:
    std::vector<std::string> parseParams(const std::string& raw) {
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
