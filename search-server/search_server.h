#pragma once
#include <string>
#include "string_processing.h"
#include <stdexcept>
#include "document.h"
#include <vector>
#include <map>
#include <set>
#include "read_input_functions.h"
#include <cmath>

const int MAX_RESULT_DOCUMENT_COUNT = 5;

const double EPSILON = 1e-6;

class SearchServer {

public:

    SearchServer() = default;

    explicit SearchServer(const std::string& stop_words_text);

    template<typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {

        using namespace std;

        for (const string& word : stop_words_) {
            if (!IsValidText(word))
                throw invalid_argument("Stop word contains special characters"s);
            if (ContainsDoubleDash(word))
                throw invalid_argument("Stop word contains double dash"s);
        }
    }

    size_t GetDocumentCount() const;

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    void SetStopWords(const std::string& text);

    template <typename Requirement>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, Requirement requirement) const {

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

    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus required_status) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    int GetDocumentId(int index) const;

private:

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    struct DocumentData
    {
        DocumentStatus status;
        int rating;
    };

    // word -> document_id -> TF
    std::map<std::string, std::map<int, double>> word_to_documents_freqs_;

    // соответствие id -> status, rating
    std::map<int, DocumentData> documents_data_;

    std::set<std::string> stop_words_;

    size_t documents_count_ = 0;

    // хранит id документов в порядке добавления пользователем;
    std::vector<int> added_documents_id_;

    bool IsStopWord(const std::string& word) const;

    // преобразует строку в вектор слов, проводя их к нижнему регистру и игнорируя стоп-слова
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    template <typename Requirement>
    std::vector<Document> FindAllDocuments(const Query& parsed_query, Requirement requirement) const {

        using namespace std;

        vector<Document> matched_documents;
        map<int, double> document_term_freq_idf_relevance; // [id, relevance]

        for (const string& plus_word : parsed_query.plus_words) {
            if (word_to_documents_freqs_.count(plus_word)) {
                double word_idf = log(static_cast<double>(documents_count_) / word_to_documents_freqs_.at(plus_word).size());
                for (const auto& [document_id, term_freq] : word_to_documents_freqs_.at(plus_word)) {
                    const auto& current_document_data = documents_data_.at(document_id);
                    if (requirement(document_id, current_document_data.status, current_document_data.rating))
                        document_term_freq_idf_relevance[document_id] += word_idf * term_freq;
                }
            }
        }
        for (string minus_word : parsed_query.minus_words) {
            if (word_to_documents_freqs_.count(minus_word)) {
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

    // переданная строка начинается с '-'
    static bool MatchedAsMinusWord(const std::string& word);

    //преобразует строку-запрос в структуру {плюс-слов, минус-слов}
    Query ParseQuery(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    int GetDocumentRating(int document_id) const;
};

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);





// Тестирование ==================================================================================

void AssertImpl(bool value, const std::string& value_str, const std::string& hint, const std::string& file,
    uint32_t line, const std::string& function);

#define ASSERT(a) AssertImpl((a), #a, ""s, __FILE__, __LINE__, __FUNCTION__)

#define ASSERT_HINT(a, hint) AssertImpl((a), #a, (hint), __FILE__, __LINE__, __FUNCTION__)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {

    using namespace std;

    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T& func, const std::string& func_name) {

    using namespace std;
    func();
    //cerr << func_name << " OK"s << endl;
    cout << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func);

// Тест проверяет, что после добавления документ можно найти корректным запросом, пустой документ не находится, пустой запрос не находит документ
void TestAddedDocumentCanBeFoundWithCorrectQuery();

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();

// Тест проверяет, что документы, содержащие минус-слова, исключаются из результатов поиска
void TestExcludeDocumentsWithMinusWordsFromResult();

/* Тест проверяет, что при матчинге документа возвращаются все слова из запроса, присутствующие в документе
При наличии минус-слова возвращается пустой список слов */
void TestMatchDocumentReturnsCorrectWords();

/* Тест проверяет, что найденные документы упорядочены по убыванию релевантности
  Если релевантность совпадает, сортировка по рейтингу */
void TestFoundDocumentsSortedByDescending();

/* Тест проверяет, что рейтинг добавленного документа равен среднему арифметическому оценок
  Если оценок нет, рейтинг равен нулю */
void TestComputeRatingOfAddedDocument();

// Тест проверяет, что корректно осуществляется поиск документов с заданным статусом
void TestFindDocumentsWithSpecificStatus();

// Тест проверяет поиск документов по заданному пользовательским предикатом фильтру
void TestFindDocumentsWithUserPredicate();

// Тест проверяет правильность расчета релевантности по TF-IDF
void TestComputeRelevanceTermFreqIdf();

void TestSearchServerStringConstructor();

void TestSearchServerStringCollectionConstructor();

void TestSearchServer();
