#pragma once

// ���� ��������� �������� ������� �� ����-������� �� �������
void TestCreateServer();

// ���� ���������, ��� ��������� ������� ��������� ����-����� ��� ���������� ����������
void TestExcludeStopWordsFromAddedDocumentContent();

// ��������� �����-����
void TestExcludeDocumentsWithMinusWords();

//�������� �� ���������� ���������� � server
void TestSuprplaceDocument();

// ������� ����������
void TestMatchDocuments();

//����������� �� �������������
void TestSortByRelevation();

//��������� ��������� �������
void TestRating();

//��������� ��������� �������� ����������� ���������
void TestFilterPredicate();

//����� ���������� �� �������
void TestSearchDocumentsByStatus();

//��������� ������� �������������
void TestCorrectCalculationRelevation();

// ������� TestSearchServer �������� ������ ����� ��� ������� ������
void TestSearchServer();