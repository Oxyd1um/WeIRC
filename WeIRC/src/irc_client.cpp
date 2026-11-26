#include "irc_client.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <sstream>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0 
#endif
void IrcClient::connect(const std::string& name, const std::string& host, int port) {
    stop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        server_name_ = name;
    }
    running_ = true;
    status("Connecting to " + name + "...");
    show("Connecting to " + name + " (" + host + ":" + std::to_string(port) + ")...");
    thread_ = std::thread(&IrcClient::run, this, host, port);
}
void IrcClient::stop() {
    running_ = false; 
    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        if (fd_ >= 0) ::shutdown(fd_, SHUT_RDWR);  // wake up poll() YOU ARE LATE ! 
    }
    if (thread_.joinable()) thread_.join();
    close_socket();
    clear_state();
}
void IrcClient::disconnect() {
    bool was_active = running_;
    stop();
    if (was_active) {
        status("Disconnected");
        emit_channels();
        emit_users();
    }
}
void IrcClient::close_socket() {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}
// Important ,  i will check this later.
void IrcClient::run(std::string host, int port) {
    std::string error;
    int fd = open_connection(host, port, error);
    std::string problem;
    if (fd < 0) {
        problem = error;
    } else {
        {
            std::lock_guard<std::mutex> lock(send_mutex_);
            fd_ = fd;
        }
        send_raw("NICK " + nick());
        send_raw("USER " + nick() + " 0 * :WeIRC user");
        read_loop();
        problem = "Connection closed.";
    }
    close_socket();
    if (running_.exchange(false)) {
        show(problem);
        clear_state();
        status("Disconnected");
        emit_channels();
        emit_users();
    }
}

// Checking Every 200ms twin.
int IrcClient::open_connection(const std::string& host, int port, std::string& error) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* list = nullptr;
    int rc = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &list);
    if (rc != 0) {
        error = std::string("DNS error: ") + gai_strerror(rc);
        return -1;
    }
    int result = -1;
    for (addrinfo* a = list; a && running_ && result < 0; a = a->ai_next) {
        int s = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s < 0) continue;
        int flags = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, flags | O_NONBLOCK);
        bool ok = (::connect(s, a->ai_addr, a->ai_addrlen) == 0);
        if (!ok && errno == EINPROGRESS) {
            for (int waited = 0; running_ && waited < 10000; waited += 200) {
                pollfd p{s, POLLOUT, 0};
                int r = ::poll(&p, 1, 200);
                if (r > 0) {
                    int err = 0;
                    socklen_t len = sizeof err;
                    getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len);
                    ok = (err == 0);
                    break;
                }
                if (r < 0 && errno != EINTR) break;
            }
        }
        if (ok) {
            fcntl(s, F_SETFL, flags);  
            result = s;
        } else {
            ::close(s);
        }
    }
    freeaddrinfo(list);
    if (result < 0) error = "Could not connect to " + host + ":" + std::to_string(port) + ".";
    return result;
}
void IrcClient::read_loop() {
std::string buffer;
// that is char ! you can't change char !
char data[4096];
    while (running_) {
        pollfd p{fd_, POLLIN, 0};
        int r = ::poll(&p, 1, 300);
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (r == 0) continue;

        ssize_t n = ::recv(fd_, data, sizeof data, 0);
        if (n <= 0) break;  
        buffer.append(data, static_cast<size_t>(n));
        if (buffer.size() > 65536) break;  

        size_t nl;
        while ((nl = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, nl);
            buffer.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) handle_line(line);
        }
    }
}
void IrcClient::send_raw(std::string line) {
    size_t cut = line.find_first_of("\r\n");
    if (cut != std::string::npos) line.resize(cut);
    if (line.size() > 510) line.resize(510);
    line += "\r\n";
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (fd_ >= 0) ::send(fd_, line.data(), line.size(), MSG_NOSIGNAL);
}

// here we go to Action world
void IrcClient::set_nick(const std::string& nick) {
    if (connected()) {
        send_raw("NICK " + nick);
    } else {
        std::lock_guard<std::mutex> lock(mutex_);
        nick_ = nick;
   }
}
void IrcClient::join(std::string channel) {
    if (channel.empty()) return;
    if (channel[0] != '#' && channel[0] != '&') channel = "#" + channel;
    send_raw("JOIN " + channel);
}
void IrcClient::part() {
std::string ch = channel();
    if (ch.empty())
        show("You are not in a channel.");
    else
        send_raw("PART " + ch);
}

void IrcClient::switch_to(const std::string& channel) {
    {
     std::lock_guard<std::mutex> lock(mutex_);
        if (std::find(channels_.begin(), channels_.end(), channel) != channels_.end())
            current_ = channel;
    }
    if (this->channel() != channel) {
        show("you are not in " + channel + ".");
        return;
    }
    show("current Channel: " + channel);
    emit_channels();
    emit_users();
}
void IrcClient::say(const std::string& text) {
    std::string target = channel();
    if (!connected() || target.empty()) {
        show("u are not in a channel twin, Use /join #channel.");
        return;
    }
    send_raw("PRIVMSG " + target + " :" + text);
    show("<" + nick() + "> " + text);
}

void IrcClient::say_to(const std::string& target, const std::string& text) {
    if (!connected()) {
        show("non connected.");
        return;
    }
    send_raw("PRIVMSG " + target + " :" + text);
    show("(to " + target + ") " + text);
}
std::string IrcClient::nick() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nick_;
}
std::string IrcClient::channel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}
std::vector<std::string> IrcClient::users() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(current_);
    if (it == users_.end()) return {};
    return std::vector<std::string>(it->second.begin(), it->second.end());
}
bool IrcClient::is_me(const std::string& n) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return n == nick_;
}
void IrcClient::clear_state() {
    std::lock_guard<std::mutex> lock(mutex_);
    registered_ = false;
    current_.clear();
    channels_.clear();
    users_.clear();
    collecting_.clear();
}
void IrcClient::emit_channels() {
    std::vector<std::string> list;
    std::string current;
    {
       std::lock_guard<std::mutex> lock(mutex_);
       list = channels_;
       current = current_;
    }
    if (on_channels) on_channels(list, current);
}
void IrcClient::emit_users() {
    if (on_users) on_users(users());
}
void IrcClient::remove_channel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_.erase(std::remove(channels_.begin(), channels_.end(), channel), channels_.end());
    users_.erase(channel);
    if (current_ == channel) current_ = channels_.empty() ? "" : channels_.back();
}
void IrcClient::handle_line(const std::string& line) {
    IrcMessage m = parse_irc_line(line);
    const std::string& cmd = m.command;
    auto param = [&](size_t i) { return i < m.params.size() ? m.params[i] : std::string(); };
    if (cmd == "PING") {
        send_raw("PONG :" + param(0));
    } else if (cmd == "001") {  
        std::string server;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!param(0).empty()) nick_ = param(0);
            registered_ = true;
            server = server_name_;
        }
        show("Connected to " + server + " as " + nick() + ".");
        status("Connected to " + server);
    } else if (cmd == "433") {  
        std::string tried = param(1);
        show("Nickname '" + tried + "' is already in use.");
        std::string next = tried + "_";
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (registered_) return;  
            nick_ = next;
        }
        show("Trying '" + next + "' instead.");
        send_raw("NICK " + next);
    } else if (cmd == "PRIVMSG" || cmd == "NOTICE") {
        handle_privmsg(m);
    } else if (cmd == "JOIN") {
        handle_join(m);
    } else if (cmd == "PART") {
        handle_part_or_kick(m, false);
    } else if (cmd == "KICK") {
        handle_part_or_kick(m, true);
    } else if (cmd == "QUIT") {
        handle_quit(m);
    } else if (cmd == "NICK") {
        handle_nick(m);
    } else if (cmd == "353" || cmd == "366") {
        handle_names(m);
    } else if (cmd == "322" || cmd == "323") {  // SOON I WILL ADD KEYSTROKES , PROMISE
        handle_list(m);
    } else if (cmd == "332") {  
        show("Topic of " + param(1) + ": " + param(2));
    } else if (cmd == "ERROR") {
        show("Server error: " + param(0));
    } else if (cmd.size() == 3 && cmd[0] >= '4' && cmd[0] <= '5') { 
        std::string text;
        for (size_t i = 1; i < m.params.size(); ++i) text += (i > 1 ? " " : "") + m.params[i];
        show(text);
    }
    
}
void IrcClient::handle_privmsg(const IrcMessage& m) {
    if (m.params.size() < 2) return;
    const std::string& target = m.params[0];
    std::string text = m.params[1];
    std::string who = m.nick.empty() ? m.prefix : m.nick;
    if (m.command == "NOTICE") {
        show("[notice] " + text);
        return;
    }
    bool action = text.size() > 8 && text.rfind("\x01" "ACTION ", 0) == 0;
    if (action) text = text.substr(8, text.size() - 9);
    std::string line = action ? "* " + who + " " + text : "<" + who + "> " + text;
    if (is_me(target))
        line = "(private) " + line;
    else if (target != channel())
        line = "[" + target + "] " + line;
    show(line);
}
void IrcClient::handle_join(const IrcMessage& m) {
    if (m.params.empty()) return;
    const std::string& room = m.params[0];
    if (is_me(m.nick)) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (std::find(channels_.begin(), channels_.end(), room) == channels_.end())
                channels_.push_back(room);
            current_ = room;
            users_[room].clear();
        }
        show("u joined " + room + ".");
        emit_channels();
        emit_users();
    } else {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            users_[room].insert(m.nick);
        }
        show(m.nick + " joined " + room + ".");
        if (room == channel()) emit_users();
    }
}
void IrcClient::handle_part_or_kick(const IrcMessage& m, bool kick) {
    if (m.params.empty()) return;
    const std::string& room = m.params[0];
    std::string who = kick ? (m.params.size() > 1 ? m.params[1] : "") : m.nick;
    if (is_me(who)) {
        remove_channel(room);
        show(kick ? "You were kicked from " + room + "." : "You left " + room + ".");
        emit_channels();
        emit_users();
    } else {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            users_[room].erase(who);
        }
        show(who + (kick ? " was kicked from " : " left ") + room + ".");
        if (room == channel()) emit_users();
    }
}
void IrcClient::handle_quit(const IrcMessage& m) {
    bool was_visible = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& item : users_)
            if (item.second.erase(m.nick)) was_visible = true;
    }
    if (was_visible) {
        show(m.nick + " quit" + (m.params.empty() ? "" : " (" + m.params[0] + ")") + ".");
        emit_users();
    }
}
void IrcClient::handle_nick(const IrcMessage& m) {
    if (m.params.empty()) return;
    const std::string& new_nick = m.params[0];
    bool me = is_me(m.nick);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (me) nick_ = new_nick;
        for (auto& item : users_)
            if (item.second.erase(m.nick)) item.second.insert(new_nick);
    }
    show(me ? "You are now known as " + new_nick + "."
            : m.nick + " is now known as " + new_nick + ".");
    emit_users();
}
void IrcClient::handle_names(const IrcMessage& m) {
    if (m.command == "353" && m.params.size() >= 4) {
        const std::string& room = m.params[2];
        std::lock_guard<std::mutex> lock(mutex_);
        if (collecting_.insert(room).second) users_[room].clear();  
        std::istringstream names(m.params[3]);
        std::string n;
        while (names >> n) {
            n.erase(0, n.find_first_not_of("@+%~&")); 
            if (!n.empty()) users_[room].insert(n);
        }
    } else if (m.command == "366" && m.params.size() >= 2) {
        const std::string& room = m.params[1];
        {
            std::lock_guard<std::mutex> lock(mutex_);
            collecting_.erase(room);
        }
        if (room == channel()) emit_users();
    }
}
std::vector<std::string> IrcClient::channels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return channels_;
}
void IrcClient::request_list(const std::string& filter) {
    if (!connected()) {
        show("non connected. Use /connect NAME first.");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        list_waiting_ = true;
        list_filter_ = filter;
        list_entries_.clear();
    }
    show("asking the serv for the ch list (this can take a few sec)...");
    send_raw("LIST");
}
static std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
void IrcClient::handle_list(const IrcMessage& m) {
    const size_t SHOW_MAX = 50;
    if (m.command == "322") {
        if (m.params.size() < 3) return;
        std::string topic = m.params.size() > 3 ? m.params[3] : "";
        std::string clean;                       
        for (char c : topic)
            if (static_cast<unsigned char>(c) >= 0x20) clean += c;
        if (clean.size() > 60) clean = clean.substr(0, 57) + "...";

        std::lock_guard<std::mutex> lock(mutex_);
        if (!list_waiting_) return;
        std::string filter = lower(list_filter_);
        if (!filter.empty() && lower(m.params[1]).find(filter) == std::string::npos) return;
        list_entries_.push_back({std::atoi(m.params[2].c_str()), m.params[1] + " " + clean});
        return;
    }
    std::vector<std::pair<int, std::string>> found;
    std::string filter;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!list_waiting_) return;
        list_waiting_ = false;
        found.swap(list_entries_);
        filter = list_filter_;
    }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });

    if (found.empty()) {
        show(filter.empty() ? "non channels found." : "non channel name contains '" + filter + "'.");
        return;
    }
    size_t count = std::min(found.size(), SHOW_MAX);
    show("Channels" + (filter.empty() ? std::string() : " matching '" + filter + "'") +
         " - showing " + std::to_string(count) + " of " + std::to_string(found.size()) +
         " (biggest first):");
    for (size_t i = 0; i < count; ++i)
        show("  " + found[i].second + "  [" + std::to_string(found[i].first) + " users]");
    if (found.size() > count) show("Narrow it down with:  /list word");
}
