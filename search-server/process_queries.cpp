#include "process_queries.h"
#include "search_server.h"

#include <vector>
#include <list>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> result(queries.size());
    transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&](const std::string& query) {
        return search_server.FindTopDocuments(query);
        });
    return result;
}

//std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
//{
//    std::list<std::list<Document>> result(queries.size());
//    transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&](const std::string& query) {
//        std::vector<Document> find_res = search_server.FindTopDocuments(query);
//        return std::list<Document>(find_res.begin(), find_res.end());
//        });
//
//    std::list<Document> joined_result;
//    for (auto& container : result) {
//        joined_result.insert(joined_result.end(), container.begin(), container.end());
//    }
//    return joined_result;
//}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    size_t total_size = 0u;
    auto matched_docs = ProcessQueries(search_server, queries);
    for (auto& query_res : matched_docs) {
        total_size += query_res.size();
    }
    std::vector<Document> result;
    result.reserve(total_size);
    for (auto& query_res : matched_docs) {
        result.insert(result.end(), query_res.begin(), query_res.end());
    }
    
    return result;
}
