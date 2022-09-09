#include "search_server.h"
#include <algorithm>
#include <numeric>
#include <cmath>

using namespace std;

SearchServer::SearchServer(const std::string& stop_words_text) {
    if (!IsValidText(stop_words_text))
        throw invalid_argument("Stop words passed when creating SearchServer contain special characters"s);
    if (ContainsDoubleDash(stop_words_text))
        throw invalid_argument("Stop words passed when creating SearchServer contain double dash"s);
    SetStopWords(stop_words_text);
}

size_t SearchServer::GetDocumentCount() const {
    return documents_data_.size();
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const vector<int>& ratings) {
    // ������� �������� �������� � ������������� id
    if (document_id < 0)
        throw invalid_argument("Document id is negative"s);
    // ������� �������� �������� � id, ����������� � id ���������, ������� ��������� �����
    if (documents_data_.count(document_id))
        throw invalid_argument("Given document id already matches to previous added document"s);
    // ����� ��������� �������� �����������
    if (!IsValidText(document))
        throw invalid_argument("Document text contains special characters"s);

    vector<string> document_words = SplitIntoWordsNoStop(document);

    // ����� ��������� �� ������ ��������� ����, ������������ � '-'
    if (any_of(document_words.begin(), document_words.end(), MatchedAsMinusWord))
        throw invalid_argument("Document text contains a word starting with minus"s);

    ++documents_count_;
    added_documents_id_.push_back(document_id);
    documents_data_[document_id] = { status, ComputeAverageRating(ratings) };
    for (const string& word : document_words) {
        double word_term_freq = static_cast<double>(count(document_words.begin(), document_words.end(), word)) / document_words.size();
        word_to_documents_freqs_[word].insert({ document_id, word_term_freq });
    }
}

void SearchServer::SetStopWords(const string& text) {
    for (const string& word : SplitIntoWords(text)) {
        stop_words_.insert(word);
    }
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus required_status) const {

    return FindTopDocuments(raw_query,
        [required_status](int document_id, DocumentStatus doc_status, int rating) {return doc_status == required_status; });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(raw_query,
        [](int document_id, DocumentStatus status, int rating) {return status == DocumentStatus::ACTUAL; });
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    // ������� �������� ��������� �� ��������������� id
    if (!documents_data_.count(document_id))
        throw invalid_argument("No document with given id"s);

    set<string> matched_plus_words;

    // throws invalid_argument exception
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
}

int SearchServer::GetDocumentId(int index) const {
    if (index < 0 ||
        index > static_cast<int>(documents_count_)) {
        throw out_of_range("Given document index is out of bounds"s);
    }

    return added_documents_id_[index];
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

// ���������� ������ ���������� � '-'
bool SearchServer::MatchedAsMinusWord(const string& word) {
    if (word.empty())
        return false;
    return word.at(0) == '-';
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    // ������ �� ������ ��������� ����, ���������� ����� ������ '-' ������
    if (ContainsDoubleDash(text))
        throw invalid_argument("Query contains double dash"s);
    // ������ �� ������ ��������� ������������
    if (!IsValidText(text))
        throw invalid_argument("Query contains special characters"s);

    Query query;
    for (const string& word : SplitIntoWordsNoStop(text)) {
        if (MatchedAsMinusWord(word)) {
            // ����� ������� '-' ���������� ����� ����� �����
            if (word.size() == 1) {
                throw invalid_argument("Text is missing after minus in query"s);
            }
            string no_prefix_word = word.substr(1);
            query.minus_words.insert(no_prefix_word);
            // ���� ����� ����� ����� ����������� ��� plus_word, ������ ��� ����� �������
            query.plus_words.erase(no_prefix_word);
        }
        else if (!query.minus_words.count(word)) {
            query.plus_words.insert(word);
        }
    }
    return query;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty())
        return 0;

    return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

int SearchServer::GetDocumentRating(int document_id) const {
    if (documents_data_.count(document_id)) {
        return documents_data_.at(document_id).rating;
    }

    return 0;
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

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
    const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const exception& e) {
        cout << "������ ���������� ��������� "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "���������� ������ �� �������: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const exception& e) {
        cout << "������ ������: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "������� ���������� �� �������: "s << query << endl;
        const size_t document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& e) {
        cout << "������ �������� ���������� �� ������ "s << query << ": "s << e.what() << endl;
    }
}





// ������������ ==================================================================================

void AssertImpl(bool value, const string& value_str, const string& hint, const string& file, uint32_t line, const string& function) {
    if (!value) {
        cout << file << '(' << line << ')' << ": "s << function << ": ASSERT("s
            << value_str << ") failed."s;
        if (!hint.empty())
            cout << " Hint: "s << hint;
        cout << endl;
        abort();
    }
}

// ���� ���������, ��� ����� ���������� �������� ����� ����� ���������� ��������, ������ �������� �� ���������, ������ ������ �� ������� ��������
void TestAddedDocumentCanBeFoundWithCorrectQuery() {

    const int document_id = 115;
    const string content = "cute brown dog on the red square"s;
    const vector<int> ratings = { 4, 3, 3, 5 };

    // ����������� �������� ��������� ���������� �������� � �� ��������� ������������ ��������
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

    // ������ �������� �� ���������
    {
        SearchServer server;
        server.AddDocument(999, ""s, DocumentStatus::ACTUAL, { 5 });

        const auto found_docs = server.FindTopDocuments("black cat"s);
        ASSERT(found_docs.empty());
    }

    // �������� �� ��������� ������ ��������
    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments(""s);
        ASSERT(found_docs.empty());
    }
}

// ���� ���������, ��� ��������� ������� ��������� ����-����� ��� ���������� ����������
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // ������� ����������, ��� ����� �����, �� ��������� � ������ ����-����,
    // ������� ������ ��������
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    // ����� ����������, ��� ����� ����� �� �����, ��������� � ������ ����-����,
    // ���������� ������ ���������
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

// ���� ���������, ��� ���������, ���������� �����-�����, ����������� �� ����������� ������
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
        ASSERT(server.FindTopDocuments("brown dog -cute"s).empty());
    }

    {
        SearchServer server;
        server.SetStopWords("on the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("brown dog -the"s).size(), 1u);
    }
}

/* ���� ���������, ��� ��� �������� ��������� ������������ ��� ����� �� �������, �������������� � ���������
��� ������� �����-����� ������������ ������ ������ ���� */
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
            const vector<string> expected_result = { "brown"s, "dog"s, "on"s, "square"s };
            ASSERT_EQUAL(matched_words, expected_result);
            ASSERT(matched_document_status == DocumentStatus::ACTUAL);
        }

        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_2);
            ASSERT_EQUAL(matched_words.size(), 1u);
            ASSERT_EQUAL(matched_words[0], "on"s);
            ASSERT(matched_document_status == DocumentStatus::ACTUAL);
        }
    }

    // ���������, ��� ����-����� �� �������� � ��������� ��������
    {
        SearchServer server;
        server.SetStopWords("on the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const string query = "brown fluffy dog on square"s;

        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_1);
            ASSERT_EQUAL(matched_words.size(), 3u);
            const vector<string> expected_result = { "brown"s, "dog"s, "square"s };
            ASSERT_EQUAL(matched_words, expected_result);
        }

        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_2);
            ASSERT(matched_words.empty());
        }
    }

    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const string query = "brown fluffy dog on square -black -cat"s;

        // �� ���������� ������� ������ ���������� ������ �� 4 ���� ��� ������� ��������� � ������ ��� �������
        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_1);
            ASSERT_EQUAL(matched_words.size(), 4u);
            const vector<string> expected_result = { "brown"s, "dog"s, "on"s, "square"s };
            ASSERT_EQUAL(matched_words, expected_result);
        }

        {
            const auto& [matched_words, matched_document_status] = server.MatchDocument(query, doc_id_2);
            ASSERT(matched_words.empty());
        }
    }
}

/* ���� ���������, ��� ��������� ��������� ����������� �� �������� �������������
  ���� ������������� ���������, ���������� �� �������� */
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
        // � ������ ������ ��� ������ ������������� ���������� ���������� �� �������� ��� ���������� [1] � [2]
        ASSERT(found_documents[1].relevance > found_documents[2].relevance
            || ((found_documents[1].relevance == found_documents[2].relevance) && (found_documents[1].rating >= found_documents[2].rating)));

        ASSERT_EQUAL(found_documents[0].id, 1);
        ASSERT_EQUAL(found_documents[1].id, 3);
        ASSERT_EQUAL(found_documents[2].id, 2);
    }
}

/* ���� ���������, ��� ������� ������������ ��������� ����� �������� ��������������� ������
  ���� ������ ���, ������� ����� ���� */
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
        const int calculated_rating = (3 + 4 + 4 + 5) / 4;
        ASSERT(calculated_rating == 4);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_documents = server.FindTopDocuments("brown dog on square"s);
        ASSERT_EQUAL(found_documents.size(), 1u);
        ASSERT_EQUAL(found_documents[0].rating, calculated_rating);
    }

    // ������� ����������� �� ���������� ������ �����
    {
        SearchServer server;
        const vector<int> ratings = { 3, 4, 4, 4 };
        const int calculated_rating = (3 + 4 + 4 + 4) / 4;
        ASSERT(calculated_rating == 3);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, { 3, 4, 4, 4 });

        const auto found_documents = server.FindTopDocuments("brown dog on square"s);
        ASSERT_EQUAL(found_documents.size(), 1u);
        ASSERT_EQUAL(found_documents[0].rating, calculated_rating);
    }
}

// ���� ���������, ��� ��������� �������������� ����� ���������� � �������� ��������
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

    // ����� �� �������, �� �������� �� ��� ������ �� ����������
    {
        const auto found_documents = server.FindTopDocuments("five four three two one zero"s, DocumentStatus::BANNED);
        ASSERT(found_documents.empty());
    }
}

// ���� ��������� ����� ���������� �� ��������� ���������������� ���������� �������
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
        ASSERT(found_documents.empty());
    }
}

// ���� ��������� ������������ ������� ������������� �� TF-IDF
void TestComputeRelevanceTermFreqIdf() {
    {
        SearchServer server;
        const double documents_count = 3.0;
        server.AddDocument(1, "white cat fashionable collar"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(2, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 2 });
        server.AddDocument(3, "well-kept dog expressive eyes"s, DocumentStatus::ACTUAL, { 3 });

        const string query = "fluffy well-kept cat"s;

        // ��� ������� ������������� �� TF-IDF:
        //    1) ��������� IDF ���� ���� �� �������
        //    2) ��������� TF ���� ������� � ������ ���������
        //    3) �������������� ��� ������� �������� ������������ TF � IDF ������� �����

        // 1) IDF - ����������� �������� �� ���������� ������� ������ ���������� ���������� �� ���-�� ����������, � ������� ����������� �����
        const int fluffy_containing_documents_count = 1;
        const int well_kept_containing_documents_count = 1;
        const int cat_containing_documents_count = 2;

        const double fluffy_idf = log(documents_count / fluffy_containing_documents_count);
        const double well_kept_idf = log(documents_count / well_kept_containing_documents_count);
        const double cat_idf = log(documents_count / cat_containing_documents_count);

        // 2) TF - ���������� ���, ������� ����� ���������� � ���������, �������� �� ����� ����� ���� ���������
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

        vector<double> fluffy_docs_term_freq = { fluffy_doc_1_count / doc_1_words_count, fluffy_doc_2_count / doc_2_words_count, fluffy_doc_3_count / doc_3_words_count }; // fluffy term frequency ��� �������, ������� � �������� ���������
        vector<double> well_kept_docs_term_freq = { well_kept_doc_1_count / doc_1_words_count, well_kept_doc_2_count / doc_2_words_count, well_kept_doc_3_count / doc_3_words_count };
        vector<double> cat_docs_term_freq = { cat_doc_1_count / doc_1_words_count, cat_doc_2_count / doc_2_words_count, cat_doc_3_count / doc_3_words_count };

        // 3) TF-IDF - ����� ������������ TF � IDF ��� ������� ���������
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

void TestSearchServerStringConstructor() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    // ������������� ������� �������, �� ���������� ����-���� �� ����������� �� ������ ���������
    {
        SearchServer server("   "s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    // ������������� ������� �������, ���������� ����-����� �������� � �������� ������� ����������
    {
        SearchServer server("on in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestSearchServerStringCollectionConstructor() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    // ������������� ������� ������ ���������� �� ����������� �� ������ ���������
    {
        const set<string> empty_stop_words;
        SearchServer server(empty_stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    // ������������� ������� ���������� ����-���� ������ �� ���������� ������
    {
        const vector<string> stop_words_collection{ "on"s, "in"s, "the"s };
        SearchServer server(stop_words_collection);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }

    // �� �� �����, �� ��������� - set
    {
        const set<string> stop_words_collection{ "on"s, "in"s, "the"s };
        SearchServer server(stop_words_collection);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
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
    RUN_TEST(TestSearchServerStringConstructor);
    RUN_TEST(TestSearchServerStringCollectionConstructor);
}
