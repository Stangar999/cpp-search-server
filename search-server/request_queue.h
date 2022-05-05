#pragma once

#include <iostream>
#include <deque>
#include <vector>

#include "document.h"
#include "search_server.h"

//-------------------------------------------------------------------------------------------------------------
class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    // сделаем "обертки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    void AddRequest(int results_num);

    struct QueryResult {
        uint64_t timestamp = 0;
        int results = 0;
    };

    std::deque<QueryResult> requests_;

    const SearchServer& search_server_;

    int no_results_requests_;

    uint64_t current_time_;

    const static int min_in_day_ = 1440;
};
//-------------------------------------------------------------------------------------------------------------
template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
    AddRequest(result.size());
    return result;
}
