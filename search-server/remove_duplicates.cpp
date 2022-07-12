#include "remove_duplicates.h"
#include "search_server.h"

void RemoveDuplicates(SearchServer& search_server) {
	std::map<std::set<std::string>, std::vector<int>> doc_set_id;
	for (const int document_id : search_server) {
		auto words_in_document = search_server.GetWordFrequencies(document_id);
		std::set<std::string> words;
		for (const auto& [word, freq] : words_in_document) {
			words.insert(word);
		}
		doc_set_id[words].push_back(document_id);
	}
	
	for (auto& [set_words, vec_ids] : doc_set_id) {
		std::sort(vec_ids.begin(), vec_ids.end());
		for (auto it = vec_ids.begin() + 1; it < vec_ids.end(); ++it) {
			std::cout << "Found duplicate document id " << *it << std::endl;
			search_server.RemoveDocument(*it);
		}
	}
}