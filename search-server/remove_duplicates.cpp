#include "remove_duplicates.h"

#include <vector>
#include <set>
#include <algorithm>
#include <map>
#include <iostream>
#include <iterator>

using namespace std;

// id документа и соответствующее ему множество слов
struct DocumentUniqueWords {
    int document_id;
    set<string_view> unique_words;
};

// сравнение структур DocumentUniqueWords для использования в std::set
bool operator<(const DocumentUniqueWords& lhv, const DocumentUniqueWords& rhv) {
    return make_pair(lhv.document_id, lhv.unique_words) < make_pair(rhv.document_id, rhv.unique_words);
}

// удаление дубликатов документов (содержат одинаковые наборы слов)
void RemoveDuplicates(SearchServer& search_server)
{
    // множество документов (id) и соответсвующих им наборов слов
    set<DocumentUniqueWords> docs_unique_words;
    
    for (size_t i = 0; i < search_server.GetDocumentCount(); ) {
        int document_id = *(next(search_server.begin(), i));
        set<string_view> current_doc_unique_words;
        const auto& doc_freqs = search_server.GetWordFrequencies(document_id);

        for (const auto& [word, freq] : doc_freqs) {
            current_doc_unique_words.insert(word);
        }

		if (any_of(docs_unique_words.begin(), docs_unique_words.end(),
			[current_doc_unique_words](const DocumentUniqueWords& d) {
				return (d.unique_words == current_doc_unique_words);
			}))
		{
			cout << "Found duplicate document id "s << document_id << endl;
			search_server.RemoveDocument(document_id);
		}
        else
        {
            docs_unique_words.insert({ document_id, current_doc_unique_words });
            ++i;
        }
    }
}