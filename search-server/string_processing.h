#pragma once
#include <vector>
#include <string>
#include <set>

// преобразует строку в вектор слов, проводя их к нижнему регистру
std::vector<std::string> SplitIntoWords(const std::string& text);

// A valid text must not contain special characters
bool IsValidText(const std::string& text);

bool ContainsDoubleDash(const std::string& text);

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {

    using namespace std;

    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}