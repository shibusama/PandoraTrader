#include <iostream>
#include <string>
#include <map>
#include <unordered_map>

int main() {
    // 使用 std::map
    std::map<std::string, int>* mapDict = { {"apple", 1}, {"banana", 2}, {"cherry", 3} };

    for (const auto& [key, value] : (*mapDict)) {
        // 遍历 std::map 中的每个元素
        std::cout << "Key: " << key << ", Value: " << value << std::endl;
    }

    // 使用 std::unordered_map
    std::unordered_map<std::string, int> unorderedMapDict = { {"apple", 1}, {"banana", 2}, {"cherry", 3} };
    std::cout << "Using std::unordered_map:" << std::endl;
    for (const auto& [key, value] : unorderedMapDict) {
        // 遍历 std::unordered_map 中的每个元素
        std::cout << "Key: " << key << ", Value: " << value << std::endl;
    }
    return 0;
}