#include "tests.h"
#include <string>
#include <vector>
#include <iostream>
#include "search_server.h"

using namespace std::string_literals;

// Реализация макросов
// ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST

template <typename function>
void RunTestImpl(function test_func, const std::string& test_func_str) {
    /* Напишите недостающий код */
    test_func();
    std::cerr << test_func_str << " OK" << std::endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
    const std::string& hint) {
    if (!value) {
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет создание сервера со стоп-словами из вектора
void TestCreateServer() {
    const std::string content = "и в на все предлоги"s;
    const std::vector<std::string> stop_words = {"и", "в", "на"};
    SearchServer server(stop_words);
    const auto found_docs = server.FindTopDocuments("все предлоги"s);
    ASSERT_EQUAL(found_docs.size(), 0);
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
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
    const std::string content1 = "cat in the city"s;
    const std::string content2 = "cat in the town"s;
    const std::vector<int> ratings = { 1, 2, 3 };
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
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    SearchServer server(""s);
    ASSERT_EQUAL(server.GetDocumentCount(), 0);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
}

// Матчинг документов
void TestMatchDocuments() {
    const int doc_id = 42;
    const std::string content = "cat in the city which is placed on the other side"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    SearchServer server("in the is on which"s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    {
        //Проверим что вернуться все совпадения, кроме стоп-слов
        auto matched_document = server.MatchDocument("cat is placed in the side home"s, 42);
        const auto [matched_words, doc_status] = matched_document;
        std::vector<std::string> right_result = { "cat"s, "placed"s, "side"s };
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
    const std::string content1 = "cat in the city which is placed on the other side"s;
    //double relevance1 = log(1) / 11;
    const std::string content2 = "cat played in the garage"s;
    //double relevance2 = 0.2 * log(1) + 0.2 * log(3);
    const std::string content3 = "cat vs dog who win"s;
    //double relevance3 = 0.2 * log(1) + 2 * 0.2 * log(3);
    const std::vector<int> ratings = { 1, 2, 3 };
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
    const std::string content = "cat vs dog who win"s;
    const std::vector<int> ratings1 = { 1, 2, 3 };
    const int rating1 = (1 + 2 + 3) / 3;
    const std::vector<int> ratings2 = { 2, 3 };
    const int rating2 = (2 + 3) / 2;
    const std::vector<int> ratings3 = { 0 };
    const std::vector<int> ratings4 = { };
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
    const std::string content = "cat vs dog who win"s;
    const std::vector<int> ratings1 = { 1, 2, 3 };
    const std::vector<int> ratings2 = { 2, 3 };
    const std::vector<int> ratings3 = { 0 };
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
    const std::string content = "cat vs dog who win"s;
    const std::vector<int> ratings = { 1, 2, 3 };
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
    const std::string content1 = "белый кот и модный ошейник"s; //tf-idf = 0.1014
    double relevance1 = 0.2 * log(1.5);
    const std::string content2 = "пушистый кот пушистый хвост"s; //tf-idf = 0.6507
    const double relevance2 = 0.5 * log(3) + 0.25 * log(1.5);
    const std::string content3 = "ухоженный пёс выразительные глаза"s; //tf-idf = 0.2746
    const double relevance3 = 0.25 * log(3);
    const std::vector<int> ratings = { };
    SearchServer server(""s);
    server.AddDocument(1, content1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, content2, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(3, content3, DocumentStatus::ACTUAL, ratings);
    //Документы отсортированы в порядке убывания relevance: 2, 3, 1
    const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT(std::abs(found_docs[0].relevance - relevance2) < EQUAL_MAX_DIFFERENCE);
    ASSERT(std::abs(found_docs[1].relevance - relevance3) < EQUAL_MAX_DIFFERENCE);
    ASSERT(std::abs(found_docs[2].relevance - relevance1) < EQUAL_MAX_DIFFERENCE);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestCreateServer);
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