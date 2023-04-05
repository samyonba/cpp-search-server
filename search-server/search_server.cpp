#include "search_server.h"
#include "process_queries.h"
#include "document.h"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <execution>
#include <exception>
#include <string>

using namespace std;

SearchServer::SearchServer(const std::string& stop_words_text) {
	if (!IsValidText(stop_words_text))
	{
		throw invalid_argument("Stop words passed when creating SearchServer contain special characters"s);
	}
	
	SetStopWords(stop_words_text);
}

SearchServer::SearchServer(const std::string_view stop_words_sv)
{
    if (!IsValidText(stop_words_sv))
    {
        throw invalid_argument("Stop words passed when creating SearchServer contain special characters"s);
    }

    SetStopWords(stop_words_sv);
}

size_t SearchServer::GetDocumentCount() const {
    return documents_data_.size();
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const vector<int>& ratings) {

    // попытка добавить документ с отрицаиельным id
    if (document_id < 0)
    {
        throw invalid_argument("Document id is negative"s);
    }

    // попытка добавить документ с id, совпадающим с id документа,добавленного ранее
    if (added_documents_id_.count(document_id))
    {
        throw invalid_argument("Given document id already matches to previous added document"s);
    }

    // текст документа содержит спецсимволы
    if (!IsValidText(document))
    {
        throw invalid_argument("Document text contains special characters"s);
    }

    vector<string_view> document_words = SplitIntoWordsNoStop(document);

    vector<set<string>::iterator> doc_words_ptrs;

    for (const auto& word : document_words) {
        words_data_.insert(static_cast<string>(word));
        doc_words_ptrs.push_back(words_data_.find(word));
    }

    added_documents_id_.insert(document_id);
    auto document_rating = ComputeAverageRating(ratings);
    documents_data_[document_id] = { status, document_rating };

    double freq_increase = 1. / document_words.size();

    for (const auto& word_ptr : doc_words_ptrs) {
        word_to_documents_freqs_[*word_ptr][document_id] += freq_increase;
        document_to_words_freqs_[document_id][*word_ptr] += freq_increase;
    }
}

void SearchServer::SetStopWords(const string_view text) {
    for (const string_view word : SplitIntoWords(text))
    {
        stop_words_.insert(static_cast<string>(word));
    }
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus required_status) const {

	return FindTopDocuments(raw_query,
		[required_status](int document_id, DocumentStatus doc_status, int rating)
		{return doc_status == required_status; });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, DocumentStatus required_status) const
{
    return FindTopDocuments(raw_query, required_status);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, DocumentStatus required_status) const
{
    return FindTopDocuments(std::execution::par, raw_query,
        [required_status](int document_id, DocumentStatus doc_status, int rating)
        {return doc_status == required_status; });
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query) const
{
    return FindTopDocuments(raw_query);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query) const
{
    return FindTopDocuments(std::execution::par, raw_query, DocumentStatus::ACTUAL);
}

// Метод возвращает вектор всех плюс-слов запроса, содержащихся в документе и статус документа
// Если документ не соответствует запросу (нет пересечений по плюс-словам или есть минус-слово), возвращает пустой вектор слов и статус документа
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    // попытка матчинга документа по несуществующему id
	if (!documents_data_.count(document_id))
    {
        throw out_of_range("No document with given id"s);
    }

	vector<string_view> matched_plus_words;

	Query parsed_query = ParseQuery(raw_query);

    const auto& words_freqs = GetWordFrequencies(document_id);

    for (const string_view word : parsed_query.minus_words) {
        if (words_freqs.count(static_cast<string>(word))) {
            return { {}, documents_data_.at(document_id).status };
        }
    }

	for (const string_view word : parsed_query.plus_words) {
        if (words_freqs.count(static_cast<string>(word))) {
            matched_plus_words.push_back(word);
        }
	}

	return { matched_plus_words, documents_data_.at(document_id).status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument
(const std::execution::sequenced_policy&, const string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument
(const std::execution::parallel_policy&, const string_view raw_query, int document_id) const {
    // попытка матчинга документа по несуществующему id
    if (!documents_data_.count(document_id))
    {
        throw out_of_range("No document with given id"s);
    }

    Query parsed_query = ParseQuery(raw_query, false);

    const auto& words_freqs = GetWordFrequencies(document_id);

    auto contains_minus_word = any_of(execution::par, parsed_query.minus_words.begin(), parsed_query.minus_words.end(),
        [&](const string_view word) {
            return words_freqs.count(static_cast<string>(word));
        });

    if (contains_minus_word) {
        return { {}, documents_data_.at(document_id).status };
    }

    vector<string_view> matched_plus_words;
    matched_plus_words.resize(parsed_query.plus_words.size());
    auto last = copy_if(execution::par, parsed_query.plus_words.begin(), parsed_query.plus_words.end(), matched_plus_words.begin(),
        [&](const string_view word) {
            return words_freqs.count(static_cast<string>(word));
        });

    std::sort(matched_plus_words.begin(), last);
    auto last_2 = std::unique(matched_plus_words.begin(), last);
    matched_plus_words.resize(distance(matched_plus_words.begin(), last_2));
    //matched_plus_words.erase(last_2, matched_plus_words.end());
    
    return { matched_plus_words, documents_data_.at(document_id).status };
}

std::set<int>::const_iterator SearchServer::begin() const {
    return added_documents_id_.cbegin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return added_documents_id_.cend();
}

// получение частот слов по id документа
const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    static const map<string_view, double> NULL_RESULT;

    if (document_to_words_freqs_.count(document_id))
    {
        return document_to_words_freqs_.at(document_id);
    }
    
    return NULL_RESULT;
}

void SearchServer::RemoveDocument(int document_id)
{
    if (!added_documents_id_.count(document_id))
    {
        return;
    }

    // удаляем id документа
    added_documents_id_.erase(document_id);

    // удаляем данные о статусе и рейтинге документа
    documents_data_.erase(documents_data_.find(document_id));
    
    // проходим по всем словам документа, удаляя id документа из соответствующих записей в word_to_documents_freqs_
    for (auto& [word, term_freq] : document_to_words_freqs_[document_id]) {
        word_to_documents_freqs_[word].erase(document_id);
    }

    // только после этого можно удалить данные о словах в документе
    document_to_words_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id)
{
    RemoveDocument(document_id);
}

//void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id)
//{
//    if (!added_documents_id_.count(document_id))
//    {
//        return;
//    }
//
//    added_documents_id_.erase(document_id);
//
//    documents_data_.erase(documents_data_.find(document_id));
//
//    std::vector<const std::string*> keys;
//
//    for (auto& elm : document_to_words_freqs_[document_id]) {
//        keys.push_back(&elm.first);
//    }
//
//    std::for_each(std::execution::par,
//        keys.begin(),
//        keys.end(),
//        [&](const std::string* key)
//        {
//            if (word_to_documents_freqs_.count(*key) > 0)
//            {
//                word_to_documents_freqs_[*key].erase(document_id);
//            }
//        });
//
//    document_to_words_freqs_.erase(document_id);
//}

void SearchServer::RemoveDocument(const std::execution::parallel_policy& policy, int document_id)
{

    if (!added_documents_id_.count(document_id))
    {
        return;
    }

    // удаляем id документа
    added_documents_id_.erase(document_id);

    // удаляем данные о статусе и рейтинге документа
    documents_data_.erase(documents_data_.find(document_id));

    std::vector<std::string_view> keys;
    //words_ptr.reserve(document_to_words_freqs_[document_id].size());
    keys.resize(document_to_words_freqs_[document_id].size());

    std::transform(std::execution::par,
        document_to_words_freqs_[document_id].begin(), document_to_words_freqs_[document_id].end(), keys.begin(),
        [](const std::pair<const std::string_view, double>& p) -> std::string_view {
            return p.first;
        });

	std::for_each(std::execution::par,
		keys.begin(),
		keys.end(),
		[&](const std::string_view key)
		{
			if (word_to_documents_freqs_.count(key) > 0)
			{
				word_to_documents_freqs_[key].erase(document_id);
			}
		});

    // только после этого можно удалить данные о словах в документе
    document_to_words_freqs_.erase(document_id);
}

bool SearchServer::IsStopWord(const string_view word) const {
    return (stop_words_.count(word) > 0);
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    vector<string_view> words;

    auto pos = text.find_first_not_of(' ');
    auto pos_end = text.npos;

    while (pos != pos_end)
    {
        auto space = text.find(' ', pos);
        string_view word = space == pos_end ? text.substr(pos) : text.substr(pos, space - pos);
        if (!IsStopWord(word))
        {
            words.push_back(word);
        }
        pos = text.find_first_not_of(' ', space);
    }

    return words;
}

bool SearchServer::IsValidText(const string_view text) const {
    return none_of(text.begin(), text.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

// переданная строка начинается с '-'
bool SearchServer::MatchedAsMinusWord(const string_view word) {
    if (word.empty())
    {
        return false;
    }

    return (word[0] == '-');
}

// with std::vector Query
void SearchServer::ParseQueryWord(const string_view word, Query& query) const {

    // запрос не должен содержать спецсимволов
    if (!IsValidText(word))
    {
        throw invalid_argument("Query contains special characters"s);
    }

    if (MatchedAsMinusWord(word))
    {

        // после символа '-' отсутствет текст минус слова
        if (word.size() == 1)
        {
            throw invalid_argument("Text is missing after minus in query"s);
        }

        string_view no_prefix_word = word.substr(1);

        // если после удаления из начала слова символа '-', в начале слова всё ещё стоит '-', запрос построен неверно
        if (MatchedAsMinusWord(no_prefix_word))
        {
            throw invalid_argument("Query contains double dash"s);
        }

        if (!IsStopWord(no_prefix_word))
        {
            query.minus_words.push_back(no_prefix_word);
        }
    }
    else if (!IsStopWord(word))
    {
        query.plus_words.push_back(word);
    }
}

// преобразует строку-запрос в структуру из плюс и минус слов
// по умолчанию дубликаты удаляются, для использования в параллельных методах от удаления
// дубликатов можно отказаться, передав erase_duplicates = false
SearchServer::Query SearchServer::ParseQuery(const string_view text, const bool erase_duplicates) const {

    Query query;

    for (const string_view word : SplitIntoWords(text)) {
        ParseQueryWord(word, query);
    }

    if (erase_duplicates)
    {
        std::sort(query.minus_words.begin(), query.minus_words.end());
        auto m_last = std::unique(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.resize(distance(query.minus_words.begin(), m_last));
        //query.minus_words.erase(m_last, query.minus_words.end());

        std::sort(query.plus_words.begin(), query.plus_words.end());
        auto p_last = std::unique(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.resize(distance(query.plus_words.begin(), p_last));
        //query.plus_words.erase(p_last, query.plus_words.end());
    }

    return query;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty())
    {
        return 0;
    }

    return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

int SearchServer::GetDocumentRating(int document_id) const {
    if (documents_data_.count(document_id))
    {
        return documents_data_.at(document_id).rating;
    }

    return 0;
}

void PrintMatchDocumentResult(int document_id, const vector<string_view>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string_view word : words) {
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
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            cout << document << endl;
        }
    }
    catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;

        for (const auto document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}
