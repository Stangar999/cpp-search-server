#pragma once

#include <iostream>

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
// Тест проверяет, поисковая система добавляет и ищет документы
void TestAddedDocumentContent();
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();
// Тест проверяет, что поисковая система исключает документы с минус словами
void TestExcludeMinusWordsFromFindDocument();
// Тест проверяет, поисковая систему на матчинг
void TestMatchDocuments(); // TODO в тест системе видимо тоже имя тест с TestMatchDocument, возникает ошибка (error: cannot convert ‘<unresolved overloaded function type>’ to ‘std::function<void()>&&’) не справедливо почему я менять должен
// Тест проверяет что результат отсортирован по релевантности
void TestSortRelevancsDocument();
// Тест проверяет, среднее аримфмет рейтинга
void TestAverRatingAddDoc();
// Тест проверяет, предикат
void TestPredicate();
// Тест проверяет, фильтр по статутсу
void TestStatus();
// Тест проверяет, релевантность
void TestRelevance();
// Тест проверяет, GetWordFrequencies
void TestGetWordFrequencies();
// Тест проверяет, методы BeginEnd
void TestBeginEnd();
// Тест проверяет, метод RemoveDocument
void TestRemoveDocument();
// Тест проверяет, RemoveDuplicates
void TestRemoveDuplicat(); // TODO в тест системе видимо тоже имя тест с TestRemoveDuplicates, возникает ошибка (error: cannot convert ‘<unresolved overloaded function type>’ to ‘std::function<void()>&&’) не справедливо почему я менять должен
// запуск тестов
void TestSearchServer();
//-------------------------------------------------------------------------------------------------------------
