#pragma once
#include <cctype>
#include <string>
#include <vector>
struct IrcMessage {
std::string prefix;  
std::string nick;     
std::string command;  
 std::vector<std::string> params;
};
inline IrcMessage parse_irc_line(const std::string& line) {
IrcMessage m;
size_t pos = 0;
const size_t n = line.size();
   auto skip_spaces = [&] { while (pos < n && line[pos] == ' ') ++pos; };
     auto next_word = [&] {
        size_t end = line.find(' ', pos);
        if (end == std::string::npos) end = n;
        std::string word = line.substr(pos, end - pos);
        pos = end;
     return word;
    };
     skip_spaces();
    if (pos < n && line[pos] == '@') { next_word(); skip_spaces(); }  
    if (pos < n && line[pos] == ':') {
        ++pos;
        m.prefix = next_word();
        m.nick = m.prefix.substr(0, m.prefix.find('!'));
        skip_spaces();
    }
    m.command = next_word();
    for (char& c : m.command) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (;;) {
        skip_spaces();
        if (pos >= n) break;
        if (line[pos] == ':') { m.params.push_back(line.substr(pos + 1)); break; }
        m.params.push_back(next_word());
    }
 return m;
}
