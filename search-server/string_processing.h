#pragma once

#include <vector>
#include <string>
#include <set>

// преобразует строку (string_view) в вектор слов, проводя их к нижнему регистру
std::vector<std::string_view> SplitIntoWords(const std::string_view text);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {

    using namespace std;

    set<string, std::less<>> non_empty_strings;
    for (const string str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }

    return non_empty_strings;
}