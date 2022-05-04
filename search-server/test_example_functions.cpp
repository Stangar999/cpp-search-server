#include <iterator>
#include "test_example_functions.h"
//#include "remove_duplicates.h"
#include "search_server.h"
//-------------------------------------------------------------------------------------------------------------
void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint) {
    if (!value) {
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}
//-------------------------------------------------------------------------------------------------------------
void TestAddedDocumentContent() {
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
    SearchServer server(""s);
    {
        server.AddDocument(42, ""s, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT(found_docs.empty());
    }
    {
        server.AddDocument(43, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT_EQUAL(found_docs.size(), SINGL_RSLT);
        ASSERT_EQUAL(found_docs[0].id, 43);
    }
}
//-------------------------------------------------------------------------------------------------------------
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), SINGL_RSLT);
        ASSERT_EQUAL(found_docs.back().id, doc_id);
    }
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}
//-------------------------------------------------------------------------------------------------------------
void TestExcludeMinusWordsFromFindDocument() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
    SearchServer server(""s);
    {
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in the -cat"s).empty());
        ASSERT(!server.FindTopDocuments("cat"s).empty());
    }
}
//-------------------------------------------------------------------------------------------------------------
void TestMatchDocuments() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
    SearchServer server(""s);
    // убеждаемся, что матчинг находит все слова
    {
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        const auto& [vec, st] = server.MatchDocument(content, doc_id);
        ASSERT(vec.size() == 4 && st == DocumentStatus::BANNED);
    }
    // убеждаемся, что матчинг исключает доки с минус словами
    {
        const auto& [vec, _] = server.MatchDocument("cat in the -city"s, doc_id);
        ASSERT(vec.empty());
    }
    {
        const auto& [vec, _] = server.MatchDocument("dog"s, doc_id);
        ASSERT(vec.empty());
    }
}
//-------------------------------------------------------------------------------------------------------------
void TestSortRelevancsDocument() {
   const int doc_id1 = 41;
   const std::string content1 = "cat in cat the city"s;
   const int doc_id2 = 42;
   const std::string content2 = "dog in the city"s;
   const int doc_id3 = 43;
   const std::string content3 = "cat out of country"s;
   const std::vector<int> ratings = {1, 2, 3};
   {
       SearchServer server(""s);
       server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
       const auto found_docs = server.FindTopDocuments("cat in the city"s);
       ASSERT(found_docs.size() == 3 &&
              found_docs[0].relevance > found_docs[1].relevance &&
              found_docs[1].relevance > found_docs[2].relevance);
   }
}
//-------------------------------------------------------------------------------------------------------------
void TestAverRatingAddDoc() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings1 = {1, 2, 3};
    const std::vector<int> ratings2 = {1, -2, 3};
    const std::vector<int> ratings3 = {-1, -2, -3};
    // убеждаемся, что документ добавляется с среднее аримфмет рейтинга
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings1);
        const auto& found_docs = server.FindTopDocuments(content);
        ASSERT_EQUAL(found_docs.back().rating, 2 );
    }
    // убеждаемся, что документ добавляется с среднее аримфмет рейтинга
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings2);
        const auto& found_docs = server.FindTopDocuments(content);
        ASSERT_EQUAL(found_docs.back().rating, static_cast<int>(0.667) );
    }
    // убеждаемся, что документ добавляется с среднее аримфмет рейтинга
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings3);
        const auto& found_docs = server.FindTopDocuments(content);
        ASSERT_EQUAL(found_docs.back().rating, -2 );
    }
}
//-------------------------------------------------------------------------------------------------------------
void TestPredicate() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const int doc_id1 = 43;
    const std::string content1 = "dog in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
    const std::vector<int> ratings1 = {2, 3, 4};
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content1, DocumentStatus::IRRELEVANT, ratings1);
        const auto& found_docs = server.FindTopDocuments("in the city"s, [&](int /*document_id*/, DocumentStatus document_status, int /*rating*/)
        {
            bool result = document_status == DocumentStatus::IRRELEVANT;
            return result;
        });
        ASSERT(found_docs[0].id == doc_id1  && found_docs.size() == SINGL_RSLT);
    }
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content1, DocumentStatus::IRRELEVANT, ratings1);
        const auto& found_docs = server.FindTopDocuments("in the city"s, [&](int document_id, DocumentStatus /*document_status*/, int /*rating*/)
        {
            bool result = document_id == doc_id1;
            return result;
        });
        ASSERT(found_docs[0].id == doc_id1 && found_docs.size() == SINGL_RSLT);
    }
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content1, DocumentStatus::IRRELEVANT, ratings1);
        const auto& found_docs = server.FindTopDocuments("in the city"s, [&](int /*document_id*/, DocumentStatus /*document_status*/, int rating)
        {
            bool result = rating == 3;
            return result;
        });
        ASSERT(found_docs[0].rating == 3 && found_docs.size() == SINGL_RSLT);
    }
}
//-------------------------------------------------------------------------------------------------------------
void TestStatus() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const int doc_id1 = 43;
    const std::string content1 = "dog in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
    const std::vector<int> ratings1 = {2, 3, 4};
    const int doc_id2 = 1;
    const int doc_id3 = 2;
    {
        SearchServer server(""s);
        server.AddDocument(1, content, DocumentStatus::BANNED, ratings);
        server.AddDocument(2, content, DocumentStatus::REMOVED, ratings);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content1, DocumentStatus::IRRELEVANT, ratings1);
        const auto& found_docs = server.FindTopDocuments("in the city"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), SINGL_RSLT );
        ASSERT_EQUAL(found_docs.back().id, doc_id1 );
        const auto& found_docs1 = server.FindTopDocuments("in the city"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs1.size(), SINGL_RSLT );
        ASSERT_EQUAL(found_docs1.back().id, doc_id );
        const auto& found_docs2 = server.FindTopDocuments("in the city"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs2.size(), SINGL_RSLT );
        ASSERT_EQUAL(found_docs2.back().id, doc_id2 );
        const auto& found_docs3 = server.FindTopDocuments("in the city"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_docs3.size(), SINGL_RSLT );
        ASSERT_EQUAL(found_docs3.back().id, doc_id3 );
    }
}
//-------------------------------------------------------------------------------------------------------------
void TestRelevance() {
   const int doc_id1 = 41;
   const std::string content1 = "cat in the city"s;
   const int doc_id2 = 42;
   const std::string content2 = "dog in the city"s;
   const int doc_id3 = 43;
   const std::string content3 = "cat out of country"s;
   const std::vector<int> ratings = {1, 2, 3};
   {
       SearchServer server(""s);
       server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
       const auto found_docs = server.FindTopDocuments("cat in the city"s);
       ASSERT(found_docs.size() == 3 && found_docs[0].relevance - 0.405465 < EPSILON &&
                                        found_docs[1].relevance - 0.304099 < EPSILON &&
                                        found_docs[2].relevance - 0.101366 < EPSILON);
   }
}
//-------------------------------------------------------------------------------------------------------------
void TestGetWordFrequencies() {
   const int doc_id1 = 41;
   const std::string content1 = "cat in the city"s;
   const std::vector<int> ratings = {1, 2, 3};
   {
       SearchServer server("in the"s);
       server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
       ASSERT(server.GetWordFrequencies(41).size() == 2);
       ASSERT(server.GetWordFrequencies(41).at("cat"s) == 0.5);
       ASSERT(server.GetWordFrequencies(41).at("city"s) == 0.5);
       ASSERT(server.GetWordFrequencies(100).empty());
   }
}
//-------------------------------------------------------------------------------------------------------------
void TestBeginEnd() {
    SearchServer search_server("and in at"s);

    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});

    ASSERT(*search_server.begin() == 1);
    ASSERT(*next(search_server.begin()) == 2);
    ASSERT(*next(search_server.end(),-1) == 3);
}
//-------------------------------------------------------------------------------------------------------------
void TestRemoveDocument() {
   const int doc_id1 = 41;
   const std::string content1 = "cat in cat the city"s;
   const int doc_id2 = 42;
   const std::string content2 = "dog in the city"s;
   const int doc_id3 = 43;
   const std::string content3 = "cat out of country"s;
   const std::vector<int> ratings = {1, 2, 3};
   {
       SearchServer server("in the of out"s);
       server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
       server.RemoveDocument(42);
       const auto found_docs = server.FindTopDocuments("cat in the city"s);
       ASSERT(found_docs.size() == 2);
       ASSERT(*server.begin() == 41);
       ASSERT(*next(server.end(),-1) == 43);
       ASSERT(server.GetDocumentCount() == 2);
   }
}
//-------------------------------------------------------------------------------------------------------------
void TestRemoveDuplicat() {
   const int doc_id1 = 41;
   const std::string content1 = "cat in cat the city"s;
   const int doc_id2 = 42;
   const std::string content2 = "dog in the city"s;
   const int doc_id3 = 43;
   const std::string content3 = "cat out of country"s;
   const int doc_id4 = 44;
   const std::string content4 = "cat out of country"s;
   const int doc_id5 = 45;
   const std::string content5 = "cat out of country"s;
   const std::vector<int> ratings = {1, 2, 3};
   {
       SearchServer server("in the of out"s);
       server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id4, content4, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id5, content4, DocumentStatus::ACTUAL, ratings);

       ASSERT(server.GetDocumentCount() == 5);
//       RemoveDuplicates(server);
       ASSERT(server.GetDocumentCount() == 3);
   }
}
//-------------------------------------------------------------------------------------------------------------
void TestSearchServer() {
    RUN_TEST(TestAddedDocumentContent);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWordsFromFindDocument);
    RUN_TEST(TestMatchDocuments);
    RUN_TEST(TestSortRelevancsDocument);
    RUN_TEST(TestAverRatingAddDoc);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatus);
    RUN_TEST(TestRelevance);
    RUN_TEST(TestGetWordFrequencies);
    RUN_TEST(TestBeginEnd);
    RUN_TEST(TestRemoveDocument);
    RUN_TEST(TestRemoveDuplicat);
}
//-------------------------------------------------------------------------------------------------------------

