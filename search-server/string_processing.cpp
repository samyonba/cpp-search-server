#include "string_processing.h"

#include <algorithm>

using namespace std;

vector<string_view> SplitIntoWords(const string_view text) {
    vector<string_view> words;

    auto pos = text.find_first_not_of(' ');
    auto pos_end = text.npos;

    while (pos != pos_end)
    {
        auto space = text.find(' ', pos);
        string_view word = space == pos_end ? text.substr(pos) : text.substr(pos, space - pos);
        words.push_back(word);
        pos = text.find_first_not_of(' ', space);
    }

    return words;
}
