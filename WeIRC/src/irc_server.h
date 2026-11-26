#pragma once
#include <atomic>
#include <map>
#include <set>
#include <string>
#include <thread>
#include "irc_message.h"
class LocalServer {
public:
   ~LocalServer() { stop(); }
   bool start(int port, std::string& error);
   void stop();
   bool running() const { return running_; }
private:
   struct Client {
        int fd = -1;
       std::string nick, buffer;
       bool got_user = false, registered = false, quit = false;
        std::set<std::string> channels;
    };
    void loop();
   void accept_client();
    void handle_line(Client& c, const std::string& line);
    void try_register(Client& c);
    void join(Client& c, const std::string& room);
   void part(Client& c, const std::string& room);
    void send_names(Client& c, const std::string& room);

    void remove_client(int fd, const std::string& reason);
    void send_to(Client& c, const std::string& text);
   void to_channel(const std::string& room, const std::string& text, int except_fd = -1);
    void to_peers(Client& c, const std::string& text);
    Client* find_nick(const std::string& nick);
    static std::string who(const Client& c) { return c.nick + "!" + c.nick + "@local"; }
   int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread thread_;
   std::map<int, Client> clients_; 
};
