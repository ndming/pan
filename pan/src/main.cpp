#include <iostream>

#include <vector>
#include <string_view>

void print(const std::vector<std::string_view>& layers) {
    for (const auto layer : layers) {
        std::cout << layer << '\n';
    }
}

int main(int argc, char* argv[]) {
    print({ "Hello", "pan" });
}
