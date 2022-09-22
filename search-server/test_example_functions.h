#pragma once
#include <iostream>
#include <string>
#include "document.h"

void TestSearchServer();

void AssertImpl(bool value, const std::string& value_str, const std::string& hint, const std::string& file,
    uint32_t line, const std::string& function);

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {

    using namespace std;

    if (t != u)
    {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

template <typename T>
void RunTestImpl(const T& func, const std::string& func_name) {

    using namespace std;

    func();
    cerr << func_name << " OK"s << endl;
}

#define ASSERT(a) AssertImpl((a), #a, ""s, __FILE__, __LINE__, __FUNCTION__)

#define ASSERT_HINT(a, hint) AssertImpl((a), #a, (hint), __FILE__, __LINE__, __FUNCTION__)

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define RUN_TEST(func) RunTestImpl((func), #func);
