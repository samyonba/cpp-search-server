#pragma once

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"
#include "read_input_functions.h"

#include <string>
#include <stdexcept>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <execution>
#include <string_view>
#include <mutex>

const int MAX_RESULT_DOCUMENT_COUNT = 5;

const double EPSILON = 1e-6;

class SearchServer {

public:

    template<typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words);
    
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view stop_words_text);

    SearchServer() = default;

    size_t GetDocumentCount() const;

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    void SetStopWords(const std::string_view text);

    template <typename Requirement>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, Requirement requirement) const;

    template <typename Requirement>
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, Requirement requirement) const;

    template <typename Requirement>
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, Requirement requirement) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus required_status) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, DocumentStatus required_status) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, DocumentStatus required_status) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const;

    // итератор на начало вектора id всех документов
    std::set<int>::const_iterator begin() const;

    // итератор на конец вектора id всех документов
    std::set<int>::const_iterator end() const;

    // получение частот слов по id документа
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    // удаление документа по id
    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);

    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

private:

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    struct DocumentData
    {
        DocumentStatus status = DocumentStatus::ACTUAL;
        int rating = 0;
    };

    // хранит id документов в порядке добавления пользователем;
    std::set<int> added_documents_id_;

    std::set<std::string, std::less<>> words_data_;

    // word -> [ document_id, TF ]
    std::map<std::string_view, std::map<int, double>> word_to_documents_freqs_;

    // id -> [ word, TF ]
    std::map<int, std::map<std::string_view, double>> document_to_words_freqs_;

    // соответствие id -> status, rating
    std::map<int, DocumentData> documents_data_;

    std::set<std::string, std::less<>> stop_words_;

private:

    bool IsStopWord(const std::string_view word) const;

    // преобразует строку в вектор слов, проводя их к нижнему регистру и игнорируя стоп-слова
    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    template <typename Requirement>
    std::vector<Document> FindAllDocuments(const Query& parsed_query, Requirement requirement) const;

    template <typename Requirement>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& parsed_query, Requirement requirement) const;

    template <typename Requirement>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& parsed_query, Requirement requirement, size_t buckets_count = 100) const;

    // A valid text must not contain special characters
    bool IsValidText(const std::string_view text) const;

    // переданная строка начинается с '-'
    static bool MatchedAsMinusWord(const std::string_view word);

    void ParseQueryWord(const std::string_view word, Query& query) const;

    //преобразует строку-запрос в структуру {плюс-слов, минус-слов}
    Query ParseQuery(const std::string_view text, const bool erase_duplicates = true) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    int GetDocumentRating(int document_id) const;
};

template<typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {

    using namespace std;

    for (const string& word : stop_words_) {
        if (!IsValidText(word))
        {
            throw invalid_argument("Stop word contains special characters"s);
        }
    }
}

template <typename Requirement>
std::vector<Document> SearchServer::FindAllDocuments(const Query& parsed_query, Requirement requirement) const {

    using namespace std;

    vector<Document> matched_documents;

    // [id, relevance]
    map<int, double> document_term_freq_idf_relevance;

    for (const string_view plus_word : parsed_query.plus_words) {
        if (word_to_documents_freqs_.count(plus_word))
        {
            double word_idf = log(static_cast<double>(added_documents_id_.size()) / word_to_documents_freqs_.at(plus_word).size());
            for (const auto& [document_id, term_freq] : word_to_documents_freqs_.at(plus_word)) {
                const auto& current_document_data = documents_data_.at(document_id);
                if (requirement(document_id, current_document_data.status, current_document_data.rating))
                {
                    document_term_freq_idf_relevance[document_id] += word_idf * term_freq;
                }
            }
        }
    }
    for (const string_view minus_word : parsed_query.minus_words) {
        if (word_to_documents_freqs_.count(minus_word))
        {
            for (const auto& [document_id, term_freq] : word_to_documents_freqs_.at(minus_word)) {
                document_term_freq_idf_relevance.erase(document_id);
            }
        }
    }

    for (const auto& [document_id, relevance] : document_term_freq_idf_relevance) {
        matched_documents.push_back({ document_id, relevance, GetDocumentRating(document_id) });
    }

    return matched_documents;
}

template <typename Requirement>
std::vector<Document> SearchServer::FindAllDocuments
(const std::execution::sequenced_policy&, const Query& parsed_query, Requirement requirement) const {

    return SearchServer::FindAllDocuments(parsed_query, requirement);
}

template <typename Requirement>
std::vector<Document> SearchServer::FindAllDocuments
(const std::execution::parallel_policy&, const Query& parsed_query, Requirement requirement, size_t buckets_count) const {

    using namespace std;

    vector<Document> matched_documents;

    ConcurrentMap<int, double> docs_to_relevance(buckets_count);

    set<int> docs_to_ignore;
    std::mutex mutex;

	for_each(execution::par, parsed_query.plus_words.begin(), parsed_query.plus_words.end(),
		[&](const string_view plus_word) {
			if (word_to_documents_freqs_.count(plus_word))
			{
				double word_idf = log(static_cast<double>(added_documents_id_.size()) / word_to_documents_freqs_.at(plus_word).size());
				for (const auto& [document_id, term_freq] : word_to_documents_freqs_.at(plus_word)) {
					const auto& current_document_data = documents_data_.at(document_id);
					if (requirement(document_id, current_document_data.status, current_document_data.rating))
					{
                        docs_to_relevance[document_id].ref_to_value +=  word_idf * term_freq;
					}
				}
			}
		}
    );

	for_each(execution::par, parsed_query.minus_words.begin(), parsed_query.minus_words.end(),
		[&](const string_view minus_word) {
			if (word_to_documents_freqs_.count(minus_word))
			{
				for (const auto& [document_id, term_freq] : word_to_documents_freqs_.at(minus_word)) {

                    lock_guard guard(mutex);
                    docs_to_ignore.insert(document_id);
				}
			}
		}
	);

    for (const auto& [document_id, relevance] : docs_to_relevance.BuildOrdinaryMap()) {
        if (!docs_to_ignore.count(document_id)) {
            matched_documents.push_back({ document_id, relevance, GetDocumentRating(document_id) });
        }
    }

    return matched_documents;
}

template <typename Requirement>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, Requirement requirement) const {

	using namespace std;

	// throws invalid_argument exception
	Query parsed_query = ParseQuery(raw_query);

	auto matched_documents = FindAllDocuments(parsed_query, requirement);

	sort(matched_documents.begin(), matched_documents.end(),
		[](const Document& lhs, const Document& rhs) {
			return lhs.relevance > rhs.relevance ||
				(abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating);
		});

	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}

	return matched_documents;
}

template <typename Requirement>
std::vector<Document> SearchServer::FindTopDocuments
(const std::execution::sequenced_policy&, const std::string_view raw_query, Requirement requirement) const {
	return FindTopDocuments(raw_query, requirement);
}

template <typename Requirement>
std::vector<Document> SearchServer::FindTopDocuments
(const std::execution::parallel_policy&, const std::string_view raw_query, Requirement requirement) const {

    using namespace std;

    // throws invalid_argument exception
    Query parsed_query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(execution::par, parsed_query, requirement);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            return lhs.relevance > rhs.relevance ||
                (abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating);
        });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server, int document_id, const std::string& document,
    DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);
