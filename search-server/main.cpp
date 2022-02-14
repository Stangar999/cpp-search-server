#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;


const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;
const size_t SINGL_RSLT = 1;

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
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
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
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int /*document_id*/, DocumentStatus document_status, int /*rating*/) {
            return document_status == status; });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
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
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
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
        int rating_sum = accumulate(ratings.begin(),ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
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
                } else {
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

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
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

/* Подставьте вашу реализацию класса SearchServer сюда */
//----------------------------------------------------------------------------
template <class T>
void RunTestImpl(T func, const string& name_func) {
    func();
    cerr << name_func << " OK"<<endl;
}

#define RUN_TEST(func) RunTestImpl((func),#func) // напишите недостающий код
//----------------------------------------------------------------------------
template <typename T1, typename T2>
ostream& operator<<(ostream& out, const pair<T1, T2>& container) {
    out << "("s << container.first<<", "<<container.second << ")"s;
    return out;
}

template <typename Container>
void Print(ostream& out, const Container& container) {
    bool is_first = true;
    for (const auto& element : container) {
        if (!is_first) {
            out << ", "s;
        }
        is_first = false;
        out << element;
    }
}

template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

template <typename Element>
ostream& operator<<(ostream& out, const set<Element>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename T1, typename T2>
ostream& operator<<(ostream& out, const map<T1,T2>& container) {
    out << "<"s;
    Print(out, container);
    out << ">"s;
    return out;
}
//----------------------------------------------------------------------------
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, поисковая система добавляет и ищет документы
void TestAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;
    // убеждаемся, что пустой документ найти нельзя,
    {
        server.AddDocument(doc_id, "", DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT(found_docs.empty());
    }
    // убеждаемся, что документ можно найти по слову,
    {
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT_EQUAL(found_docs.size(), SINGL_RSLT);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), SINGL_RSLT);
        ASSERT_EQUAL(found_docs.back().id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}
// Тест проверяет, что поисковая система исключает документы с минус словами
void TestExcludeMinusWordsFromFindDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // убеждаемся, что поиск c минус словом не даст результатов
    SearchServer server;
    {
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in the -cat"s).empty());
        ASSERT(!server.FindTopDocuments("cat"s).empty());
    }
}
// Тест проверяет, поисковая систему на матчинг
void TestMatchDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;
    // убеждаемся, что матчинг находит все слова
    {
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        const auto& [vec, st] = server.MatchDocument(content, doc_id);
        ASSERT(vec.size() == 4 && st == DocumentStatus::BANNED);
    }
    // убеждаемся, что матчинг исключает доки с минус словами
    {
        const auto& [vec, st] = server.MatchDocument("cat in the -city"s, doc_id);
        ASSERT(vec.empty());
    }
    {
        const auto& [vec, st] = server.MatchDocument("dog"s, doc_id);
        ASSERT(vec.empty());
    }
}
// Тест проверяет что результат отсортирован по релевантности
void TestSortRelevancsDocument() {
   const int doc_id1 = 41;
   const string content1 = "cat in cat the city"s;
   const int doc_id2 = 42;
   const string content2 = "dog in the city"s;
   const int doc_id3 = 43;
   const string content3 = "cat out of country"s;
   const vector<int> ratings = {1, 2, 3};
   {
       SearchServer server;
       server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
       const auto found_docs = server.FindTopDocuments("cat in the city"s);
       ASSERT(found_docs.size() == 3 && found_docs[0].relevance > found_docs[1].relevance > found_docs[2].relevance);
   }
}
// Тест проверяет, среднее аримфмет рейтинга
void TestAverRatingAddDoc() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings1 = {1, 2, 3};
    const vector<int> ratings2 = {1, -2, 3};
    const vector<int> ratings3 = {-1, -2, -3};
    // убеждаемся, что документ добавляется с среднее аримфмет рейтинга
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings1);
        const auto& found_docs = server.FindTopDocuments(content);
        ASSERT_EQUAL(found_docs.back().rating, 2 );
    }
    // убеждаемся, что документ добавляется с среднее аримфмет рейтинга
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings2);
        const auto& found_docs = server.FindTopDocuments(content);
        ASSERT_EQUAL(found_docs.back().rating, static_cast<int>(0.667) );
    }
    // убеждаемся, что документ добавляется с среднее аримфмет рейтинга
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings3);
        const auto& found_docs = server.FindTopDocuments(content);
        ASSERT_EQUAL(found_docs.back().rating, -2 );
    }
}

// Тест проверяет, предикат
void TestPredicate() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const int doc_id1 = 43;
    const string content1 = "dog in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const vector<int> ratings1 = {2, 3, 4};
    {
        SearchServer server;
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
        SearchServer server;
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
        SearchServer server;
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
// Тест проверяет, фильтр по статутсу
void TestStatus() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const int doc_id1 = 43;
    const string content1 = "dog in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const vector<int> ratings1 = {2, 3, 4};
    const int doc_id2 = 1;
    const int doc_id3 = 2;
    {
        SearchServer server;
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
// Тест проверяет, релевантность
void TestRelevancs() {
   const int doc_id1 = 41;
   const string content1 = "cat in the city"s;
   const int doc_id2 = 42;
   const string content2 = "dog in the city"s;
   const int doc_id3 = 43;
   const string content3 = "cat out of country"s;
   const vector<int> ratings = {1, 2, 3};
   {
       SearchServer server;
       server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
       server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
       const auto found_docs = server.FindTopDocuments("cat in the city"s);
       ASSERT(found_docs.size() == 3 && found_docs[0].relevance - 0.405465 < EPSILON &&
                                        found_docs[1].relevance - 0.304099 < EPSILON &&
                                        found_docs[2].relevance - 0.101366 < EPSILON);
   }
}
/*
Разместите код остальных тестов здесь
*/

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddedDocumentContent);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWordsFromFindDocument);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSortRelevancsDocument);
    RUN_TEST(TestAverRatingAddDoc);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatus);
    RUN_TEST(TestRelevancs);

    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}


// ==================== для примера =========================


//void PrintDocument(const Document& document) {
//    cout << "{ "s
//         << "document_id = "s << document.id << ", "s
//         << "relevance = "s << document.relevance << ", "s
//         << "rating = "s << document.rating
//         << " }"s << endl;
//}

//int main() {
//    SearchServer search_server;
//    search_server.SetStopWords("и в на"s);

//    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
//    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
//    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
//    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

//    cout << "ACTUAL by default:"s << endl;
//    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
//        PrintDocument(document);
//    }

//    cout << "BANNED:"s << endl;
//    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
//        PrintDocument(document);
//    }

//    cout << "Even ids:"s << endl;
//    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s,
//                                                                   [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
//        PrintDocument(document);
//    }
//}
