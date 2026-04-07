/**
 * BD (Binary Decoder) - Main Entry Point
 *
 * The language that kills HTML, CSS, and JS.
 * One .bd file = One complete website.
 * Written in pure C++ from scratch.
 *
 * Usage:
 *   bd compile <file.bd> [output.html]
 *   bd serve <file.bd> [port]
 *   bd info
 */

#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include "compiler.h"
#include "server.h"
#include "opcodes.h"

void printHelp() {
    std::cout << R"(
   ____  ____
  | __ )|  _ \   Binary Decoder v0.1.0-beta
  |  _ \| | | |  The language that kills HTML, CSS, and JS.
  | |_) | |_| |  Written in pure C++ from scratch.
  |____/|____/

  USAGE:
    bd <command> [options]

  COMMANDS:
    compile <file.bd> [out.html]    Compile a .bd file to HTML
    serve <file.bd> [port]          Start dev server (default: 5000)
    info                            Show all opcodes
    version                         Show version

  EXAMPLES:
    bd compile landing.bd
    bd serve landing.bd 5000
    bd landing.bd
)" << std::endl;
}

void handleCompile(const std::string& input, const std::string& output) {
    if (!std::filesystem::exists(input)) {
        std::cerr << "Error: File not found: " << input << "\n";
        return;
    }

    std::cout << "[BD] Compiling " << input << "...\n";
    auto start = std::chrono::high_resolution_clock::now();

    bd::Compiler compiler;
    std::string html = compiler.compileFile(input);

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::ofstream out(output);
    out << html;
    out.close();

    std::cout << "[BD] Output: " << output << "\n";
    std::cout << "[BD] Compiled in " << ms << "ms\n";

    if (!compiler.errors.empty()) {
        std::cout << "[BD] Warnings:\n";
        for (auto& e : compiler.errors)
            std::cout << "  " << e << "\n";
    }
}

void handleServe(const std::string& input, int port) {
    if (!std::filesystem::exists(input)) {
        std::cerr << "Error: File not found: " << input << "\n";
        return;
    }

    bd::Server server(input, port);
    server.start();
}

void handleInfo() {
    std::cout << "\n  BD OPCODE REFERENCE\n\n";
    auto& table = bd::allOpcodes();

    // Group by category
    std::cout << "  === STRUCTURE (00xxxxxx) - replaces HTML ===\n";
    for (auto& [code, name] : table) {
        if (code.substr(0, 2) == "00")
            std::cout << "    " << code << "  " << name << "\n";
    }

    std::cout << "\n  === STYLE (01xxxxxx) - replaces CSS ===\n";
    for (auto& [code, name] : table) {
        if (code.substr(0, 2) == "01")
            std::cout << "    " << code << "  " << name << "\n";
    }

    std::cout << "\n  === LOGIC (10xxxxxx) - replaces JavaScript ===\n";
    for (auto& [code, name] : table) {
        if (code.substr(0, 2) == "10")
            std::cout << "    " << code << "  " << name << "\n";
    }

    std::cout << "\n  === META (11xxxxxx) - config & advanced ===\n";
    for (auto& [code, name] : table) {
        if (code.substr(0, 2) == "11")
            std::cout << "    " << code << "  " << name << "\n";
    }

    std::cout << "\n  Total opcodes: " << table.size() << "\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 0;
    }

    std::string command = argv[1];

    if (command == "--help" || command == "-h") {
        printHelp();
        return 0;
    }

    if (command == "version" || command == "-v" || command == "--version") {
        std::cout << "BD (Binary Decoder) v0.1.0-beta\n";
        return 0;
    }

    if (command == "info" || command == "opcodes") {
        handleInfo();
        return 0;
    }

    if (command == "compile" || command == "build") {
        if (argc < 3) {
            std::cerr << "Error: No input file specified\n";
            return 1;
        }
        std::string input = argv[2];
        std::string output;
        if (argc >= 4) {
            output = argv[3];
        } else {
            output = input;
            if (output.size() > 3 && output.substr(output.size() - 3) == ".bd") {
                output = output.substr(0, output.size() - 3) + ".html";
            } else {
                output += ".html";
            }
        }
        handleCompile(input, output);
        return 0;
    }

    if (command == "serve" || command == "dev") {
        if (argc < 3) {
            std::cerr << "Error: No input file specified\n";
            return 1;
        }
        std::string input = argv[2];
        int port = (argc >= 4) ? std::stoi(argv[3]) : 5000;
        handleServe(input, port);
        return 0;
    }

    // If argument ends in .bd, assume serve
    if (command.size() > 3 && command.substr(command.size() - 3) == ".bd") {
        int port = (argc >= 3) ? std::stoi(argv[2]) : 5000;
        handleServe(command, port);
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    printHelp();
    return 1;
}
