#include "remove_duplicates.h"
#include "search_server.h"

void RemoveDuplicates(SearchServer& search_server) {
	std::map<std::set<std::string>, int> doc_set_id;
	std::vector<int> id_for_remove;
	for (const int document_id : search_server) {
		auto words_in_document = search_server.GetWordFrequencies(document_id);
		std::set<std::string> words;
		for (const auto& [word, freq] : words_in_document) {
			words.insert(word);
		}
		if (doc_set_id.count(words) > 0) {
			int id_remove = (document_id < doc_set_id[words]) ? doc_set_id[words] : document_id;
			doc_set_id[words] = (document_id < doc_set_id[words]) ? document_id : doc_set_id[words];
			id_for_remove.push_back(id_remove);
		}
		else {
			doc_set_id[words] = document_id;
		}
	}
	
	for (int id : id_for_remove) {
		std::cout << "Found duplicate document id " << id << std::endl;
		search_server.RemoveDocument(id);
	}
}