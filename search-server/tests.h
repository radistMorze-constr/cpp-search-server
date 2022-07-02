#pragma once

// Тест проверяет создание сервера со стоп-словами из вектора
void TestCreateServer();

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();

// Поддержка минус-слов
void TestExcludeDocumentsWithMinusWords();

//Проверка на добавление документов в server
void TestSuprplaceDocument();

// Матчинг документов
void TestMatchDocuments();

//Сортировака по релевантности
void TestSortByRelevation();

//Корректно считается рейтинг
void TestRating();

//Правильно фильтрует согласно задаваемого предиката
void TestFilterPredicate();

//Поиск документов по статусу
void TestSearchDocumentsByStatus();

//Правильно считает реливантность
void TestCorrectCalculationRelevation();

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();