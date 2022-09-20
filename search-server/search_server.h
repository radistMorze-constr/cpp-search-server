#pragma once
#include <string>
#include <vector>
#include <map>
#include <numeric>
#include <algorithm>
#include <iterator>
#include <cmath>
#include <execution>
#include <string_view>
#include <utility>
#include <future>

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"

static const int MAX_RESULT_DOCUMENT_COUNT = 5;
static const double EQUAL_MAX_DIFFERENCE = 1e-6;
static const size_t NUMBER_THREADS = 12;

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

    explicit SearchServer(const std::string_view stop_words_text);

    explicit SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy policy, const std::string_view raw_query, DocumentStatus status_input) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status_input) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy policy, const std::string_view raw_query) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    template <typename ExecutionPolicy, typename Filter>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy policy, const std::string_view raw_query, Filter filter_function) const;

    template <typename Filter>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, Filter filter_function) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(std::execution::sequenced_policy seq, int document_id);
    
    void RemoveDocument(std::execution::parallel_policy par, int document_id);

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy seq, const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy par, const std::string_view raw_query, int document_id) const;

private:
    struct DocumentData {
        std::string text_doc;
        int rating;
        DocumentStatus status;
    };

    std::map <int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    struct QueryPar {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view text) const;

    QueryPar ParseQueryPar(const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename ExecutionPolicy, typename Filter>
    std::vector<Document> FindAllDocuments(const ExecutionPolicy policy, const QueryPar& query, Filter filter_function) const;

    template <typename Filter>
    std::vector<Document> FindAllDocuments(const Query& query, Filter filter_function) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy policy, const std::string_view raw_query, DocumentStatus status_input) const {
    return FindTopDocuments(policy, raw_query, [status_input](int id, DocumentStatus status, int rating) {
        return status == status_input;
        });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy policy, const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, [](int id, DocumentStatus status, int rating) {
        return status == DocumentStatus::ACTUAL;
        });
}

template <typename ExecutionPolicy, typename Filter>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy policy, const std::string_view raw_query, Filter filter_function) const {
    if (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        return FindTopDocuments(raw_query, filter_function);
    }
    const QueryPar query_par = ParseQueryPar(raw_query);
    //std::unique(std::execution::par, query_par.plus_words.begin(), query_par.plus_words.end());
    auto matched_documents = FindAllDocuments(std::execution::par, query_par, filter_function);

    //LOG_DURATION("Sorting in FindTopDocuments");
    std::sort(std::execution::par, matched_documents.begin(), matched_documents.end(),
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
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, Filter filter_function) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, filter_function);

    //LOG_DURATION("Sorting in FindTopDocuments");
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

template <typename ExecutionPolicy, typename Filter>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy policy, const QueryPar& query_par, Filter filter_function) const {
    ConcurrentMap<int, double> document_to_relevance(NUMBER_THREADS);

    std::for_each(policy, query_par.plus_words.begin(), query_par.plus_words.end(), [&](const std::string_view word) {
        if (word_to_document_freqs_.count(word) != 0) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const DocumentData& document = documents_.at(document_id);
                if (filter_function(document_id, document.status, document.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
        });

    std::for_each(policy, query_par.minus_words.begin(), query_par.minus_words.end(), [&](const std::string_view word) {
        if (word_to_document_freqs_.count(word) != 0) {
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.Erase(document_id);
            }
        }
        });

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
            });
    }
    return matched_documents;
}

template <typename Filter>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Filter filter_function) const {
    //LOG_DURATION("Function FindAllDocuments");
    std::map<int, double> document_to_relevance;
    for (const std::string_view word : query.plus_words) {
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

    for (const std::string_view word : query.minus_words) {
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