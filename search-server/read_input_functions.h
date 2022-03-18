#pragma once

#include <iostream>
#include <set>
#include <map>
#include <vector>

using namespace std::string_literals;
//-------------------------------------------------------------------------------------------------------------
std::string ReadLine();
//-------------------------------------------------------------------------------------------------------------
int ReadLineWithNumber();
//-------------------------------------------------------------------------------------------------------------
template <typename T1, typename T2>
std::ostream& operator<<(std::ostream& out, const std::pair<T1, T2>& container) {
    out << "("s << container.first<<", "<<container.second << ")"s;
    return out;
}
//-------------------------------------------------------------------------------------------------------------
template <typename Container>
void Print(std::ostream& out, const Container& container) {
    bool is_first = true;
    for (const auto& element : container) {
        if (!is_first) {
            out << ", "s;
        }
        is_first = false;
        out << element;
    }
}
//-------------------------------------------------------------------------------------------------------------
template <typename Element>
std::ostream& operator<<(std::ostream& out, const std::vector<Element>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}
//-------------------------------------------------------------------------------------------------------------
template <typename Element>
std::ostream& operator<<(std::ostream& out, const std::set<Element>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}
//-------------------------------------------------------------------------------------------------------------
template <typename T1, typename T2>
std::ostream& operator<<(std::ostream& out, const std::map<T1,T2>& container) {
    out << "<"s;
    Print(out, container);
    out << ">"s;
    return out;
}
//-------------------------------------------------------------------------------------------------------------
