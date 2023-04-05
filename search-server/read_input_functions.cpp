#include "read_input_functions.h"

#include <iostream>
#include <sstream>

using namespace std;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<int> ReadRatingsLine() {
    vector<int> ratings;
    string rawInput;
    stringstream ss;

    getline(cin, rawInput);
    ss << rawInput;

    size_t ratings_count = 0u;
    int val;
    ss >> ratings_count;

    for (size_t i = 0; i < ratings_count; i++)
    {
        ss >> val;
        ratings.push_back(val);
    }

    return ratings;
}