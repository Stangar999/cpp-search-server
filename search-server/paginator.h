#pragma once

#include <vector>
#include <iostream>
#include <algorithm>
//-------------------------------------------------------------------------------------------------------------
template <typename Iterator>
class IteratorRange{
public:
    IteratorRange(Iterator begin, Iterator end):
        begin_(begin),
        end_(end)
    {
    }
    Iterator begin() const {
        return begin_;
    }
    Iterator end() const {
        return end_;
    }
private:
    Iterator begin_;
    Iterator end_;
};
//-------------------------------------------------------------------------------------------------------------
template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        for (size_t left = distance(begin, end); left > 0;) {
            const size_t current_page_size = std::min(page_size, left);
            const Iterator current_page_end = next(begin, current_page_size);
            pages_.push_back({begin, current_page_end});

            left -= current_page_size;
            begin = current_page_end;
        }
    }
    auto begin() const {
        return pages_.begin();
    }
    auto end() const {
        return pages_.end();
    }
    int size() const {
        return pages_.end() - pages_.begin();
    }
private:
    std::vector<IteratorRange<Iterator>> pages_;
};
//-------------------------------------------------------------------------------------------------------------
template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
//-------------------------------------------------------------------------------------------------------------
template <typename Iterator>
std::ostream& operator<<(std::ostream& out, const IteratorRange<Iterator>& Itr) {
    for (Iterator it = Itr.begin(); it != Itr.end(); ++it) {
        out << *it;
    }
    return out;
}
//-------------------------------------------------------------------------------------------------------------
