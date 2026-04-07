/**
 * BD Encoder - Converts human-readable .bd to pure binary .bd
 *
 * Uses a variable-length character encoding optimized for web content.
 * Most common characters (e, t, a, o, i, n, s, r, h, l) get shorter codes.
 * Less common characters get longer codes.
 *
 * Format:
 *   - Opcodes stay as 8-bit binary
 *   - Strings encoded as variable-length binary, terminated by 0000 (4-bit null)
 *   - Multiple params separated by 0000
 *   - Line ends with newline
 *   - Comments encoded with special prefix 1111 + encoded text + 0000
 *
 * bd encode input.bd > output.bd    (readable -> pure binary)
 * bd decode input.bd > output.bd    (pure binary -> readable)
 */

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <fstream>

namespace bd {

// Fixed 8-bit ASCII encoding for strings.
// Each character = 8 binary bits. Null terminator = 00000000.
// Clean, reliable, pure binary.

// Encode a string to 8-bit binary (each char = 8 bits)
inline std::string encodeString(const std::string& str) {
    std::string result;
    for (unsigned char ch : str) {
        for (int b = 7; b >= 0; b--) {
            result += ((ch >> b) & 1) ? '1' : '0';
        }
    }
    return result;
}

// Decode 8-bit binary back to string (reads until 00000000 null terminator)
inline std::string decodeString(const std::string& bits, size_t& pos) {
    std::string result;

    while (pos + 8 <= bits.size()) {
        // Read 8 bits
        int val = 0;
        for (int b = 0; b < 8; b++) {
            val = (val << 1) | (bits[pos + b] - '0');
        }
        pos += 8;

        if (val == 0) return result; // null terminator
        result += (char)val;
    }

    return result;
}

class Encoder {
private:
    static std::string escapeStr(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\t') out += "\\t";
            else out += c;
        }
        return out;
    }

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

public:
    // Encode readable .bd to pure binary .bd
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

            // Comments: encode with prefix 11111111
            if (trimmed[0] == '#') {
                std::string comment = trimmed.substr(1);
                while (!comment.empty() && comment[0] == ' ') comment = comment.substr(1);
                out << "11111111 " << encodeString(comment) << "0000\n";
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
                    out << " " << encodeString(params[i]) << "0000";
                }
            }

            out << "\n";
        }

        return out.str();
    }

    // Decode pure binary .bd back to readable .bd
    static std::string decode(const std::string& source) {
        std::istringstream stream(source);
        std::string line;
        std::ostringstream out;

        while (std::getline(stream, line)) {
            std::string trimmed = trim(line);

            if (trimmed.empty()) {
                out << "\n";
                continue;
            }

            // Comment line
            if (trimmed.size() >= 8 && trimmed.substr(0, 8) == "11111111") {
                std::string rest = trim(trimmed.substr(8));
                size_t pos = 0;
                std::string comment = decodeString(rest, pos);
                out << "# " << comment << "\n";
                continue;
            }

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

            // Decode params
            std::string rest = (trimmed.size() > 8) ? trim(trimmed.substr(8)) : "";
            if (!rest.empty()) {
                // Strip spaces for decoding
                std::string bits;
                for (char c : rest) {
                    if (c == '0' || c == '1') bits += c;
                }

                size_t pos = 0;
                while (pos + 8 <= bits.size()) {
                    std::string param = decodeString(bits, pos);
                    out << " \"" << escapeStr(param) << "\"";
                }
            }

            out << "\n";
        }

        return out.str();
    }

};

} // namespace bd
