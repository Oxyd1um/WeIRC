#pragma once
#include <functional>
#include <string>
#include "irc_client.h"
#include "irc_server.h"
#include "servers.h"
class App {
public:
    IrcClient client;
    ServerList servers;
    LocalServer local_server;
    std::function<void(const std::string&)> print;
    std::function<void()> on_servers_changed;
    App();
    ~App() { shutdown(); }
    void load();                                // Why th fk are you here twin (:
    bool handle_input(const std::string& line); 
    void shutdown();                            
private:
    void command(const std::string& line);
    void show(const std::string& text) { if (print) print(text); }
    void show_help();
    void list_servers();
    void show_users();
    void show_my_channels();
    void connect_to(const std::string& name);
    void create_local_server(int port);
};
