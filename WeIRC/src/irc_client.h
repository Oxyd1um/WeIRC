#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "irc_message.h"
class IrcClient {
public:
   std::function<void(const std::string&)> on_text;
    std::function<void(const std::string&)> on_status; 
    std::function<void(const std::vector<std::string>&)> on_users;  
    std::function<void(const std::vector<std::string>&, const std::string&)> on_channels;
    ~IrcClient() { stop(); }
   void connect(const std::string& name, const std::string& host, int port);
    void disconnect();  
    void stop();        
    bool active() const { return running_; }                   
    bool connected() const { return running_ && fd_ >= 0; }    
     void set_nick(const std::string& nick);
    void join(std::string channel);
     void part(); 
    void switch_to(const std::string& channel);   

    void say(const std::string& text);            
    void say_to(const std::string& target, const std::string& text);  
    void request_list(const std::string& filter);  
    void quit() { send_raw("QUIT :Leaving"); }

    void send_raw(std::string line);              
    std::string nick() const;
     std::string channel() const;
    std::vector<std::string> users() const;        
     std::vector<std::string> channels() const;     
private:
    void run(std::string host, int port);
    int open_connection(const std::string& host, int port, std::string& error);
    void read_loop();
    void close_socket();
    void handle_line(const std::string& line);
    void handle_privmsg(const IrcMessage& m);
    void handle_join(const IrcMessage& m);
    void handle_part_or_kick(const IrcMessage& m, bool kick);
    void handle_quit(const IrcMessage& m);
    void handle_nick(const IrcMessage& m);
    void handle_names(const IrcMessage& m);
    void handle_list(const IrcMessage& m);
    void clear_state();
    void remove_channel(const std::string& channel);
    bool is_me(const std::string& nick) const;
    void show(const std::string& text) { if (on_text) on_text(text); }
    void status(const std::string& text) { if (on_status) on_status(text); }
    void emit_channels();
    void emit_users();
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<int> fd_{-1};
    std::mutex send_mutex_;   
    mutable std::mutex mutex_;   
    std::string nick_ = "WeIRC";
    std::string server_name_;
    bool registered_ = false;                   
    std::string current_;                      
    std::vector<std::string> channels_;        
    std::map<std::string, std::set<std::string>> users_;  
    std::set<std::string> collecting_;          
    bool list_waiting_ = false;                 
    std::string list_filter_;
    std::vector<std::pair<int, std::string>> list_entries_;  
};
