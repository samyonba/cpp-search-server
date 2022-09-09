#include "string_processing.h"
#include <algorithm>

using namespace std;

// преобразует строку в вектор слов, проводя их к нижнему регистру
vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += tolower(c);
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

// A valid text must not contain special characters
bool IsValidText(const string& text) {
    return none_of(text.begin(), text.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

bool ContainsDoubleDash(const string& text) {
    return (text.find("--"s) != string::npos);
}
