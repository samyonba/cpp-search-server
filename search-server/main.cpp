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

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

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

//vector<int> ReadRatingsLine() {
//    int N;
//    cin >> N;
//    vector<int> ratings;
//    //ratings.reserve(N);
//
//    int val;
//    for (size_t i = 0; i < N; i++)
//    {
//        cin >> val;
//        ratings.push_back(val);
//    }
//    ReadLine();
//    return ratings;
//}

vector<int> ReadRatingsLine() {
    vector<int> ratings;
    string rawInput;
    stringstream ss;

    getline(cin, rawInput);
    ss << rawInput;

    int N;
    int val;
    ss >> N;

    for (size_t i = 0; i < N; i++)
    {
        ss >> val;
        ratings.push_back(val);
    }

    return ratings;
}

vector<string> SplitIntoWords(const string& text) {
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
            word += c;
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

    int GetDocumentCount() {
        return document_count_;
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {

        ++document_count_;

        documents_status_rating_[document_id] = { status, ComputeAverageRating(ratings) };

        vector<string> document_words = SplitIntoWordsNoStop(document);
        for (const string& word : document_words) {
            double word_TF = static_cast<double>(count(document_words.begin(), document_words.end(), word)) / document_words.size();
            word_to_documents_freqs_[word].insert({ document_id, word_TF });
        }
    }

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void SortDocuments(vector<Document>& matched_documents) {
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                return pair(lhs.rating, lhs.relevance) > pair(rhs.rating, rhs.relevance);
            });
    }

    template <typename Requirement>
    vector<Document> FindTopDocuments(const string& raw_query, Requirement requirement) const {

        Query parsed_query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(parsed_query, requirement);

        // копия EPSILON
        auto local_epsilon = EPSILON;

        // с локальной копией все работает
        sort(matched_documents.begin(), matched_documents.end(),
            [local_epsilon](const Document& lhs, const Document& rhs) {
                return lhs.relevance > rhs.relevance ||
                    (abs(lhs.relevance - rhs.relevance) < local_epsilon && lhs.rating > rhs.rating);
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
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) {
            return status == DocumentStatus::ACTUAL; });
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
        return { matched_plus_words_v, documents_status_rating_.at(document_id).status };
    };

private:

    const double EPSILON = 1e-6;

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    map<string, map<int, double>> word_to_documents_freqs_; //<слово, <document_id, TF>>

    struct DocumentData
    {
        DocumentStatus status;
        int rating;
    };

    map<int, DocumentData> documents_status_rating_;

    set<string> stop_words_;

    int document_count_ = 0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    template <typename Requirement>
    vector<Document> FindAllDocuments(const Query& parsed_query, Requirement requirement) const {
        vector<Document> matched_documents;
        map<int, double> document_TF_IDF_relevance; // [id, relevance]

        for (const string& plus_word : parsed_query.plus_words) {
            if (word_to_documents_freqs_.count(plus_word)) {
                double word_IDF = log(static_cast<double>(document_count_) / word_to_documents_freqs_.at(plus_word).size());
                for (const auto& [document_id, TF] : word_to_documents_freqs_.at(plus_word)) {
                    if (requirement(document_id, documents_status_rating_.at(document_id).status, documents_status_rating_.at(document_id).rating))
                        document_TF_IDF_relevance[document_id] += word_IDF * TF;
                }
            }
        }
        for (string minus_word : parsed_query.minus_words) {
            if (word_to_documents_freqs_.count(minus_word)) {
                for (const auto& [document_id, TF] : word_to_documents_freqs_.at(minus_word)) {
                    document_TF_IDF_relevance.erase(document_id);
                }
            }
        }

        for (const auto& [document_id, relevance] : document_TF_IDF_relevance) {
            matched_documents.push_back({ document_id, relevance, GetDocumentRating(document_id) });
        }

        return matched_documents;
    }

    // переданная строка начинается с '-'
    static bool MatchedAsStopWord(const string& word) {
        if (word.empty())
            return false;
        return word.at(0) == '-';
    }

    //преобразует строку-запрос в структуру {плюс-слов, минус-слов}
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (MatchedAsStopWord(word)) {
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
        if (documents_status_rating_.count(document_id)) {
            return documents_status_rating_.at(document_id).rating;
        }

        return 0;
    }
};

// Считывает из cin стоп-слова и документ и возвращает настроенный экземпляр поисковой системы
//SearchServer CreateSearchServer() {
//    SearchServer search_server;
//
//    search_server.SetStopWords(ReadLine());
//
//    int N = ReadLineWithNumber();
//    for (size_t document_id = 0; document_id < N; document_id++)
//    {
//        string document_content = ReadLine();
//        vector<int> ratings = ReadRatingsLine();
//        search_server.AddDocument(document_id, document_content, ratings);
//        //search_server.AddDocument(document_id, ReadLine(), ReadRatingsLine());
//    }
//
//    return search_server;
//}

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

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}
