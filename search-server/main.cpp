#include <iostream>
#include <random>
#include <execution>

#include "search_server.h"
#include "string_processing.h"
#include "request_queue.h"
#include "paginator.h"
#include "read_input_functions.h"
#include "test_example_functions.h"
#include "log_duration.h"
#include "process_queries.h"

using namespace std;
void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "AddDocument FAIL "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    try {
        LOG_DURATION_STREAM("Operation time"s, cout);
        cout << "FindTopDocuments query: "s << raw_query << endl;
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "FindTopDocuments FAIL: "s << e.what() << endl;
    }
}
//-------------------------------------------------------------------------------------------------------------
void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        LOG_DURATION_STREAM("Operation time"s, cout);
        cout << "MatchDocuments query: "s << query << endl;
        for (int document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "MatchDocuments FAIL: "s << query << ": "s << e.what() << endl;
    }
}
//-------------------------------------------------------------------------------------------------------------
string GenerateWord(mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob = 0) {
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

template <typename ExecutionPolicy>
void Test(string_view mark, SearchServer search_server, const string& query, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    const int document_count = search_server.GetDocumentCount();
    int word_count = 0;
    for (int id = 0; id < document_count; ++id) {
        const auto [words, status] = search_server.MatchDocument(policy, query, id);
        word_count += words.size();
    }
    cout << word_count << endl;
}

    #define TEST(policy) Test(#policy, search_server, query, execution::policy)

int main() {
    mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 1000, 10);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

    const string query = GenerateQuery(generator, dictionary, 500, 0.1);

    TestSearchServer();
    SearchServer search_server(dictionary[0]);
    {
        LOG_DURATION("add"s);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }
    }

    TEST(seq);
    TEST(par);
}
//string GenerateWord(mt19937& generator, int max_length) {
//    const int length = uniform_int_distribution(1, max_length)(generator);
//    string word;
//    word.reserve(length);
//    for (int i = 0; i < length; ++i) {
//        word.push_back(uniform_int_distribution('a', 'z')(generator));
//    }
//    return word;
//}

//vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
//    vector<string> words;
//    words.reserve(word_count);
//    for (int i = 0; i < word_count; ++i) {
//        words.push_back(GenerateWord(generator, max_length));
//    }
//    sort(words.begin(), words.end());
//    words.erase(unique(words.begin(), words.end()), words.end());
//    return words;
//}

//string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int max_word_count) {
//    const int word_count = uniform_int_distribution(1, max_word_count)(generator);
//    string query;
//    for (int i = 0; i < word_count; ++i) {
//        if (!query.empty()) {
//            query.push_back(' ');
//        }
//        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
//    }
//    return query;
//}

//vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
//    vector<string> queries;
//    queries.reserve(query_count);
//    for (int i = 0; i < query_count; ++i) {
//        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
//    }
//    return queries;
//}

//template <typename ExecutionPolicy>
//void Test(string_view mark, SearchServer search_server, ExecutionPolicy&& policy) {
//    LOG_DURATION("");
//    const int document_count = search_server.GetDocumentCount();
//    for (int id = 0; id < document_count; ++id) {
//        search_server.RemoveDocument(policy, id);
//    }
//    cout << search_server.GetDocumentCount() << endl;
//}

//#define TEST(mode) Test(#mode, search_server, execution::mode)

//int main() {
//    mt19937 generator;

//    const auto dictionary = GenerateDictionary(generator, 10'000, 25);
//    const auto documents = GenerateQueries(generator, dictionary, 10'000, 100);

//    {
//        SearchServer search_server(dictionary[0]);
//        for (size_t i = 0; i < documents.size(); ++i) {
//            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
//        }

//        TEST(seq);
//    }
//    {
//        SearchServer search_server(dictionary[0]);
//        for (size_t i = 0; i < documents.size(); ++i) {
//            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
//        }

//        TEST(par);
//    }
//}

//int main() {
//    SearchServer search_server("and with"s);

//    int id = 0;
//    for (
//         const string& text : {
//         "funny pet and nasty rat"s,
//         "funny pet with curly hair"s,
//         "funny pet and not very nasty rat"s,
//         "pet with rat and rat and rat"s,
//         "nasty rat with curly hair"s,
//}
//         ) {
//        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
//    }

//    const string query = "curly and funny"s;

//    auto report = [&search_server, &query] {
//        cout << search_server.GetDocumentCount() << " documents total, "s
//             << search_server.FindTopDocuments(query).size() << " documents for query ["s << query << "]"s << endl;
//    };

//    report();
//    // однопоточная версия
//    search_server.RemoveDocument(5);
//    report();
//    // однопоточная версия
//    search_server.RemoveDocument(execution::seq, 1);
//    report();
//    // многопоточная версия
//    search_server.RemoveDocument(execution::par, 2);
//    report();

//    return 0;
//}

//int main() {
//    SearchServer search_server("and with"s);

//    int id = 0;
//    for (
//         const string& text : {
//         "funny pet and nasty rat"s,
//         "funny pet with curly hair"s,
//         "funny pet and not very nasty rat"s,
//         "pet with rat and rat and rat"s,
//         "nasty rat with curly hair"s,
//         }
//    ) {
//        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
//    }

//    const vector<string> queries = {
//        "nasty rat -not"s,
//        "not very funny nasty pet"s,
//        "curly hair"s
//    };
//    for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
//        cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
//    }

//    return 0;
//}

//string GenerateWord(mt19937& generator, int max_length) {
//    const int length = uniform_int_distribution(1, max_length)(generator);
//    string word;
//    word.reserve(length);
//    for (int i = 0; i < length; ++i) {
//        word.push_back(uniform_int_distribution('a', 'z')(generator));
//    }
//    return word;
//}

//vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
//    vector<string> words;
//    words.reserve(word_count);
//    for (int i = 0; i < word_count; ++i) {
//        words.push_back(GenerateWord(generator, max_length));
//    }
//    sort(words.begin(), words.end());
//    words.erase(unique(words.begin(), words.end()), words.end());
//    return words;
//}

//string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int max_word_count) {
//    const int word_count = uniform_int_distribution(1, max_word_count)(generator);
//    string query;
//    for (int i = 0; i < word_count; ++i) {
//        if (!query.empty()) {
//            query.push_back(' ');
//        }
//        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
//    }
//    return query;
//}

//vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
//    vector<string> queries;
//    queries.reserve(query_count);
//    for (int i = 0; i < query_count; ++i) {
//        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
//    }
//    return queries;
//}

//template <typename QueriesProcessor>
//void Test(string_view mark, QueriesProcessor processor, const SearchServer& search_server, const vector<string>& queries) {
//    LOG_DURATION("");
//    const auto documents_lists = processor(search_server, queries);
//}

//#define TEST(processor) Test(#processor, processor, search_server, queries)

//int main() {
//    mt19937 generator;
//    const auto dictionary = GenerateDictionary(generator, 2'000, 25);
//    const auto documents = GenerateQueries(generator, dictionary, 20'000, 10);

//    SearchServer search_server(dictionary[0]);
//    for (size_t i = 0; i < documents.size(); ++i) {
//        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
//    }

//    const auto queries = GenerateQueries(generator, dictionary, 2'000, 7);
//    TEST(ProcessQueriesJoined);
//}

//int main() {
//    SearchServer search_server("and with"s);

//    int id = 0;
//    for (
//         const string& text : {
//         "funny pet and nasty rat"s,
//         "funny pet with curly hair"s,
//         "funny pet and not very nasty rat"s,
//         "pet with rat and rat and rat"s,
//         "nasty rat with curly hair"s,
//         }
//         ) {
//        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
//    }

//    const vector<string> queries = {
//        "nasty rat -not"s,
//        "not very funny nasty pet"s,
//        "curly hair"s
//    };
//    id = 0;
//    for (
//         const auto& documents : ProcessQueries(search_server, queries)
//         ) {
//        cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << endl;
//    }

//    return 0;
//}
//int main() {
//    {
//      LOG_DURATION_STREAM("Operation time", cout);
//      TestSearchServer();
//    }

//    SearchServer search_server("and in at"s);
//    RequestQueue request_queue(search_server);

//    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
//    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
//    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
//    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
//    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});

//    // 1439 запросов с нулевым результатом
//    for (int i = 0; i < 1439; ++i) {
//        request_queue.AddFindRequest("empty request"s);
//    }
//    // все еще 1439 запросов с нулевым результатом
//    request_queue.AddFindRequest("curly dog"s);
//    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
//    request_queue.AddFindRequest("big collar"s);
//    // первый запрос удален, 1437 запросов с нулевым результатом
//    request_queue.AddFindRequest("sparrow"s);
//    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;

//    MatchDocuments(search_server, "curly cat"s);
//    FindTopDocuments(search_server, "big dog"s);
//    return 0;
//}
//-------------------------------------------------------------------------------------------------------------
