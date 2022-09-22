#pragma once
#include "search_server.h"
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    // "обёртки" для всех методов поиска, чтобы хранить результаты для статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        size_t documents_count = 0;

        bool IsNullRequest() const;
    };

    const static int min_in_day_ = 1440;

    std::deque<QueryResult> requests_;

    const SearchServer& server_;

    size_t null_result_count_ = 0;

private:
    void ProcessQueryResult(const QueryResult& query_result);
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    QueryResult query_result{};
    auto documents = server_.FindTopDocuments(raw_query, document_predicate);
    query_result.documents_count = documents.size();
    ProcessQueryResult(query_result);

    return documents;
}
