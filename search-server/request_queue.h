#pragma once
#include "search_server.h"
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : server_(search_server) {
        // напишите реализацию
    }

    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        QueryResult query_result{};
        auto documents = server_.FindTopDocuments(raw_query, document_predicate);
        query_result.documents_count = documents.size();
        ProcessQueryResult(query_result);

        return documents;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        size_t documents_count;

        bool IsNullRequest() const;
    };

    const static int min_in_day_ = 1440;

    std::deque<QueryResult> requests_;

    const SearchServer& server_;

    size_t null_result_count_ = 0;

private:
    void ProcessQueryResult(const QueryResult& query_result);
};
