#include "app.h"
#include <sstream>
#include "util.h"
App::App() {
    client.on_text = [this](const std::string& text) { show(text); };
}
void App::load() {
    servers.load();
    if (on_servers_changed) on_servers_changed();
    show("WeIRC started. Type /help for commands.");
}
void App::shutdown() {
client.on_text = nullptr;
client.on_status = nullptr;
client.on_users = nullptr;
client.on_channels = nullptr;
print = nullptr;
on_servers_changed = nullptr;
client.stop();
local_server.stop();
}
bool App::handle_input(const std::string& raw) {
    std::string line = trim(raw);
    if (line.empty()) return true;

    if (line[0] != '/') {
        client.say(line);
        return true;
    }
    if (line == "/quit" || line == "/exit" || line == "exit") {
        if (client.connected()) client.quit();
        return false;
    }
    command(line);
    return true;
}
void App::command(const std::string& line) {
std::istringstream in(line);
std::string cmd;
in >> cmd;
    if (cmd == "/help" || cmd == "help") {
        show_help();

    } else if (cmd == "/add" || cmd == "add") {
        std::string name, host, error;
        int port = 0;
        if (!(in >> name >> host >> port)) {
            show("Usage Bro: /add NAME HOST PORT");
        } else if (!servers.add(name, host, port, error)) {
            show(error);
        } else {
            show("saved serv: " + name);
            if (on_servers_changed) on_servers_changed();
        }

    } else if (cmd == "/servers") {
        list_servers();

    } else if (cmd == "/connect") {
        std::string name;
        in >> name;
        if (name.empty())
            show("Usage: /connect NAME   (see /servers)");
        else
            connect_to(name);
    } else if (cmd == "/disconnect") {
        if (client.active()) {
            client.disconnect();
            show("Disconnected.");
        } else
            show("Not connected.");

    } else if (cmd == "/nick") {
        std::string name;
        in >> name;
        if (name.empty())
            show("Usage: /nick NAME");
        else
            client.set_nick(name);
    } else if (cmd == "/join") {
        std::string channel;
        in >> channel;
        if (channel.empty())
            show("Usage: /join #channel");
        else if (!client.connected())
            show("Not connected. Use /connect NAME first.");
        else
            client.join(channel);
    } else if (cmd == "/part") {
        client.part();
    } else if (cmd == "/switch") {
        std::string channel;
        in >> channel;
        if (channel.empty())
            show("Usage: /switch #channel   (see /list-ch)");
        else
            client.switch_to(channel);
    } else if (cmd == "/users" || cmd == "/list-us") {
        show_users();

    } else if (cmd == "/list-ch") {
        show_my_channels();
    } else if (cmd == "/list") {
        std::string filter;
        in >> filter;
        client.request_list(filter);
    } else if (cmd == "/msg") {
        std::string target, text;
        in >> target;
        std::getline(in, text);
        text = trim(text);
        if (target.empty() || text.empty())
            show("Usage: /msg NICK text");
        else
            client.say_to(target, text);
    } else if (cmd == "/create") {
        int port = 6667;
        in >> port;
        create_local_server(port);
    } else {
        show("Unknown Command what the hell did u type ?: " + cmd + ". Use /help.");
    }
}

void App::show_help() {
    show("/add NAME HOST PORT   save a server");
    show("/servers              list saved servers");
    show("/connect NAME         connect to a saved server");
    show("/disconnect           disconnect");
    show("/nick NAME            change your nickname");
    show("/join #channel        join a channel");
    show("/part                 leave the current channel");
    show("/switch #channel      change the current channel");
    show("/list [word]          all channels on the server (biggest first, 50 max)");
    show("/list-us              users of the current channel  (same as /users)");
    show("/list-ch              your current channel and joined channels");
    show("/msg NICK text        private message");
    show("/create [PORT]        start a local test server (default 6667)");
    show("/quit                 exit");
}

void App::list_servers() {
    auto names = servers.names();
    if (names.empty()) {
        show("NO saved servers , use /add NAME HOST PORT  (or /create for a local one).");
        return;
    }
    show("Saved servers:");
    for (const auto& name : names) {
        const Server* s = servers.find(name);
        show("  " + name + " -> " + s->host + ":" + std::to_string(s->port));
    }
}

void App::show_users() {
    std::string channel = client.channel();
    if (channel.empty()) {
        show("u are not in a channel.");
        return;
    }
    std::string text = "usr in " + channel + ":";
    for (const auto& user : client.users()) text += " " + user;
    show(text);
}
void App::show_my_channels() {
    std::string current = client.channel();
    if (current.empty()) {
        show("u are not in a channel twin , Use /join #channel.");
        return;
    }
    show("current channel: " + current);
    std::string others;
    for (const auto& channel : client.channels())
        if (channel != current) others += " " + channel;
    if (!others.empty()) show("Also joined:" + others);
}

void App::connect_to(const std::string& name) {
    const Server* s = servers.find(name);
    if (!s)
        show("Unknown serv: " + name + ". See /servers.");
    else
        client.connect(name, s->host, s->port);
}

void App::create_local_server(int port) {
    std::string error;
    if (!local_server.start(port, error)) {
        show(error);
        return;
    }
    servers.add("local", "0.0.0.0", port, error);
    if (on_servers_changed) on_servers_changed();
    show("LAN IRC Client listening in 0.0.0.0:" + std::to_string(port) +
         ". Connect with /connect local");
}
