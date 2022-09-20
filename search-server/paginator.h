#pragma once
#include <iterator>
#include <iostream>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
        : begin_(begin), end_(end), size_(distance(begin, end)) {

    }

    Iterator begin() const {
        return begin_;
    }

    Iterator end() const {
        return end_;
    }

    size_t size() const {
        return std::distance(begin_, end_);
    }

private:
    Iterator begin_;
    Iterator end_;
    size_t size_;
};

template<typename Iterator>
std::ostream& operator<< (std::ostream& output, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it)
    {
        output << *it;
    }
    return output;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        size_t total_docs_count = std::distance(begin, end);
        size_t added_docs_count = 0;

        for (size_t i = 0; i < total_docs_count - page_size; i += page_size)
        {
            pages_.push_back({ begin, std::next(begin, page_size) });
            std::advance(begin, page_size);
            added_docs_count += page_size;
        }
        if (added_docs_count < total_docs_count) {
            pages_.push_back({ begin, end });
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
