//#include "search_server.h"

#include <algorithm>
#include <iostream>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <stdexcept>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EQUAL_MAX_DIFFERENCE = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

static bool IsValidWord(const string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!IsValidWord(str))
            throw invalid_argument("Стоп-слова содержат специсимволы");
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    }
    
    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if ((document_id < 0) || (documents_.count(document_id) > 0)) {
            throw invalid_argument("Некорректный id документа (отрицательный, либо повторяющийся)");
        }

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
        document_ids_.push_back(document_id);
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status_input = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status_input](int id, DocumentStatus status, int rating) {
            return status == status_input;
            });
    }

    template <typename Filter>
    vector<Document> FindTopDocuments(const string& raw_query, Filter filter_function) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, filter_function);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EQUAL_MAX_DIFFERENCE) {
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

    int GetDocumentCount() const {
        return documents_.size();
    }

    int GetDocumentId(int index) const {
        if (index >= 0 && index < GetDocumentCount()) {
            return document_ids_[index];
        }
        throw out_of_range("Попытка получить доступ к некорректному id");
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
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

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) 
                throw invalid_argument("Содержит недопустимые технические символы");
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        if (text.empty())
            throw invalid_argument("Пустая строка в запросе");
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        if (text.empty() || text[0] == '-' || !IsValidWord(text))
            throw invalid_argument("Некорректная строка в запросе");
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
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

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Filter>
    vector<Document> FindAllDocuments(const Query& query, Filter filter_function) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};

// Переопределение оператора вывода в поток для вектора
template <typename type>
ostream& operator<<(ostream& os, const vector<type>& data) {
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

// Реализация макросов
// ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST

template <typename function>
void RunTestImpl(function test_func, const string& test_func_str) {
    /* Напишите недостающий код */
    test_func();
    cerr << test_func_str << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Поддержка минус-слов
void TestExcludeDocumentsWithMinusWords() {
    const int doc_id = 42;
    const string content1 = "cat in the city"s;
    const string content2 = "cat in the town"s;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server1(""s), server2(""s);
    server1.AddDocument(doc_id, content1, DocumentStatus::ACTUAL, ratings);
    ASSERT(server1.FindTopDocuments("cat -city"s).empty());
    server2.AddDocument(doc_id, content2, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server2.FindTopDocuments("cat -city"s);
    ASSERT(!found_docs.empty());
    ASSERT_EQUAL_HINT(found_docs[0].id, doc_id, "The document without minus-word isn't found"s);
}

//Проверка на добавление документов в server
void TestSuprplaceDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server(""s);
    ASSERT_EQUAL(server.GetDocumentCount(), 0);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
}

// Матчинг документов
void TestMatchDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city which is placed on the other side"s;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server("in the is on which"s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    {
        //Проверим что вернуться все совпадения, кроме стоп-слов
        auto matched_document = server.MatchDocument("cat is placed in the side home"s, 42);
        const auto [matched_words, doc_status] = matched_document;
        vector<string> right_result = { "cat"s, "placed"s, "side"s };
        ASSERT_EQUAL(matched_words, right_result);
        ASSERT(doc_status == DocumentStatus::ACTUAL);
    }

    {
        //Проверим, что при содержании минус-слов вернется пустой список
        auto matched_document = server.MatchDocument("-cat placed side home"s, 42);
        const auto [matched_words, doc_status] = matched_document;
        ASSERT(matched_words.empty());
        ASSERT(doc_status == DocumentStatus::ACTUAL);
    }
}

//Сортировака по релевантности
void TestSortByRelevation() {
    const string content1 = "cat in the city which is placed on the other side"s;
    double relevance1 = log(1) / 11;
    const string content2 = "cat played in the garage"s;
    double relevance2 = 0.2 * log(1) + 0.2 * log(3);
    const string content3 = "cat vs dog who win"s;
    double relevance3 = 0.2 * log(1) + 2 * 0.2 * log(3);
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server(""s);
    server.AddDocument(1, content1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, content2, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(3, content3, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("cat played who win"s);
    ASSERT(found_docs[0].relevance > found_docs[1].relevance);
    ASSERT(found_docs[1].relevance > found_docs[2].relevance);
}

//Корректно считается рейтинг
void TestRating() {
    const string content = "cat vs dog who win"s;
    const vector<int> ratings1 = { 1, 2, 3 };
    const int rating1 = (1 + 2 + 3) / 3;
    const vector<int> ratings2 = { 2, 3 };
    const int rating2 = (2 + 3) / 2;
    const vector<int> ratings3 = { 0 };
    const vector<int> ratings4 = { };
    const int rating_zero = 0;
    SearchServer server1(""s), server2(""s), server3(""s), server4(""s);
    server1.AddDocument(1, content, DocumentStatus::ACTUAL, ratings1);
    const auto found_docs1 = server1.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs1[0].rating, rating1);
    server2.AddDocument(2, content, DocumentStatus::ACTUAL, ratings2);
    const auto found_docs2 = server2.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs2[0].rating, rating2);
    server3.AddDocument(3, content, DocumentStatus::ACTUAL, ratings3);
    const auto found_docs3 = server3.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs3[0].rating, rating_zero);
    server4.AddDocument(4, content, DocumentStatus::ACTUAL, ratings4);
    const auto found_docs4 = server4.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs4[0].rating, rating_zero);

}

//Правильно фильтрует согласно задаваемого предиката
void TestFilterPredicate() {
    const string content = "cat vs dog who win"s;
    const vector<int> ratings1 = { 1, 2, 3 };
    const vector<int> ratings2 = { 2, 3 };
    const vector<int> ratings3 = { 0 };
    SearchServer server(""s);
    server.AddDocument(2, content, DocumentStatus::ACTUAL, ratings1);
    server.AddDocument(3, content, DocumentStatus::BANNED, ratings2);
    server.AddDocument(1, content, DocumentStatus::IRRELEVANT, ratings3);
    auto found_docs = server.FindTopDocuments("cat"s, [](int id, DocumentStatus status, int rating) {
        return id == 2;
        });
    ASSERT_EQUAL(found_docs[0].id, 2);
    found_docs = server.FindTopDocuments("cat"s, [](int id, DocumentStatus status, int rating) {
        return rating == 2;
        });
    ASSERT_EQUAL(found_docs[0].id, 2);
}

//Поиск документов по статусу
void TestSearchDocumentsByStatus() {
    const string content = "cat vs dog who win"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server(""s);
        server.AddDocument(1, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, content, DocumentStatus::BANNED, ratings);
        server.AddDocument(3, content, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(4, content, DocumentStatus::REMOVED, ratings);
        auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs[0].id, 1);
        found_docs = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs[0].id, 2);
        found_docs = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs[0].id, 3);
        found_docs = server.FindTopDocuments("cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_docs[0].id, 4);
    }
    SearchServer server(""s);
    server.AddDocument(1, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(3, content, DocumentStatus::IRRELEVANT, ratings);
    server.AddDocument(4, content, DocumentStatus::REMOVED, ratings);
    auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
    ASSERT(found_docs.empty());
}

//Правильно считает реливантность
void TestCorrectCalculationRelevation() {
    const string content1 = "белый кот и модный ошейник"s; //tf-idf = 0.1014
    double relevance1 = 0.2 * log(1.5);
    const string content2 = "пушистый кот пушистый хвост"s; //tf-idf = 0.6507
    const double relevance2 = 0.5 * log(3) + 0.25 * log(1.5);
    const string content3 = "ухоженный пёс выразительные глаза"s; //tf-idf = 0.2746
    const double relevance3 = 0.25 * log(3);
    const vector<int> ratings = { };
    SearchServer server(""s);
    server.AddDocument(1, content1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, content2, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(3, content3, DocumentStatus::ACTUAL, ratings);
    //Документы отсортированы в порядке убывания relevance: 2, 3, 1
    const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT(abs(found_docs[0].relevance - relevance2) < EQUAL_MAX_DIFFERENCE);
    ASSERT(abs(found_docs[1].relevance - relevance3) < EQUAL_MAX_DIFFERENCE);
    ASSERT(abs(found_docs[2].relevance - relevance1) < EQUAL_MAX_DIFFERENCE);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsWithMinusWords);
    RUN_TEST(TestSuprplaceDocument);
    RUN_TEST(TestMatchDocuments);
    RUN_TEST(TestSortByRelevation);
    RUN_TEST(TestRating);
    RUN_TEST(TestFilterPredicate);
    RUN_TEST(TestSearchDocumentsByStatus);
    RUN_TEST(TestCorrectCalculationRelevation);
}

// --------- Окончание модульных тестов поисковой системы -----------

// ------------ Пример использования ----------------

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
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
            PrintDocument(document);
        }
    }
    catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    setlocale(0, "");
    TestSearchServer();
    cout << "Если строка выведена, значит все тесты прошли" << endl;

    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, { 1, 1, 1 });

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
}