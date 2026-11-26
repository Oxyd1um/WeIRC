#pragma once
#include <map>
#include <string>
#include <vector>
struct Server {
    std::string host;
    int port = 6667;
};
class ServerList {
public:
void load();
bool add(const std::string& name, const std::string& host, int port, std::string& error);
const Server* find(const std::string& name) const;
std::vector<std::string> names() const;
private:
void save() const;
std::map<std::string, Server> servers_;
};
