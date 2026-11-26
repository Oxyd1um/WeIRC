#include "servers.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
static std::filesystem::path config_dir() {
    const char* home = std::getenv("HOME");
    if (!home) return {};
    return std::filesystem::path(home) / ".local/share/WeIRC";
}
static bool valid_name(const std::string& name) {
    if (name.empty() || !std::isalnum(static_cast<unsigned char>(name[0]))) return false;
    for (char c : name)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != '-')
            return false;
    return true;
}
void ServerList::load() {
    auto dir = config_dir();
    if (dir.empty()) return;
    std::ifstream file(dir / "servers.conf");
    std::string name, host;
    int port;
    while (file >> name >> host >> port)
        if (valid_name(name) && port > 0 && port <= 65535) servers_[name] = {host, port};
}
void ServerList::save() const {
    auto dir = config_dir();
    if (dir.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::ofstream file(dir / "servers.conf");
    for (const auto& [name, s] : servers_) file << name << ' ' << s.host << ' ' << s.port << '\n';
}
bool ServerList::add(const std::string& name, const std::string& host, int port,
                     std::string& error) {
    if (!valid_name(name)) {
        error = "Bad name. Use letters, digits, '.', '_' or '-' only.";
        return false;
    }
    if (host.empty() || port < 1 || port > 65535) {
        error = "Bad host or port.";
        return false;
    }
    servers_[name] = {host, port};
    save();
    return true;
}
const Server* ServerList::find(const std::string& name) const {
    auto it = servers_.find(name);
    return it == servers_.end() ? nullptr : &it->second;
}
std::vector<std::string> ServerList::names() const {
std::vector<std::string> out;
    for (const auto& item : servers_) out.push_back(item.first);
    return out;
}
