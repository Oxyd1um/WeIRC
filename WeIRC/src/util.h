#pragma once
#include <ctime>
#include <string>
inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
inline std::string timestamp() {
    std::time_t now = std::time(nullptr);
    std::tm t{};
    localtime_r(&now, &t);
    char buf[16];
    std::strftime(buf, sizeof buf, "%H:%M:%S", &t);
    return buf;
}
