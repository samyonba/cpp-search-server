#include "test_example_functions.h"
#include "search_server.h"
#include "remove_duplicates.h"
#include <string>
#include <vector>

using namespace std;

// Тестирование ==================================================================================

void AssertImpl(bool value, const string& value_str, const string& hint, const string& file, uint32_t line, const string& function) {
    if (!value) {
        cerr << file << '(' << line << ')' << ": "s << function << ": ASSERT("s
            << value_str << ") failed."s;
        if (!hint.empty())
            cerr << " Hint: "s << hint;
        cerr << endl;
        abort();
    }
}

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
        ASSERT(found_docs.empty());
    }

    // документ не находится пустым запросом
    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments(""s);
        ASSERT(found_docs.empty());
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
        ASSERT(server.FindTopDocuments("brown dog -cute"s).empty());
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

        // по указанному запросу должен выводиться список из 4 слов для первого документа и пустой для второго
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

        // в данном случае при равной релевантности выполнится сортировка по рейтингу для документов [1] и [2]
        ASSERT(found_documents[1].relevance > found_documents[2].relevance
            || ((found_documents[1].relevance == found_documents[2].relevance) && (found_documents[1].rating >= found_documents[2].rating)));

        ASSERT_EQUAL(found_documents[0].id, 1);
        ASSERT_EQUAL(found_documents[1].id, 3);
        ASSERT_EQUAL(found_documents[2].id, 2);
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
        const int calculated_rating = (3 + 4 + 4 + 5) / 4;
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
        const int calculated_rating = (3 + 4 + 4 + 4) / 4;
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
        ASSERT(found_documents.empty());
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
        ASSERT(found_documents.empty());
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

        const double fluffy_idf = log(documents_count / fluffy_containing_documents_count);
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

void TestSearchServerStringConstructor() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    // инициализация сервера строкой, не содержащей стоп-слов не сказывается на поиске документа
    {
        SearchServer server("   "s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    // инициализация сервера строкой, содержащей стоп-слова приведет к возврату пустого результата
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

    // инициализация сервера пустой коллекцией не сказывается на поиске документа
    {
        const set<string> empty_stop_words;
        SearchServer server(empty_stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    // инициализация сервера коллекцией стоп-слов влияет на результаты поиска
    {
        const vector<string> stop_words_collection{ "on"s, "in"s, "the"s };
        SearchServer server(stop_words_collection);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }

    // то же самое, но коллекция - set
    {
        const set<string> stop_words_collection{ "on"s, "in"s, "the"s };
        SearchServer server(stop_words_collection);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestGetWordFrequencies() {
    SearchServer server;
    server.AddDocument(1, "white cat fashionable collar"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(2, "fluffy cat fluffy"s, DocumentStatus::ACTUAL, { 2 });
    server.AddDocument(3, "well-kept dog"s, DocumentStatus::ACTUAL, { 3 });

    map<string, double> doc1_words_tf = { {"white"s, 1. / 4}, {"cat"s, 1. / 4}, {"fashionable"s, 1. / 4}, {"collar"s, 1. / 4} };
    map<string, double> doc2_words_tf = { {"fluffy"s, 2. / 3}, {"cat"s, 1. / 3} };
    map<string, double> doc3_words_tf = { {"well-kept"s, 1. / 2}, {"dog"s, 1. / 2} };

    ASSERT_EQUAL(server.GetWordFrequencies(1), doc1_words_tf);
    ASSERT_EQUAL(server.GetWordFrequencies(2), doc2_words_tf);
    ASSERT_EQUAL(server.GetWordFrequencies(3), doc3_words_tf);
}

void TestRemoveDocument() {
    SearchServer server;
    server.AddDocument(1, "white cat fashionable collar"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(2, "fluffy cat fluffy"s, DocumentStatus::ACTUAL, { 2 });
    server.AddDocument(3, "well-kept dog"s, DocumentStatus::ACTUAL, { 3 });

    vector<int> correct_id = { 1, 2, 3 };
    int index = 0;
    for (const auto document_id : server) {
        ASSERT_EQUAL(document_id, correct_id[index++]);
    }

    server.RemoveDocument(2);
    correct_id = { 1, 3 };
    index = 0;
    for (const auto document_id : server) {
        ASSERT_EQUAL(document_id, correct_id[index++]);
    }
    ASSERT(server.FindTopDocuments("fluffy"s).empty());
}

// Тест проверяет, корректно ли удаляются дубликаты документов при помощи функции RemoveDuplicates(SearchServer& s)
void TestRemoveDuplicates() {
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // дубликат документа 2, будет удалён
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // отличие только в стоп-словах, считаем дубликатом
    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, считаем дубликатом документа 1
    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // добавились новые слова, дубликатом не является
    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });

    // есть не все слова, не является дубликатом
    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // слова из разных документов, не является дубликатом
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    vector<int> documents_expected_id{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    vector<int> current_documents_id;

    for (const auto document_id : search_server) {
        current_documents_id.push_back(document_id);
    }
    ASSERT_EQUAL(documents_expected_id, current_documents_id);

    RemoveDuplicates(search_server);

    documents_expected_id = { 1, 2, 6, 8, 9 };
    current_documents_id.clear();
    for (const auto document_id : search_server) {
        current_documents_id.push_back(document_id);
    }
    ASSERT_EQUAL(documents_expected_id, current_documents_id);
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
    RUN_TEST(TestGetWordFrequencies);
    RUN_TEST(TestRemoveDocument);
    RUN_TEST(TestRemoveDuplicates);
}