#include <iostream>

#include "search_server.h"
#include "string_processing.h"
#include "request_queue.h"
#include "paginator.h"
#include "read_input_functions.h"
#include "tests.h"
#include "log_duration.h"

using namespace std;
//void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
//                 const vector<int>& ratings) {
//    try {
//        search_server.AddDocument(document_id, document, status, ratings);
//    } catch (const exception& e) {
//        cout << "?????? ?????????? ????????? "s << document_id << ": "s << e.what() << endl;
//    }
//}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    try {
        LOG_DURATION_STREAM("Operation time", cout);
        cout << "FindTopDocuments query: "s << raw_query << endl;
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "FindTopDocuments FAIL: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        LOG_DURATION_STREAM("Operation time", cout);
        cout << "MatchDocuments query: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "MatchDocuments FAIL: "s << query << ": "s << e.what() << endl;
    }
}
//-------------------------------------------------------------------------------------------------------------
int main() {
    {
      LOG_DURATION_STREAM("Operation time", cout);
      TestSearchServer();
    }

    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});

    // 1439 �������� � ������� �����������
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // ��� ��� 1439 �������� � ������� �����������
    request_queue.AddFindRequest("curly dog"s);
    // ����� �����, ������ ������ ������, 1438 �������� � ������� �����������
    request_queue.AddFindRequest("big collar"s);
    // ������ ������ ������, 1437 �������� � ������� �����������
    request_queue.AddFindRequest("sparrow"s);
    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;

    MatchDocuments(search_server, "curly cat"s);
    FindTopDocuments(search_server, "big dog"s);
    return 0;
}
//-------------------------------------------------------------------------------------------------------------
