#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace test {
struct Case { const char* name; std::function<void()> run; };
inline std::vector<Case>& cases() { static std::vector<Case> result; return result; }
struct Register { Register(const char* name, std::function<void()> run) { cases().push_back({name, std::move(run)}); } };
inline void require(bool ok, const char* expression, const char* file, int line) {
    if (!ok) throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + ": " + expression);
}
template<class Exception, class Function>
void throws(Function run, const char* expression, const char* file, int line) {
    bool caught = false;
    try { run(); } catch (const Exception&) { caught = true; }
    require(caught, expression, file, line);
}
}
#define TEST_JOIN_(a,b) a##b
#define TEST_JOIN(a,b) TEST_JOIN_(a,b)
#define TEST_CASE(name) \
    static void TEST_JOIN(test_,__LINE__)(); \
    static test::Register TEST_JOIN(reg_,__LINE__)(name, TEST_JOIN(test_,__LINE__)); \
    static void TEST_JOIN(test_,__LINE__)()
#define CHECK(expression) test::require(bool(expression), #expression, __FILE__, __LINE__)
#define THROWS_AS(expression, exception) test::throws<exception>([&]{ expression; }, #expression, __FILE__, __LINE__)
