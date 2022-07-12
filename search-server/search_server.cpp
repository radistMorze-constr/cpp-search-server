#include "search_server.h"

//inline constexpr int SearchServer::INVALID_DOCUMENT_ID = -1;

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Попытка добавления документа с отрицательным id = " + std::to_string(document_id));
    }
    if (documents_.count(document_id) > 0) {
        throw std::invalid_argument("Попытка добавления документа с уже существующим id = " + std::to_string(document_id));
    }

    const std::vector<std::string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id,
        DocumentData{
            ComputeAverageRating(ratings),
            status
        });
    document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status_input) const {
    return FindTopDocuments(raw_query, [status_input](int id, DocumentStatus status, int rating) {
        return status == status_input;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, [](int id, DocumentStatus status, int rating) {
        return status == DocumentStatus::ACTUAL;
        });
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::vector<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::vector<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static std::map<std::string, double> result;
    if (documents_.count(document_id) == 0) {
        return result;
    }
    return document_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    auto it_to_id = find(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(it_to_id);
    documents_.erase(document_id);
    for (const auto& [word, _] : document_to_word_freqs_[document_id]) {
        word_to_document_freqs_[word].erase(document_id);
    }
    document_to_word_freqs_.erase(document_id);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    LOG_DURATION("Operation time");
    const Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Слово запроса \"" + word + "\" содержит спецсимволы");
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
    if (text.empty()) {
        throw std::invalid_argument("Пустая строка в запросе");
    }
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty()) {
        throw std::invalid_argument("Слово из запроса состоит из одного знака \"-\"");
    }
    if (text[0] == '-') {
        throw std::invalid_argument("Минус-слово из запроса \"" + text + "\" содержит более одного знака \"-\" в начале");
    }
    if (!IsValidWord(text)) {
        throw std::invalid_argument("Слово из запроса \"" + text + "\" содержит спецсимволы");
    }
    return {
        text,
        is_minus,
        IsStopWord(text)
    };
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    for (const std::string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            }
            else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}