/**
 * BD Encoder - Bootstrap tool
 *
 * Converts human-readable .bd (with quotes) into real .bd (pure binary + notes).
 * This is a one-time dev tool for bootstrapping. The real format is pure binary.
 *
 * bd encode readable.bd output.bd
 */

#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

namespace bd {

class Encoder {
public:
    // Convert readable format (with quotes) to pure binary format (with notes)
    static std::string encode(const std::string& source) {
        std::istringstream stream(source);
        std::string line;
        std::ostringstream out;

        while (std::getline(stream, line)) {
            std::string trimmed = trim(line);

            // Empty lines
            if (trimmed.empty()) {
                out << "\n";
                continue;
            }

            // Comments pass through as-is
            if (trimmed[0] == '#') {
                out << trimmed << "\n";
                continue;
            }

            // Must start with 8-bit opcode
            if (trimmed.size() < 8) {
                out << trimmed << "\n";
                continue;
            }

            std::string opcode = trimmed.substr(0, 8);
            bool isBin = true;
            for (char c : opcode) { if (c != '0' && c != '1') { isBin = false; break; } }
            if (!isBin) {
                out << trimmed << "\n";
                continue;
            }

            out << opcode;

            // Parse and encode params
            std::string rest = (trimmed.size() > 8) ? trim(trimmed.substr(8)) : "";
            if (!rest.empty()) {
                auto params = parseParams(rest);
                for (size_t i = 0; i < params.size(); i++) {
                    out << " ";
                    // Encode each char as 8-bit binary
                    for (unsigned char ch : params[i]) {
                        for (int b = 7; b >= 0; b--) {
                            out << (((ch >> b) & 1) ? '1' : '0');
                        }
                    }
                    // Null terminator
                    out << " 00000000";
                }
            }

            out << "\n";
        }

        return out.str();
    }

private:
    static std::vector<std::string> parseParams(const std::string& raw) {
        std::vector<std::string> params;
        size_t i = 0;
        while (i < raw.size()) {
            if (raw[i] == ' ' || raw[i] == '\t') { i++; continue; }
            if (raw[i] == '"') {
                std::string str;
                i++;
                while (i < raw.size() && raw[i] != '"') {
                    if (raw[i] == '\\' && i + 1 < raw.size()) {
                        char next = raw[i + 1];
                        if (next == '"')  { str += '"'; i += 2; continue; }
                        if (next == 'n')  { str += '\n'; i += 2; continue; }
                        if (next == 't')  { str += '\t'; i += 2; continue; }
                        if (next == '\\') { str += '\\'; i += 2; continue; }
                    }
                    str += raw[i]; i++;
                }
                if (i < raw.size()) i++;
                params.push_back(str);
            } else {
                std::string val;
                while (i < raw.size() && raw[i] != ' ' && raw[i] != '\t' && raw[i] != '"') {
                    val += raw[i]; i++;
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
