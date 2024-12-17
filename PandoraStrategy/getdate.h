#pragma once
#include <iostream>
#include <ctime>
#include <sstream>
#include <string>
#pragma warning(disable:4996)
string getCurrentDateString() {
    time_t currentTime = time(nullptr);
    tm* timeInfo = localtime(&currentTime);

    char buffer[9];  // yyyyMMdd格式需要8个字符长度再加一个字符串结束符'\0'
    strftime(buffer, sizeof(buffer), "%Y%m%d", timeInfo);

    return string(buffer);
}

int main() {
    string cursor_str = getCurrentDateString();
    cout << cursor_str << endl;
    return 0;
}
