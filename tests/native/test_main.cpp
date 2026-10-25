#include "test.hpp"
#include <iostream>

int main() {
    int failed = 0;
    for (const auto& item : test::cases()) {
        try { item.run(); std::cout << "PASS " << item.name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << item.name << ": " << error.what() << '\n'; }
        catch (...) { ++failed; std::cerr << "FAIL " << item.name << ": unknown exception\n"; }
    }
    std::cout << test::cases().size() - failed << '/' << test::cases().size() << " headless tests passed\n";
    return failed ? 1 : 0;
}
