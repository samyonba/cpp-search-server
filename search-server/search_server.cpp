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
    // попытка добавить документ с отрицаиельным id
    if (document_id < 0)
        throw invalid_argument("Document id is negative"s);
    // попытка добавить документ с id, совпадающим с id документа, который добавился ранее
    if (documents_data_.count(document_id))
        throw invalid_argument("Given document id already matches to previous added document"s);
    // текст документа содержит спецсимволы
    if (!IsValidText(document))
        throw invalid_argument("Document text contains special characters"s);

    vector<string> document_words = SplitIntoWordsNoStop(document);

    // текст документа не должен содержать слов, начинающихся с '-'
    if (any_of(document_words.begin(), document_words.end(), MatchedAsMinusWord))
        throw invalid_argument("Document text contains a word starting with minus"s);

    added_documents_id_.push_back(document_id);
    documents_data_[document_id] = { status, ComputeAverageRating(ratings) };
    for (const string& word : document_words) {
        double word_term_freq = static_cast<double>(count(document_words.begin(), document_words.end(), word)) / document_words.size();
        word_to_documents_freqs_[word].insert({ document_id, word_term_freq });
        document_to_words_freqs_[document_id].insert({ word, word_term_freq });
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
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    // попытка матчинга документа по несуществующему id
	if (!documents_data_.count(document_id))
		throw invalid_argument("No document with given id"s);

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
}

const map<string, double> NULL_RESULT;

// получение частот слов по id документа
const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    if (document_to_words_freqs_.count(document_id))
        return document_to_words_freqs_.at(document_id);
    else {
        return NULL_RESULT;
    }
}

void SearchServer::RemoveDocument(int document_id)
{
    added_documents_id_.erase(find(added_documents_id_.begin(), added_documents_id_.end(), document_id));
    documents_data_.erase(documents_data_.find(document_id));
    document_to_words_freqs_.erase(document_to_words_freqs_.find(document_id));
    for (auto& [word, id_tf] : word_to_documents_freqs_) {
        auto it = id_tf.find(document_id);
        if (it != id_tf.end())
            id_tf.erase(it);
    }
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

// переданная строка начинается с '-'
bool SearchServer::MatchedAsMinusWord(const string& word) {
    if (word.empty())
        return false;
    return word.at(0) == '-';
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    // запрос не должен содержать слов, содержащих более одного '-' подряд
    if (ContainsDoubleDash(text))
        throw invalid_argument("Query contains double dash"s);
    // запрос не должен содержать спецсимволов
    if (!IsValidText(text))
        throw invalid_argument("Query contains special characters"s);

    Query query;
    for (const string& word : SplitIntoWordsNoStop(text)) {
        if (MatchedAsMinusWord(word)) {
            // после символа '-' отсутствет текст минус слова
            if (word.size() == 1) {
                throw invalid_argument("Text is missing after minus in query"s);
            }
            string no_prefix_word = word.substr(1);
            query.minus_words.insert(no_prefix_word);
            // если такое слово ранее добавлялось как plus_word, теперь его нужно удалить
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
