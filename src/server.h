/**
 * BD Dev Server - Pure C++ HTTP server using Winsock
 *
 * Zero dependencies. Built from scratch.
 * Serves compiled .bd files with live reload.
 */

#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <string>
#include <thread>
#include <atomic>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <mutex>
#include "compiler.h"

namespace bd {

class Server {
public:
    std::string bdFile;
    int port;
    Compiler compiler;
    std::string cachedHTML;
    std::atomic<bool> running{false};
    std::atomic<bool> needsReload{false};
    std::mutex htmlMutex;

    Server(const std::string& file, int p = 5000)
        : bdFile(file), port(p) {}

    void start() {
        // Compile initially
        doCompile();

#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "[BD] WSAStartup failed\n";
            return;
        }
#endif

        SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSock == INVALID_SOCKET) {
            std::cerr << "[BD] Socket creation failed\n";
            return;
        }

        // Allow port reuse
        int opt = 1;
        setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            std::cerr << "[BD] Bind failed on port " << port << "\n";
            closesocket(serverSock);
            return;
        }

        if (listen(serverSock, 10) == SOCKET_ERROR) {
            std::cerr << "[BD] Listen failed\n";
            closesocket(serverSock);
            return;
        }

        running = true;

        printBanner();

        // Start file watcher thread
        std::thread watcherThread(&Server::watchFile, this);
        watcherThread.detach();

        // Accept loop
        while (running) {
            SOCKET clientSock = accept(serverSock, nullptr, nullptr);
            if (clientSock == INVALID_SOCKET) continue;

            // Handle in thread
            std::thread(&Server::handleClient, this, clientSock).detach();
        }

        closesocket(serverSock);
#ifdef _WIN32
        WSACleanup();
#endif
    }

private:
    void printBanner() {
        std::cout << "\n";
        std::cout << "  ======================================\n";
        std::cout << "   ____  ____\n";
        std::cout << "  | __ )|  _ \\   Binary Decoder\n";
        std::cout << "  |  _ \\| | | |  Dev Server v0.1.0\n";
        std::cout << "  | |_) | |_| |  C++ from scratch\n";
        std::cout << "  |____/|____/\n";
        std::cout << "\n";
        std::cout << "  -> http://localhost:" << port << "\n";
        std::cout << "  -> Watching for changes...\n";
        std::cout << "  -> Press Ctrl+C to stop\n";
        std::cout << "  ======================================\n\n";
    }

    void doCompile() {
        std::cout << "[BD] Compiling " << bdFile << "...\n";
        auto start = std::chrono::high_resolution_clock::now();

        std::string html = compiler.compileFile(bdFile);

        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[BD] Compiled in " << ms << "ms\n";

        if (!compiler.errors.empty()) {
            std::cout << "[BD] Warnings:\n";
            for (auto& e : compiler.errors)
                std::cout << "  " << e << "\n";
        }

        std::lock_guard<std::mutex> lock(htmlMutex);
        cachedHTML = injectLiveReload(html);
    }

    std::string injectLiveReload(const std::string& html) {
        std::string script = R"(
<script>
(function(){
  var lastCheck=Date.now();
  setInterval(function(){
    fetch('/__bd_check').then(function(r){return r.text()}).then(function(t){
      if(parseInt(t)>lastCheck){lastCheck=Date.now();window.location.reload();}
    }).catch(function(){});
  },500);
})();
</script>)";

        size_t pos = html.find("</body>");
        if (pos != std::string::npos) {
            return html.substr(0, pos) + script + "\n" + html.substr(pos);
        }
        return html + script;
    }

    void watchFile(void) {
        namespace fs = std::filesystem;
        auto lastWrite = fs::last_write_time(bdFile);

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            try {
                auto currentWrite = fs::last_write_time(bdFile);
                if (currentWrite != lastWrite) {
                    lastWrite = currentWrite;
                    std::cout << "[BD] Change detected, recompiling...\n";
                    doCompile();
                    reloadTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                }
            } catch (...) {}
        }
    }

    long long reloadTimestamp = 0;

    void handleClient(SOCKET sock) {
        // Read request
        char buf[4096] = {0};
        int received = recv(sock, buf, sizeof(buf) - 1, 0);
        if (received <= 0) {
            closesocket(sock);
            return;
        }

        std::string request(buf, received);

        // Parse request line
        std::string method, path;
        std::istringstream reqStream(request);
        reqStream >> method >> path;

        std::string response;

        if (path == "/" || path == "/index.html") {
            std::lock_guard<std::mutex> lock(htmlMutex);
            response = httpResponse(200, "text/html", cachedHTML);
        }
        else if (path == "/__bd_check") {
            response = httpResponse(200, "text/plain", std::to_string(reloadTimestamp));
        }
        else {
            // Try serve static file
            std::string filePath = std::filesystem::path(bdFile).parent_path().string() + path;
            std::ifstream file(filePath, std::ios::binary);
            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
                std::string mime = guessMime(path);
                response = httpResponse(200, mime, content);
            } else {
                response = httpResponse(404, "text/plain", "404 Not Found");
            }
        }

        send(sock, response.c_str(), (int)response.size(), 0);
        closesocket(sock);
    }

    std::string httpResponse(int status, const std::string& contentType, const std::string& body) {
        std::string statusText = (status == 200) ? "OK" : "Not Found";
        std::ostringstream out;
        out << "HTTP/1.1 " << status << " " << statusText << "\r\n";
        out << "Content-Type: " << contentType << "; charset=utf-8\r\n";
        out << "Content-Length: " << body.size() << "\r\n";
        out << "Cache-Control: no-cache\r\n";
        out << "Connection: close\r\n";
        out << "\r\n";
        out << body;
        return out.str();
    }

    std::string guessMime(const std::string& path) {
        if (path.ends_with(".css")) return "text/css";
        if (path.ends_with(".js")) return "application/javascript";
        if (path.ends_with(".json")) return "application/json";
        if (path.ends_with(".png")) return "image/png";
        if (path.ends_with(".jpg") || path.ends_with(".jpeg")) return "image/jpeg";
        if (path.ends_with(".svg")) return "image/svg+xml";
        if (path.ends_with(".gif")) return "image/gif";
        if (path.ends_with(".ico")) return "image/x-icon";
        if (path.ends_with(".woff2")) return "font/woff2";
        if (path.ends_with(".woff")) return "font/woff";
        return "application/octet-stream";
    }
};

} // namespace bd
