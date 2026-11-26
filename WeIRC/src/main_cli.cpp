// main_cli.cpp - البرنامج النصي (terminal version, no FLTK needed)
//
//   ./weirc-cli [-n NICK] [-c SERVER]
//
// Type /commands or chat text, one line at a time. Ctrl+D or /quit exits.
// Lines from the network arrive on a background thread, so printing is
// protected by a mutex.
#include <csignal>
#include <iostream>
#include <mutex>
#include <string>
#include "app.h"
#include "util.h"
int main(int argc, char** argv) {
std::signal(SIGPIPE, SIG_IGN);  
std::string nick, auto_connect;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-n" || arg == "--nick") && i + 1 < argc) {
            nick = argv[++i];
        } else if ((arg == "-c" || arg == "--connect") && i + 1 < argc) {
            auto_connect = argv[++i];
        } else {
            std::cout << "Usage: weirc-cli [-n NICK] [-c SERVER_NAME]\n";
            return arg == "-h" || arg == "--help" ? 0 : 1;
        }
    }
App app;
std::mutex out_mutex;
app.print = [&](const std::string& text) {
std::lock_guard<std::mutex> lock(out_mutex);
std::cout << '[' << timestamp() << "] " << text << std::endl;
    };
    if (!nick.empty()) app.client.set_nick(nick);
    app.load();
    if (!auto_connect.empty()) app.handle_input("/connect " + auto_connect);
    std::string line;
    while (std::getline(std::cin, line))
        if (!app.handle_input(line)) break;
   app.shutdown();
   return 0;
}
