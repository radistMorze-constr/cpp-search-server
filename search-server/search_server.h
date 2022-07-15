#pragma once
#include <string>
#include <vector>
#include <map>
#include <numeric>
#include <algorithm>
#include <iterator>
#include <cmath>

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"

static const int MAX_RESULT_DOCUMENT_COUNT = 5;
static const double EQUAL_MAX_DIFFERENCE = 1e-6;

// Переопределение оператора вывода в поток для вектора
template <typename type>
std::ostream& operator<<(std::ostream& os, const std::vector<type>& data) {
    os << "[";
    if (!data.empty()) {
        bool is_first = true;
        for (const auto& elem : data) {
            if (!is_first) os << ", ";
            is_first = false;
            os << elem;
        }
    }
    os << "]";
    return os;
}

class SearchServer {
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status_input) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    template <typename Filter>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, Filter filter_function) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::map <int, std::map<std::string, double>> document_to_word_freqs_;
    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(const std::string& word) const;

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string text) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string& text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename Filter>
    std::vector<Document> FindAllDocuments(const Query& query, Filter filter_function) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
}

template <typename Filter>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, Filter filter_function) const {
    LOG_DURATION("Operation time");
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, filter_function);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EQUAL_MAX_DIFFERENCE) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename Filter>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Filter filter_function) const {
    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const DocumentData& document = documents_.at(document_id);
            if (filter_function(document_id, document.status, document.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
            });
    }
    return matched_documents;
}