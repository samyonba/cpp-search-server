#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : server_(search_server) {

}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    QueryResult query_result{};
    auto documents = server_.FindTopDocuments(raw_query, status);
    query_result.documents_count = documents.size();
    ProcessQueryResult(query_result);

    return documents;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    QueryResult query_result{};
    auto documents = server_.FindTopDocuments(raw_query);
    query_result.documents_count = documents.size();
    ProcessQueryResult(query_result);

    return documents;
}

int RequestQueue::GetNoResultRequests() const {
    return static_cast<int>(null_result_count_);
}

bool RequestQueue::QueryResult::IsNullRequest() const {
    return documents_count == 0;
}

void RequestQueue::ProcessQueryResult(const QueryResult& query_result) {
    requests_.push_back(query_result);
    if (query_result.IsNullRequest()) {
        ++null_result_count_;
    }
    if (requests_.size() > min_in_day_) {
        if (requests_.front().IsNullRequest()) {
            --null_result_count_;
        }
        requests_.pop_front();
    }
}
