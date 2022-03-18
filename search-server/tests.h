#pragma once

#include <iostream>

#include "search_server.h"

//----------------------------------------------------------------------------
using namespace std::string_literals;

const size_t SINGL_RSLT = 1;
//----------------------------------------------------------------------------
template <class T>
void RunTestImpl(T func, const std::string& name_func) {
    func();
    std::cerr << name_func << " OK"<<std::endl;
}
#define RUN_TEST(func) RunTestImpl((func),#func)
//----------------------------------------------------------------------------
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cout << std::boolalpha;
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
//----------------------------------------------------------------------------
void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint);

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
//----------------------------------------------------------------------------
// ���� ���������, ��������� ������� ��������� � ���� ���������
void TestAddedDocumentContent();
// ���� ���������, ��� ��������� ������� ��������� ����-����� ��� ���������� ����������
void TestExcludeStopWordsFromAddedDocumentContent();
// ���� ���������, ��� ��������� ������� ��������� ��������� � ����� �������
void TestExcludeMinusWordsFromFindDocument();
// ���� ���������, ��������� ������� �� �������
void TestMatchDocument();
// ���� ��������� ��� ��������� ������������ �� �������������
void TestSortRelevancsDocument();
// ���� ���������, ������� �������� ��������
void TestAverRatingAddDoc();
// ���� ���������, ��������
void TestPredicate();
// ���� ���������, ������ �� ��������
void TestStatus();
// ���� ���������, �������������
void TestRelevance();
// ������ ������
void TestSearchServer();
//-------------------------------------------------------------------------------------------------------------
