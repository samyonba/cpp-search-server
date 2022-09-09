#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>


std::string ReadLine();

int ReadLineWithNumber();

std::vector<int> ReadRatingsLine();

template <typename T>
std::ostream& operator<< (std::ostream& output, const std::vector<T>& container) {

    using namespace std;

    output << '[';
    bool is_first = true;
    for (const T& elem : container) {
        if (!is_first)
            output << ',';
        output << ' ' << elem;
        is_first = false;
    }
    output << " ]"s;
    return output;
}

template <typename T>
std::ostream& operator<< (std::ostream& os, std::set<T> container) {

    using namespace std;

    bool is_first = true;
    os << '{';
    for (const auto& elem : container) {
        if (!is_first) {
            os << ", "s << elem;
        }
        else
            os << elem;
        is_first = false;
    }
    os << '}';
    return os;
}

template <typename K, typename V>
std::ostream& operator<< (std::ostream& os, std::map<K, V> container) {

    using namespace std;

    bool is_first = true;
    os << '{';
    for (const auto& [key, value] : container) {
        if (!is_first) {
            os << ", "s << key << ": "s << value;
        }
        else
            os << key << ": "s << value;
        is_first = false;
    }
    os << '}';
    return os;
}