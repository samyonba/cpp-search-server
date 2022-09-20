#include "remove_duplicates.h"
#include <vector>
#include <set>
#include <algorithm>
#include <map>

using namespace std;

void RemoveDuplicates(SearchServer& search_server)
{
    struct DocumentUniqueWords {
        int document_id;
        set<string> unique_words;
    };
    vector<DocumentUniqueWords> documents_unique_words;
    sort(documents_unique_words.begin(), documents_unique_words.end(), [](const DocumentUniqueWords& lhv, const DocumentUniqueWords& rhv) {
        return lhv.document_id < rhv.document_id;
        });
    for (const int document_id : search_server) {
        set<string> current_doc_unique_words;
        auto& doc_freqs = search_server.GetWordFrequencies(document_id);
        for (const auto& [word, freq] : doc_freqs) {
            current_doc_unique_words.insert(word);
        }
        documents_unique_words.push_back({ document_id, current_doc_unique_words });
    }
    for (size_t i = 0; i < documents_unique_words.size() - 1; ++i)
    {
        for (size_t j = i + 1; j < documents_unique_words.size(); )
        {
            if (documents_unique_words[i].unique_words == documents_unique_words[j].unique_words) {
                int document_to_remove_id = documents_unique_words[j].document_id;
                documents_unique_words.erase(documents_unique_words.begin() + j);
                search_server.RemoveDocument(document_to_remove_id);

                cout << "Found duplicate document id "s << document_to_remove_id << endl;
            }
            else {
                ++j;
            }
        }
    }
}