#include "search_server.h"

//inline constexpr int SearchServer::INVALID_DOCUMENT_ID = -1;


SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

SearchServer::SearchServer(const std::string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Попытка добавления документа с отрицательным id = " + std::to_string(document_id));
    }
    if (documents_.count(document_id) > 0) {
        throw std::invalid_argument("Попытка добавления документа с уже существующим id = " + std::to_string(document_id));
    }

    documents_.emplace(document_id,
        DocumentData{
            std::string(document),
            ComputeAverageRating(ratings),
            status
        });

    const std::vector<std::string_view> words = SplitIntoWordsNoStop(documents_[document_id].text_doc);
    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }

    document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status_input) const {
    return FindTopDocuments(raw_query, [status_input](int id, DocumentStatus status, int rating) {
        return status == status_input;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return FindTopDocuments(raw_query, [](int id, DocumentStatus status, int rating) {
        return status == DocumentStatus::ACTUAL;
        });
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static std::map<std::string_view, double> result;
    if (documents_.count(document_id) == 0) {
        return result;
    }
    return document_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy seq, int document_id) {
    SearchServer::RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    auto it_to_id = find(std::execution::par, document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(it_to_id);
    documents_.erase(document_id);
    for (const auto& [word, _] : document_to_word_freqs_[document_id]) {
        word_to_document_freqs_[word].erase(document_id);
    }
    document_to_word_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy par, int document_id) {
    auto it_to_id = find(std::execution::par, document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(it_to_id);
    documents_.erase(document_id);
    std::vector<const std::string_view*> words_in_id(document_to_word_freqs_[document_id].size());
    std::transform(std::execution::par, document_to_word_freqs_[document_id].begin(), document_to_word_freqs_[document_id].end(), words_in_id.begin(), [](auto& word_freq) {
        return &word_freq.first;
        });
    std::for_each(std::execution::par, words_in_id.begin(), words_in_id.end(), [&document_id, this](const std::string_view* ptr_word) {
        word_to_document_freqs_[*ptr_word].erase(document_id);
        });
    document_to_word_freqs_.erase(document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    //LOG_DURATION("Operation time");
    const Query query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;
    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            //matched_words.clear();
            return { matched_words, documents_.at(document_id).status };
        }
    }
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word_to_document_freqs_.find(word)->first);
        }
    }
    return { matched_words, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy seq, const std::string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy par, const std::string_view raw_query, int document_id) const {
    //LOG_DURATION("Operation time");
    const QueryPar query = ParseQueryPar(raw_query);
    bool flag =  std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&](const std::string_view word) {
        auto it = document_to_word_freqs_.find(document_id);
        return it != document_to_word_freqs_.end() && it->second.count(word) != 0;
        });
    if (flag) {
        return { {}, documents_.at(document_id).status };
    }
    std::vector<std::string_view> matched_words(query.plus_words.size());
    
    auto last = std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [&](const std::string_view word) {
        return word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id);
        });
        
    matched_words.resize(std::distance(matched_words.begin(), last));
    std::sort(matched_words.begin(), matched_words.end());
    last = std::unique(std::execution::par, matched_words.begin(), matched_words.end());
    matched_words.resize(std::distance(matched_words.begin(), last));
    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Слово запроса \"" + std::string(word) + "\" содержит спецсимволы");
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

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
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
        throw std::invalid_argument("Минус-слово из запроса \"" + std::string(text) + "\" содержит более одного знака \"-\" в начале");
    }
    if (!IsValidWord(text)) {
        throw std::invalid_argument("Слово из запроса \"" + std::string(text) + "\" содержит спецсимволы");
    }
    return {
        text,
        is_minus,
        IsStopWord(text)
    };
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const {
    Query query;
    for (const std::string_view word : SplitIntoWords(text)) {
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

SearchServer::QueryPar SearchServer::ParseQueryPar(const std::string_view text) const {
    QueryPar query_par;
    for (const std::string_view word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query_par.minus_words.push_back(query_word.data);
            }
            else {
                if (std::find(query_par.plus_words.begin(), query_par.plus_words.end(), query_word.data) == query_par.plus_words.end()) {
                    query_par.plus_words.push_back(query_word.data);
                }
            }
        }
    }
    return query_par;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}