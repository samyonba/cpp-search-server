#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>
#include <numeric>
#include <sstream>
#include <cassert>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<int> ReadRatingsLine() {
    vector<int> ratings;
    string rawInput;
    stringstream ss;

    getline(cin, rawInput);
    ss << rawInput;

    size_t ratings_count = 0u;
    int val;
    ss >> ratings_count;

    for (size_t i = 0; i < ratings_count; i++)
    {
        ss >> val;
        ratings.push_back(val);
    }

    return ratings;
}

vector<string> SplitIntoWordsToLower(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += tolower(c);
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

class SearchServer {

public:

    size_t GetDocumentCount() const {
        return document_count_;
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {

        ++document_count_;

        documents_data_[document_id] = { status, ComputeAverageRating(ratings) };

        vector<string> document_words = SplitIntoWordsNoStopToLower(document);
        for (const string& word : document_words) {
            double word_term_freq = static_cast<double>(count(document_words.begin(), document_words.end(), word)) / document_words.size();
            word_to_documents_freqs_[word].insert({ document_id, word_term_freq });
        }
    }

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWordsToLower(text)) {
            stop_words_.insert(word);
        }
    }

    template <typename Requirement>
    vector<Document> FindTopDocuments(const string& raw_query, Requirement requirement) const {

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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus required_status) const {

        return FindTopDocuments(raw_query,
            [required_status](int document_id, DocumentStatus doc_status, int rating) {return doc_status == required_status; });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) {return status == DocumentStatus::ACTUAL; });
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        set<string> matched_plus_words;
        Query parsed_query = ParseQuery(raw_query);

        for (const string& plus_word : parsed_query.plus_words) {
            if (word_to_documents_freqs_.count(plus_word)) {
                if (word_to_documents_freqs_.at(plus_word).count(document_id)) {
                    matched_plus_words.insert(plus_word);
                }
            }
        }
        for (string minus_word : parsed_query.minus_words) {
            if (word_to_documents_freqs_.count(minus_word)) {
                if (word_to_documents_freqs_.at(minus_word).count(document_id)) {
                    matched_plus_words.clear();
                    break;
                }
            }
        }
        vector<string> matched_plus_words_v(matched_plus_words.begin(), matched_plus_words.end());
        sort(matched_plus_words_v.begin(), matched_plus_words_v.end());
        return { matched_plus_words_v, documents_data_.at(document_id).status };
    };

private:

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    struct DocumentData
    {
        DocumentStatus status;
        int rating;
    };

    map<string, map<int, double>> word_to_documents_freqs_; //<слово, <document_id, TF>>

    map<int, DocumentData> documents_data_;

    set<string> stop_words_;

    size_t document_count_ = 0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStopToLower(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWordsToLower(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    template <typename Requirement>
    vector<Document> FindAllDocuments(const Query& parsed_query, Requirement requirement) const {
        vector<Document> matched_documents;
        map<int, double> document_term_freq_idf_relevance; // [id, relevance]

        for (const string& plus_word : parsed_query.plus_words) {
            if (word_to_documents_freqs_.count(plus_word)) {
                double word_idf = log(static_cast<double>(document_count_) / word_to_documents_freqs_.at(plus_word).size());
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
    static bool MatchedAsMinusWord(const string& word) {
        if (word.empty())
            return false;
        return word.at(0) == '-';
    }

    //преобразует строку-запрос в структуру {плюс-слов, минус-слов}
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWordsNoStopToLower(text)) {
            if (MatchedAsMinusWord(word)) {
                string no_prefix_word = word.substr(1);
                query.minus_words.insert(no_prefix_word);
                query.plus_words.erase(no_prefix_word);
            }
            else if (!query.minus_words.count(word)) {
                query.plus_words.insert(word);
            }
        }
        return query;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty())
            return 0;

        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    }

    int GetDocumentRating(int document_id) const {
        if (documents_data_.count(document_id)) {
            return documents_data_.at(document_id).rating;
        }

        return 0;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

template <typename T>
ostream& operator<< (ostream& os, set<T> container) {
    bool is_first = true;
    os << '{';
    for (const auto& elem : container) {
        if (!is_first) {
            os << ", "s << elem;
        }
        else
            os << elem;
        is_first = false;
    }
    os << '}';
    return os;
}

template <typename K, typename V>
ostream& operator<< (ostream& os, map<K, V> container) {
    bool is_first = true;
    os << '{';
    for (const auto& [key, value] : container) {
        if (!is_first) {
            os << ", "s << key << ": "s << value;
        }
        else
            os << key << ": "s << value;
        is_first = false;
    }
    os << '}';
    return os;
}

void AssertImpl(bool value, const string& value_str, const string& hint, const string& file, uint32_t line, const string& function) {
    // Реализуйте тело функции Assert

    if (!value) {
        cout << file << '(' << line << ')' << ": "s << function << ": ASSERT("s
            << value_str << ") failed."s;
        if (!hint.empty())
            cout << " Hint: "s << hint;
        cout << endl;
        abort();
    }
}

#define ASSERT(a) AssertImpl((a), #a, ""s, __FILE__, __LINE__, __FUNCTION__)

#define ASSERT_HINT(a, hint) AssertImpl((a), #a, (hint), __FILE__, __LINE__, __FUNCTION__)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
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
void RunTestImpl(const T& func, const string& func_name) {
    func();
    //cerr << func_name << " OK"s << endl;
    cout << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func);

// Тест проверяет, что после добавления документ можно найти корректным запросом, пустой документ не находится, пустой запрос не находит документ
void TestAddedDocumentCanBeFoundWithCorrectQuery() {

    const int document_id = 115;
    const string content = "cute brown dog on the red square"s;
    const vector<int> ratings = { 4, 3, 3, 5 };

    // добавленный документ находится корректным запросом и не находится неподходящим запросом
    {
        SearchServer server;
        ASSERT_EQUAL(server.GetDocumentCount(), 0u);
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1u);

        auto found_docs = server.FindTopDocuments("black cat"s);
        ASSERT_EQUAL(found_docs.size(), 0u);

        found_docs = server.FindTopDocuments("brown dog"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT(found_docs.front().id == document_id);
    }

    // пустой документ не находится
    {
        SearchServer server;
        server.AddDocument(999, ""s, DocumentStatus::ACTUAL, { 5 });

        const auto found_docs = server.FindTopDocuments("black cat"s);
        ASSERT_EQUAL(found_docs.size(), 0u);
    }

    // документ не находится пустым запросом
    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments(""s);
        ASSERT_EQUAL(found_docs.size(), 0u);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

// Тест проверяет, что документы, содержащие минус-слова, исключаются из результатов поиска
void TestExcludeDocumentsWithMinusWordsFromResult() {
    const int doc_id = 42;
    const string content = "cute brown dog on the red square"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("brown dog -black"s).size(), 1u);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("brown dog -cute"s).size(), 0u);
    }

    {
        SearchServer server;
        server.SetStopWords("on the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("brown dog -the"s).size(), 1u);
    }
}

/* Тест проверяет, что при матчинге документа возвращаются все слова из запроса, присутствующие в документе
При наличии минус-слова возвращается пустой список слов */
void TestMatchDocumentReturnsCorrectWords() {
    const int doc_id_1 = 42;
    const string content_1 = "cute brown dog on the Red square"s;
    const vector<int> ratings_1 = { 1, 2, 3 };

    const int doc_id_2 = 43;
    const string content_2 = "cat on the road"s;
    const vector<int> ratings_2 = { 2 };

    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const string query = "brown fluffy dog on square"s;

        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_1);
            ASSERT_EQUAL(matched_words.size(), 4u);
            ASSERT_EQUAL(matched_words[0], "brown"s);
            ASSERT_EQUAL(matched_words[1], "dog"s);
            ASSERT_EQUAL(matched_words[2], "on"s);
            ASSERT_EQUAL(matched_words[3], "square"s);
            ASSERT(matched_document_status == DocumentStatus::ACTUAL);
        }

        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_2);
            ASSERT_EQUAL(matched_words.size(), 1u);
            ASSERT_EQUAL(matched_words[0], "on"s);
            ASSERT(matched_document_status == DocumentStatus::ACTUAL);
        }
    }

    // проверяет, что стоп-слова не попадают в результат матчинга
    {
        SearchServer server;
        server.SetStopWords("on the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const string query = "brown fluffy dog on square"s;

        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_1);
            ASSERT_EQUAL(matched_words.size(), 3u);
            ASSERT_EQUAL(matched_words[0], "brown"s);
            ASSERT_EQUAL(matched_words[1], "dog"s);
            ASSERT_EQUAL(matched_words[2], "square"s);
        }

        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_2);
            ASSERT_EQUAL(matched_words.size(), 0u);
        }

    }

    
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const string query = "brown fluffy dog on square -black -cat"s;

        // по указанному запросу должен выводиться список из 4 слов для первого документа и пустой для второго
        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_1);
            ASSERT_EQUAL(matched_words.size(), 4u);
            ASSERT_EQUAL(matched_words[0], "brown"s);
            ASSERT_EQUAL(matched_words[1], "dog"s);
            ASSERT_EQUAL(matched_words[2], "on"s);
            ASSERT_EQUAL(matched_words[3], "square"s);
        }

        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_2);
            ASSERT_EQUAL(matched_words.size(), 0u);
        }
    }
}

/* Тест проверяет, что найденные документы упорядочены по убыванию релевантности
  Если релевантность совпадает, сортировка по рейтингу */
void TestFoundDocumentsSortedByDescending() {
    {
        SearchServer server;
        server.AddDocument(1, "one two three four five"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(2, "one two three a b"s, DocumentStatus::ACTUAL, { 2 });
        server.AddDocument(3, "one two three x y"s, DocumentStatus::ACTUAL, { 3 });
        server.AddDocument(4, "no matched words here at all"s, DocumentStatus::ACTUAL, { 4 });

        const auto found_documents = server.FindTopDocuments("five four three two one zero"s);
        ASSERT_EQUAL(found_documents.size(), 3u);
        ASSERT(found_documents[0].relevance > found_documents[1].relevance
            || ((found_documents[0].relevance == found_documents[1].relevance) && (found_documents[0].rating >= found_documents[1].rating)));
        ASSERT(found_documents[1].relevance > found_documents[2].relevance
            || ((found_documents[1].relevance == found_documents[2].relevance) && (found_documents[1].rating >= found_documents[2].rating)));
    }
}

/* Тест проверяет, что рейтинг добавленного документа равен среднему арифметическому оценок
  Если оценок нет, рейтинг равен нулю */
void TestComputeRatingOfAddedDocument() {
    const int doc_id = 42;
    const string content = "cute brown dog on the red square"s;

    {
        SearchServer server;
        const vector<int> empty_ratings = {};
        const int default_rating = 0;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, empty_ratings);

        const auto found_documents = server.FindTopDocuments("brown dog on square"s);
        ASSERT_EQUAL(found_documents.size(), 1u);
        ASSERT_EQUAL(found_documents[0].rating, default_rating);
    }

    {
        SearchServer server;
        const vector<int> ratings = { 3, 4, 4, 5 };
        const int calculated_rating = accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size()); // = (3 + 4 + 4 + 5)/4 = 16/4 = 4
        ASSERT(calculated_rating == 4);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_documents = server.FindTopDocuments("brown dog on square"s);
        ASSERT_EQUAL(found_documents.size(), 1u);
        ASSERT_EQUAL(found_documents[0].rating, calculated_rating);
    }

    // рейтинг округляется до ближайшего целого числа
    {
        SearchServer server;
        const vector<int> ratings = { 3, 4, 4, 4 };
        const int calculated_rating = accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size()); // = (3 + 4 + 4 + 4)/4 = 15/4 = (int)3.75 = 3
        ASSERT(calculated_rating == 3);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, { 3, 4, 4, 4 });

        const auto found_documents = server.FindTopDocuments("brown dog on square"s);
        ASSERT_EQUAL(found_documents.size(), 1u);
        ASSERT_EQUAL(found_documents[0].rating, calculated_rating);
    }
}

// Тест проверяет, что корректно осуществляется поиск документов с заданным статусом
void TestFindDocumentsWithSpecificStatus() {

    SearchServer server;
    server.AddDocument(1, "one two three four five"s, DocumentStatus::IRRELEVANT, { 0 });
    server.AddDocument(2, "one two three a b"s, DocumentStatus::ACTUAL, { 2 });
    server.AddDocument(3, "one two three x y"s, DocumentStatus::IRRELEVANT, { 3 });
    server.AddDocument(4, "no matched words here at all"s, DocumentStatus::IRRELEVANT, { 4 });

    {
        const auto found_documents = server.FindTopDocuments("five four three two one zero"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_documents.size(), 2u);
        ASSERT_EQUAL(found_documents[0].id, 1);
        ASSERT_EQUAL(found_documents[1].id, 3);
    }

    // поиск по статусу, не заданном ни для одного из документов
    {
        const auto found_documents = server.FindTopDocuments("five four three two one zero"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_documents.size(), 0u);
    }
}

// Тест проверяет поиск документов по заданному пользовательским предикатом фильтру
void TestFindDocumentsWithUserPredicate() {
    
    SearchServer server;
    server.AddDocument(1, "one two three four five"s, DocumentStatus::IRRELEVANT, { 0 });
    server.AddDocument(2, "one two three a b"s, DocumentStatus::ACTUAL, { 2 });
    server.AddDocument(3, "one two three x y"s, DocumentStatus::IRRELEVANT, { 3 });
    server.AddDocument(4, "no matched words here at all"s, DocumentStatus::IRRELEVANT, { 4 });

    {
        const auto found_documents = server.FindTopDocuments("five four three two one zero"s,
            [](int document_id, DocumentStatus status, int rating) {return rating > 0; });
        ASSERT_EQUAL(found_documents.size(), 2u);
        ASSERT_EQUAL(found_documents[0].id, 3);
        ASSERT_EQUAL(found_documents[1].id, 2);
    }

    {
        const auto found_documents = server.FindTopDocuments("five four three two one zero"s,
            [](int document_id, DocumentStatus status, int rating) { return rating > 0 && status == DocumentStatus::ACTUAL; });
        ASSERT_EQUAL(found_documents.size(), 1u);
        ASSERT_EQUAL(found_documents[0].id, 2);
    }

    {
        const auto found_documents = server.FindTopDocuments("five four three two one zero"s,
            [](int document_id, DocumentStatus status, int rating) {return rating > 0 && status == DocumentStatus::ACTUAL && document_id % 2 != 0; });
        ASSERT_EQUAL(found_documents.size(), 0u);
    }
}

// Тест проверяет правильность расчета релевантности по TF-IDF
void TestComputeRelevanceTermFreqIdf() {
    {
        SearchServer server;
        const double documents_count = 3.0;
        server.AddDocument(1, "white cat fashionable collar"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(2, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 2 });
        server.AddDocument(3, "well-kept dog expressive eyes"s, DocumentStatus::ACTUAL, { 3 });

        const string query = "fluffy well-kept cat"s;

        // Для расчета релевантности по TF-IDF:
        //    1) посчитать IDF всех слов из запроса
        //    2) посчитать TF слов запроса в каждом документе
        //    3) просуммировать для каждого докумета произведения TF и IDF каждого слова

        // 1) IDF - натуральный логарифм от результата деления общего количества документов на кол-во документов, в которых встречается слово
        const int fluffy_containing_documents_count = 1;
        const int well_kept_containing_documents_count = 1;
        const int cat_containing_documents_count = 2;

        const double fluffy_idf = log( documents_count / fluffy_containing_documents_count);
        const double well_kept_idf = log(documents_count / well_kept_containing_documents_count);
        const double cat_idf = log(documents_count / cat_containing_documents_count);

        // 2) TF - количество раз, сколько слово встечается в документе, деленное на общее число слов документа
        double doc_1_words_count = 4;
        double doc_2_words_count = 4;
        double doc_3_words_count = 4;

        double fluffy_doc_1_count = 0;
        double fluffy_doc_2_count = 2;
        double fluffy_doc_3_count = 0;

        double well_kept_doc_1_count = 0;
        double well_kept_doc_2_count = 0;
        double well_kept_doc_3_count = 1;

        double cat_doc_1_count = 1;
        double cat_doc_2_count = 1;
        double cat_doc_3_count = 0;

        vector<double> fluffy_docs_term_freq = { fluffy_doc_1_count / doc_1_words_count, fluffy_doc_2_count / doc_2_words_count, fluffy_doc_3_count / doc_3_words_count }; // fluffy term frequency для первого, второго и третьего документа
        vector<double> well_kept_docs_term_freq = { well_kept_doc_1_count / doc_1_words_count, well_kept_doc_2_count / doc_2_words_count, well_kept_doc_3_count / doc_3_words_count };
        vector<double> cat_docs_term_freq = { cat_doc_1_count / doc_1_words_count, cat_doc_2_count / doc_2_words_count, cat_doc_3_count / doc_3_words_count };

        // 3) TF-IDF - сумма произведений TF и IDF для каждого документа
        double doc_one_result_relevance = fluffy_docs_term_freq[0] * fluffy_idf
            + well_kept_docs_term_freq[0] * well_kept_idf
            + cat_docs_term_freq[0] * cat_idf;

        double doc_two_result_relevance = fluffy_docs_term_freq[1] * fluffy_idf
            + well_kept_docs_term_freq[1] * well_kept_idf
            + cat_docs_term_freq[1] * cat_idf;

        double doc_three_result_relevance = fluffy_docs_term_freq[2] * fluffy_idf
            + well_kept_docs_term_freq[2] * well_kept_idf
            + cat_docs_term_freq[2] * cat_idf;

        const auto found_documents = server.FindTopDocuments(query);

        ASSERT_EQUAL(found_documents.size(), 3u);
        ASSERT_EQUAL(found_documents[0].id, 2);
        ASSERT_EQUAL(found_documents[1].id, 3);
        ASSERT_EQUAL(found_documents[2].id, 1);

        ASSERT(abs(found_documents[0].relevance - doc_two_result_relevance) < EPSILON);
        ASSERT(abs(found_documents[1].relevance - doc_three_result_relevance) < EPSILON);
        ASSERT(abs(found_documents[2].relevance - doc_one_result_relevance) < EPSILON);
    }
}

void TestSearchServer() {
    RUN_TEST(TestAddedDocumentCanBeFoundWithCorrectQuery);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsWithMinusWordsFromResult);
    RUN_TEST(TestMatchDocumentReturnsCorrectWords);
    RUN_TEST(TestFoundDocumentsSortedByDescending);
    RUN_TEST(TestComputeRatingOfAddedDocument);
    RUN_TEST(TestFindDocumentsWithSpecificStatus);
    RUN_TEST(TestFindDocumentsWithUserPredicate);
    RUN_TEST(TestComputeRelevanceTermFreqIdf);
}

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
