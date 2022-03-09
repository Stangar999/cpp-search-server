#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <exception>
#include <optional>

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

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
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

template <typename Iterator>
class IteratorRange{
public:
    IteratorRange(Iterator begin, Iterator end):
        begin_(begin),
        end_(end)
    {
    }
    Iterator begin() const {
        return begin_;
    }
    Iterator end() const {
        return end_;
    }
//    int size() const {
//        return distance(begin_, end_);
//    }
private:
    Iterator begin_;
    Iterator end_;
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, int size) {
        while(begin != end){
            if(static_cast<int>(distance(begin, end)) >= size){
                vecItR.push_back(IteratorRange(begin, next(begin, size)));
                advance(begin, size);
            }else{
                vecItR.push_back(IteratorRange(begin, end));
                begin = end;
            }
        }
    }
    auto begin() const {
        return vecItR.begin();
    }
    auto end() const {
        return vecItR.end();
    }
    int size() const {
        return vecItR.end() - vecItR.begin();
    }
private:
    vector<IteratorRange<Iterator>> vecItR;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

class SearchServer {
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        for(const auto& str : stop_words_){
            if(!IsValidWord(str)){
                throw invalid_argument("���� ������� � ������ �� 0 �� 31");
            }
        }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))
    {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                                   const vector<int>& ratings) {
        if ((document_id < 0) || (documents_.count(document_id) > 0)) {
            throw invalid_argument("(document_id < 0) || (documents_.count(document_id) > 0)");
        }
        vector<string> words = SplitIntoWordsNoStop(document);

        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        document_ids_.push_back(document_id);
    }

    template <typename DocumentPredicate>
     vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        auto matched_documents = FindAllDocuments(ParseQuery(raw_query), document_predicate);

        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
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
        return FindTopDocuments(
            raw_query,
            [status](int, DocumentStatus document_status, int) {
                return document_status == status;});
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    int GetDocumentId(int index) const {        
        return document_ids_.at(index);
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        Query query = ParseQuery(raw_query);
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
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                throw invalid_argument("!IsValidWord(word)");
            }
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
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        if (text.empty()) {
            throw invalid_argument("text.empty()");
        }
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
            throw invalid_argument("text.empty() || text[0] == '-' || !IsValidWord(text)");
        }
        return QueryWord{text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query result = {};
        for (const string& word : SplitIntoWords(text)) {
            QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    result.minus_words.insert(query_word.data);
                } else {
                    result.plus_words.insert(query_word.data);
                }
            }
        }
        return result;
    }

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
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};
//----------------------------------------------------------------------------
template <class T>
void RunTestImpl(T func, const string& name_func) {
    func();
    cerr << name_func << " OK"<<endl;
}

#define RUN_TEST(func) RunTestImpl((func),#func)
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

template <typename Iterator>
ostream& operator<<(ostream& out, const IteratorRange<Iterator>& Itr) {
    for (Iterator it = Itr.begin(); it != Itr.end(); ++it) {
        cout << *it;
    }
    return out;
}

ostream& operator<<(ostream& out, const Document& document) {
    out << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s /*<< endl*/;
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
// -------- ������ ��������� ������ ��������� ������� ----------

// ���� ���������, ��������� ������� ��������� � ���� ���������
void TestAddedDocumentContent() {
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
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

// ���� ���������, ��� ��������� ������� ��������� ����-����� ��� ���������� ����������
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
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
// ���� ���������, ��� ��������� ������� ��������� ��������� � ����� �������
void TestExcludeMinusWordsFromFindDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server(""s);
    {
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in the -cat"s).empty());
        ASSERT(!server.FindTopDocuments("cat"s).empty());
    }
}
// ���� ���������, ��������� ������� �� �������
void TestMatchDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server(""s);
    // ����������, ��� ������� ������� ��� �����
    {
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        const auto& [vec, st] = server.MatchDocument(content, doc_id);
        ASSERT(vec.size() == 4 && st == DocumentStatus::BANNED);
    }
    // ����������, ��� ������� ��������� ���� � ����� �������
    {
        const auto& [vec, st] = server.MatchDocument("cat in the -city"s, doc_id);
        ASSERT(vec.empty());
    }
    {
        const auto& [vec, st] = server.MatchDocument("dog"s, doc_id);
        ASSERT(vec.empty());
    }
}
// ���� ��������� ��� ��������� ������������ �� �������������
void TestSortRelevancsDocument() {
   const int doc_id1 = 41;
   const string content1 = "cat in cat the city"s;
   const int doc_id2 = 42;
   const string content2 = "dog in the city"s;
   const int doc_id3 = 43;
   const string content3 = "cat out of country"s;
   const vector<int> ratings = {1, 2, 3};
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
// ���� ���������, ������� �������� ��������
void TestAverRatingAddDoc() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings1 = {1, 2, 3};
    const vector<int> ratings2 = {1, -2, 3};
    const vector<int> ratings3 = {-1, -2, -3};
    // ����������, ��� �������� ����������� � ������� �������� ��������
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings1);
        const auto& found_docs = server.FindTopDocuments(content);
        ASSERT_EQUAL(found_docs.back().rating, 2 );
    }
    // ����������, ��� �������� ����������� � ������� �������� ��������
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings2);
        const auto& found_docs = server.FindTopDocuments(content);
        ASSERT_EQUAL(found_docs.back().rating, static_cast<int>(0.667) );
    }
    // ����������, ��� �������� ����������� � ������� �������� ��������
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings3);
        const auto& found_docs = server.FindTopDocuments(content);
        ASSERT_EQUAL(found_docs.back().rating, -2 );
    }
}

// ���� ���������, ��������
void TestPredicate() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const int doc_id1 = 43;
    const string content1 = "dog in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const vector<int> ratings1 = {2, 3, 4};
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
// ���� ���������, ������ �� ��������
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
// ���� ���������, �������������
void TestRelevance() {
   const int doc_id1 = 41;
   const string content1 = "cat in the city"s;
   const int doc_id2 = 42;
   const string content2 = "dog in the city"s;
   const int doc_id3 = 43;
   const string content3 = "cat out of country"s;
   const vector<int> ratings = {1, 2, 3};
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

void TestSearchServer() {
    RUN_TEST(TestAddedDocumentContent);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWordsFromFindDocument);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSortRelevancsDocument);
    RUN_TEST(TestAverRatingAddDoc);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatus);
    RUN_TEST(TestRelevance);
}

// --------- ��������� ��������� ������ ��������� ������� -----------

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
    } catch (const exception& e) {
        cout << "������ ���������� ��������� "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "���������� ������ �� �������: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "������ ������: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "������� ���������� �� �������: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "������ �������� ���������� �� ������ "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, {1, 1, 1});

    const auto search_results = search_server.FindTopDocuments("curly dog"s);
    int page_size = 2;

    const auto pages = Paginate(search_results, page_size);

    // ������� ��������� ��������� �� ���������
    for (auto page = pages.begin(); page != pages.end(); ++page) {
        cout << *page << endl;
        cout << "Page break"s << endl;
    }
}
