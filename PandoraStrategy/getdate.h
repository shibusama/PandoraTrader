#pragma once
#include <iostream>
#include <ctime>
#include <sstream>
#include <string>
#pragma warning(disable:4996)
std::string getCurrentDateString() {
    std::time_t currentTime = std::time(nullptr);
    std::tm* timeInfo = std::localtime(&currentTime);

    char buffer[9];  // yyyyMMdd格式需要8个字符长度再加一个字符串结束符'\0'
    std::strftime(buffer, sizeof(buffer), "%Y%m%d", timeInfo);

    return std::string(buffer);
}

int main() {
    std::string cursor_str = getCurrentDateString();
    std::cout << cursor_str << std::endl;
    return 0;
}
