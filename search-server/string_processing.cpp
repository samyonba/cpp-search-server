#include "string_processing.h"
#include <algorithm>

using namespace std;

// преобразует строку в вектор слов, приводя их к нижнему регистру
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
