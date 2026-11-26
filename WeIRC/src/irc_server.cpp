#include "irc_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <vector>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static const char* SERVER = ":weirc.local";

bool LocalServer::start(int port, std::string& error) {
    if (running_) {
        error = "Local server is already running.";
        return false;
    }
    if (port < 1 || port > 65535) {
        error = "Invalid port.";
        return false;
    }
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = std::string("socket: ") + std::strerror(errno);
        return false;
    }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(port));
    inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);  // all interfaces (whole LAN)

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0 || ::listen(fd, 16) < 0) {
        error = "Could not listen on 0.0.0.0:" + std::to_string(port) + " (" +
                std::strerror(errno) + ")";
        ::close(fd);
        return false;
    }
    listen_fd_ = fd;
    running_ = true;
    thread_ = std::thread(&LocalServer::loop, this);
    return true;
}

void LocalServer::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    for (auto& item : clients_) ::close(item.first);
    clients_.clear();
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
}

// ---------------------------------------------------------------------
// Main loop: wait for sockets, read lines, handle them.
// ---------------------------------------------------------------------
void LocalServer::loop() {
    while (running_) {
        std::vector<pollfd> fds{{listen_fd_, POLLIN, 0}};
        for (const auto& item : clients_) fds.push_back({item.first, POLLIN, 0});

        int ready = ::poll(fds.data(), fds.size(), 300);
        if (ready <= 0) continue;

        if (fds[0].revents & POLLIN) accept_client();

        std::vector<int> dead;
        for (size_t i = 1; i < fds.size(); ++i) {
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            auto it = clients_.find(fds[i].fd);
            if (it == clients_.end()) continue;
            Client& c = it->second;

            char data[1024];
            ssize_t n = ::recv(c.fd, data, sizeof data, 0);
            if (n <= 0) {
                dead.push_back(c.fd);
                continue;
            }
            c.buffer.append(data, static_cast<size_t>(n));

            size_t nl;
            while (!c.quit && (nl = c.buffer.find('\n')) != std::string::npos) {
                std::string line = c.buffer.substr(0, nl);
                c.buffer.erase(0, nl + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) handle_line(c, line);
            }
            if (c.quit) dead.push_back(c.fd);
        }
        for (int fd : dead) remove_client(fd, "Quit");
    }
}

void LocalServer::accept_client() {
    int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd < 0) return;
    Client c;
    c.fd = fd;
    clients_[fd] = c;
}

// ---------------------------------------------------------------------
// One line from a client.
// ---------------------------------------------------------------------
void LocalServer::handle_line(Client& c, const std::string& line) {
    IrcMessage m = parse_irc_line(line);
    const std::string& cmd = m.command;
    std::string p0 = m.params.empty() ? "" : m.params[0];

    if (cmd == "NICK") {
        if (p0.empty()) return;
        Client* other = find_nick(p0);
        if (other && other != &c) {
            send_to(c, std::string(SERVER) + " 433 * " + p0 + " :Nickname is already in use");
            return;
        }
        std::string old_who = who(c);
        bool was_registered = c.registered;
        c.nick = p0;
        if (was_registered) {
            std::string msg = ":" + old_who + " NICK :" + p0;
            send_to(c, msg);
            to_peers(c, msg);
        }
        try_register(c);
    } else if (cmd == "USER") {
        c.got_user = true;
        try_register(c);
    } else if (cmd == "PING") {
        send_to(c, std::string(SERVER) + " PONG weirc.local :" + p0);
    } else if (cmd == "QUIT") {
        c.quit = true;
    } else if (!c.registered) {
        send_to(c, std::string(SERVER) + " 451 * :You have not registered");
    } else if (cmd == "JOIN") {
        // "JOIN #a,#b"
        size_t start = 0;
        while (start <= p0.size()) {
            size_t comma = p0.find(',', start);
            std::string room = p0.substr(start, comma == std::string::npos ? comma : comma - start);
            if (!room.empty()) join(c, room[0] == '#' ? room : "#" + room);
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    } else if (cmd == "PART") {
        part(c, p0);
    } else if (cmd == "NAMES") {
        send_names(c, p0);
    } else if (cmd == "LIST") {
        std::map<std::string, int> counts;
        for (const auto& item : clients_)
            for (const auto& room : item.second.channels) ++counts[room];
        for (const auto& item : counts)
            send_to(c, std::string(SERVER) + " 322 " + c.nick + " " + item.first + " " +
                           std::to_string(item.second) + " :");
        send_to(c, std::string(SERVER) + " 323 " + c.nick + " :End of LIST");
    } else if (cmd == "PRIVMSG" || cmd == "NOTICE") {
        if (m.params.size() < 2) return;
        std::string msg = ":" + who(c) + " " + cmd + " " + p0 + " :" + m.params[1];
        if (p0[0] == '#') {
            if (!c.channels.count(p0))
                send_to(c, std::string(SERVER) + " 404 " + c.nick + " " + p0 +
                               " :Cannot send to channel");
            else
                to_channel(p0, msg, c.fd);  // everyone EXCEPT the sender
        } else if (Client* target = find_nick(p0)) {
            send_to(*target, msg);
        } else {
            send_to(c, std::string(SERVER) + " 401 " + c.nick + " " + p0 + " :No such nick");
        }
    }
}

void LocalServer::try_register(Client& c) {
    if (c.registered || c.nick.empty() || !c.got_user) return;
    c.registered = true;
    send_to(c, std::string(SERVER) + " 001 " + c.nick + " :Welcome to WeIRC, " + c.nick);
}

void LocalServer::join(Client& c, const std::string& room) {
    if (c.channels.count(room)) return;
    c.channels.insert(room);
    to_channel(room, ":" + who(c) + " JOIN " + room);  // includes the joiner
    send_names(c, room);
}

void LocalServer::part(Client& c, const std::string& room) {
    if (!c.channels.count(room)) {
        send_to(c, std::string(SERVER) + " 442 " + c.nick + " " + room + " :You're not on that channel");
        return;
    }
    to_channel(room, ":" + who(c) + " PART " + room);  // includes the leaver
    c.channels.erase(room);
}

void LocalServer::send_names(Client& c, const std::string& room) {
    std::string names;
    for (const auto& item : clients_)
        if (item.second.channels.count(room)) names += item.second.nick + " ";
    send_to(c, std::string(SERVER) + " 353 " + c.nick + " = " + room + " :" + names);
    send_to(c, std::string(SERVER) + " 366 " + c.nick + " " + room + " :End of NAMES list");
}

void LocalServer::remove_client(int fd, const std::string& reason) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) return;
    if (it->second.registered) to_peers(it->second, ":" + who(it->second) + " QUIT :" + reason);
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
    clients_.erase(it);
}

// ---------------------------------------------------------------------
// Sending helpers
// ---------------------------------------------------------------------
void LocalServer::send_to(Client& c, const std::string& text) {
    std::string line = text + "\r\n";
    ::send(c.fd, line.data(), line.size(), MSG_NOSIGNAL);
}

void LocalServer::to_channel(const std::string& room, const std::string& text, int except_fd) {
    for (auto& item : clients_)
        if (item.first != except_fd && item.second.channels.count(room)) send_to(item.second, text);
}

void LocalServer::to_peers(Client& c, const std::string& text) {
    for (auto& item : clients_) {
        if (item.first == c.fd) continue;
        for (const auto& room : c.channels)
            if (item.second.channels.count(room)) {
                send_to(item.second, text);
                break;  // send once per person
            }
    }
}

LocalServer::Client* LocalServer::find_nick(const std::string& nick) {
    for (auto& item : clients_)
        if (item.second.registered && item.second.nick == nick) return &item.second;
    return nullptr;
}
