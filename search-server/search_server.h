#pragma once

#include <string>
#include <stdexcept>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <utility>
#include <algorithm>
#include <optional>
#include <execution>
#include <cassert>
#include <functional>

#include "document.h"
#include "string_processing.h"

using namespace std::string_literals;

constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double EPSILON = 1e-6;
//-------------------------------------------------------------------------------------------------------------
class SearchServer {
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, const std::string& document, DocumentStatus status,
                                   const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

//    template <typename ExecutPolic>
//    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(ExecutPolic execut_polic, const std::string& raw_query, int document_id) const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string& raw_query, int document_id) const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string& raw_query, int document_id) const;

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename ExecutPolic>
    void RemoveDocument(ExecutPolic execut_polic, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::map<int, std::map<std::string, double>> documents_words_freqs_;

    bool IsStopWord(const std::string& word) const;

    static bool IsValidWord(const std::string& word);

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string text) const;

    struct Query {
        std::vector<std::string> plus_words;
        std::vector<std::string> minus_words;
    };

    Query ParseQuery(const std::string& text, bool is_del_copy = true) const;

//    template <typename ExecutPolic>
//    Query ParseQuery(ExecutPolic execut_polic, const std::string& text) const;

    /** Удаляет повторяющиеся элементы из вектора, вектор получается отсортированным */
    void DelCopyElemVec(std::vector<std::string>& vec) const;

    /** Удаляет повторяющиеся элементы из вектора, вектор получается отсортированным */
    template <typename ExecutPolic>
    void DelCopyElemVec(ExecutPolic execut_polic, std::vector<std::string>& vec) const;

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
};
//----------------------------------------------------------------------------
template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    for(const auto& str : stop_words_){
        if(!IsValidWord(str)){
            throw std::invalid_argument("есть символы с кодами от 0 до 31"s);
        }
    }
}
//-------------------------------------------------------------------------------------------------------------
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {
    auto matched_documents = FindAllDocuments(ParseQuery(raw_query), document_predicate);

    std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
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
//-------------------------------------------------------------------------------------------------------------
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) { //
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}
//-------------------------------------------------------------------------------------------------------------
template <typename ExecutPolyc>
void SearchServer::RemoveDocument(ExecutPolyc execut, int document_id){
    if(!document_ids_.count(document_id)){
        return;
    }

   std::vector<const std::string*> words_to_delete(documents_words_freqs_.at(document_id).size());

    transform(execut,
              documents_words_freqs_.at(document_id).begin(), documents_words_freqs_.at(document_id).end(),
              words_to_delete.begin(),
              [](const std::pair<const std::string, double>& par){
                  return &par.first;
              }
    );

    std::for_each(execut,
                  words_to_delete.begin(), words_to_delete.end(),
                  [this, document_id](const std::string* str_v){
                      word_to_document_freqs_[*str_v].erase(document_id);
    });

    documents_words_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}
//-------------------------------------------------------------------------------------------------------------
//template <typename ExecutPolic>
//SearchServer::Query SearchServer::ParseQuery(ExecutPolic execut_polic, const std::string& text) const {
//    Query result = {};
//    //std::vector <std::string> words(SplitIntoWords(text));
//    for (const std::string& word : SplitIntoWords(text)) {
//        QueryWord query_word = ParseQueryWord(word);
//        if (!query_word.is_stop) {
//            if (query_word.is_minus) {
//                result.minus_words.push_back(query_word.data);
//            } else {
//                result.plus_words.push_back(query_word.data);
//            }
//        }
//    }
//    return result;
//}
//-------------------------------------------------------------------------------------------------------------

//result.minus_words.resize(vec_uniq.size());
//std::transform(execut_polic,
//         vec_uniq.begin(), vec_uniq.end(),
//         result.minus_words.begin(),
//         [this](const std::string& word){
//    QueryWord query_word = ParseQueryWord(word);
//    if (!query_word.is_stop) {
//        if (query_word.is_minus) {
//           return word;
//        }
//    }
//    return std::string{};
//});
//DelCopyElemVec(execut_polic, result.minus_words);
//if(result.minus_words.front() == ""){
//    result.minus_words.erase(result.minus_words.begin());
//}

//result.plus_words.resize(vec_uniq.size());
//std::transform(execut_polic,
//         vec_uniq.begin(), vec_uniq.end(),
//         result.plus_words.begin(),
//         [this](const std::string& word){
//    QueryWord query_word = ParseQueryWord(word);
//    if (!query_word.is_stop) {
//        if (!query_word.is_minus) {
//           return word;
//        }
//    }
//    return std::string{};
//});
//DelCopyElemVec(execut_polic, result.plus_words);
//if(result.plus_words.front() == ""){
//    result.plus_words.erase(result.plus_words.begin());
//}

//    for (const std::string& word : vec_uniq) {
//        QueryWord query_word = ParseQueryWord(word);
//        if (!query_word.is_stop) {
//            if (query_word.is_minus) {
//                result.minus_words.push_back(query_word.data);
//            } else {
//                result.plus_words.push_back(query_word.data);
//            }
//        }
//    }
//    return result;
//}

//    const size_t count_minus_word = transform_reduce(
//        execut_polic,
//        vec_uniq.begin(), vec_uniq.end(),
//        0,
//        std::plus<>{},
//        [this](const std::string& word) {
//        return ParseQueryWord(word).is_minus;
//    }
//    );

//    const size_t count_plus_word = transform_reduce(
//        execut_polic,
//        vec_uniq.begin(), vec_uniq.end(),
//        0,
//        std::plus<>{},
//        [this](const std::string& word) {
//        QueryWord query_word = ParseQueryWord(word);
//        return (!query_word.is_minus && !query_word.is_stop);
//    }
//    );

//    std::transform(execut_polic,
//                   vec_uniq.begin(), vec_uniq.end(),
//                   result.minus_words.begin(),
//                   [](const std::string& word){
//            if (query_word.is_minus) {
//                result.minus_words.push_back(query_word.data);
//            } else {
//                result.plus_words.push_back(query_word.data);
//            }
//        }
//    });


//    std::for_each(execut_polic,
//             vec_uniq.begin(), vec_uniq.end(),
//             [this, &result](const std::string& word){
//        QueryWord query_word = ParseQueryWord(word);
//        if (!query_word.is_stop) {
//            if (query_word.is_minus) {
//                result.minus_words.push_back(query_word.data);
//            } else {
//                result.plus_words.push_back(query_word.data);
//            }
//        }
//    });
//-------------------------------------------------------------------------------------------------------------
//template <typename ExecutPolic>
//void SearchServer::DelCopyElemVec(ExecutPolic execut_polic, std::vector<std::string>& vec) const{
//    std::sort(execut_polic, vec.begin(), vec.end());
//    std::vector<std::string>::iterator last = std::unique(vec.begin(), vec.end());
//    vec.erase(last, vec.end());
//}
//-------------------------------------------------------------------------------------------------------------
//template <typename ExecutPolic>
//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(ExecutPolic execut_polic, const std::string& raw_query, int document_id) const {
//    if(!document_ids_.count(document_id)){
//        throw std::out_of_range("id fail");
//    }
//    Query query = ParseQuery(execut_polic, raw_query);
//    DelCopyElemVec(execut_polic, query.minus_words);
//    bool is_was_minus = any_of(execut_polic,
//              query.minus_words.begin(), query.minus_words.end(),
//              [this, document_id](const std::string& word){
//        if (word_to_document_freqs_.count(word) != 0) { // TODO можно ли в разных потоках читать так ?
//            if (word_to_document_freqs_.at(word).count(document_id)) { // TODO можно ли в разных потоках читать так ?
//                return true;
//            }
//        }
//        return false;
//    } );


//    if(is_was_minus){
//        return {{}, documents_.at(document_id).status};
//    }

//    std::vector<std::string> matched_words(query.plus_words.size());
//    DelCopyElemVec(execut_polic, query.plus_words);

//    std::vector<std::string>::iterator it_last_elem = std::copy_if(
//                 std::execution::seq,
//                 query.plus_words.begin(), query.plus_words.end(),
//                 matched_words.begin(),
//                 [this, document_id](const std::string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return true;
//            }
//        }
//        return false;
//    });

//    matched_words.erase(it_last_elem, matched_words.end());

//    return {matched_words, documents_.at(document_id).status};
//}
//    std::transform(execut_polic,
//              query.plus_words.begin(), query.plus_words.end(),
//              matched_words.begin(),
//             [this, document_id](const std::string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return word;
//            }
//        }
//        return std::string{};
//    });
//    DelCopyElemVec(execut_polic, matched_words);
//    if(matched_words.front() == ""){
//        matched_words.erase(matched_words.begin());
//    }
//-------------------------------------------------------------------------------------------------------------
//#pragma once

//#include <string>
//#include <stdexcept>
//#include <vector>
//#include <list>
//#include <map>
//#include <set>
//#include <utility>
//#include <algorithm>
//#include <optional>
//#include <execution>
//#include <cassert>

//#include "document.h"
//#include "string_processing.h"

//using namespace std::string_literals;

//constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;
//constexpr double EPSILON = 1e-6;
////-------------------------------------------------------------------------------------------------------------
//class SearchServer {
//public:
//    inline static constexpr int INVALID_DOCUMENT_ID = -1;

//    template <typename StringContainer>
//    explicit SearchServer(const StringContainer& stop_words);

//    explicit SearchServer(const std::string& stop_words_text);

//    void AddDocument(int document_id, const std::string& document, DocumentStatus status,
//                                   const std::vector<int>& ratings);

//    template <typename DocumentPredicate>
//    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;

//    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

//    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

//    int GetDocumentCount() const;

//    std::set<int>::const_iterator begin() const;

//    std::set<int>::const_iterator end() const;

//    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

//    template <typename ExecutPolic>
//    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(ExecutPolic execut_polic, const std::string& raw_query, int document_id) const;

//    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string& raw_query, int document_id) const;

//    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string& raw_query, int document_id) const;

//    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

//    void RemoveDocument(int document_id);

//    template <typename ExecutPolic>
//    void RemoveDocument(ExecutPolic execut_polic, int document_id);

//private:
//    struct DocumentData {
//        int rating;
//        DocumentStatus status;
//    };
//    const std::set<std::string> stop_words_;
//    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
//    std::map<int, DocumentData> documents_;
//    std::set<int> document_ids_;
//    std::map<int, std::map<std::string, double>> documents_words_freqs_;

//    bool IsStopWord(const std::string& word) const;

//    static bool IsValidWord(const std::string& word);

//    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

//    static int ComputeAverageRating(const std::vector<int>& ratings);

//    struct QueryWord {
//        std::string data;
//        bool is_minus;
//        bool is_stop;
//    };

//    QueryWord ParseQueryWord(std::string text) const;

//    struct Query {
//        std::vector<std::string> plus_words;
//        std::vector<std::string> minus_words;
//    };

//    Query ParseQuery(const std::string& text) const;

//    template <typename ExecutPolic>
//    Query ParseQuery(ExecutPolic execut_polic, const std::string& text) const;

//    /** Удаляет повторяющиеся элементы из вектора, вектор получается отсортированным */
//    void DelCopyElemVec(std::vector<std::string>& vec) const;

//    /** Удаляет повторяющиеся элементы из вектора, вектор получается отсортированным */
//    template <typename ExecutPolic>
//    void DelCopyElemVec(ExecutPolic execut_polic, std::vector<std::string> &vec) const;

//    double ComputeWordInverseDocumentFreq(const std::string& word) const;

//    template <typename DocumentPredicate>
//    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
//};
////----------------------------------------------------------------------------
//template <typename StringContainer>
//SearchServer::SearchServer(const StringContainer& stop_words)
//    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
//    for(const auto& str : stop_words_){
//        if(!IsValidWord(str)){
//            throw std::invalid_argument("есть символы с кодами от 0 до 31"s);
//        }
//    }
//}
////-------------------------------------------------------------------------------------------------------------
//template <typename DocumentPredicate>
//std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {
//    auto matched_documents = FindAllDocuments(ParseQuery(raw_query), document_predicate);

//    std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
//        if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
//            return lhs.rating > rhs.rating;
//        } else {
//            return lhs.relevance > rhs.relevance;
//        }
//    });
//    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
//        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
//    }
//    return matched_documents;
//}
////-------------------------------------------------------------------------------------------------------------
//template <typename DocumentPredicate>
//std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
//    std::map<int, double> document_to_relevance;
//    for (const std::string& word : query.plus_words) {
//        if (word_to_document_freqs_.count(word) == 0) {
//            continue;
//        }
//        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
//        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) { //
//            const auto& document_data = documents_.at(document_id);
//            if (document_predicate(document_id, document_data.status, document_data.rating)) {
//                document_to_relevance[document_id] += term_freq * inverse_document_freq;
//            }
//        }
//    }

//    for (const std::string& word : query.minus_words) {
//        if (word_to_document_freqs_.count(word) == 0) {
//            continue;
//        }
//        for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
//            document_to_relevance.erase(document_id);
//        }
//    }

//    std::vector<Document> matched_documents;
//    for (const auto& [document_id, relevance] : document_to_relevance) {
//        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
//    }
//    return matched_documents;
//}
////-------------------------------------------------------------------------------------------------------------
//template <typename ExecutPolyc>
//void SearchServer::RemoveDocument(ExecutPolyc execut, int document_id){
//    if(!document_ids_.count(document_id)){
//        return;
//    }

//   std::vector<const std::string*> words_to_delete(documents_words_freqs_.at(document_id).size());

//    transform(execut,
//              documents_words_freqs_.at(document_id).begin(), documents_words_freqs_.at(document_id).end(),
//              words_to_delete.begin(),
//              [](const std::pair<const std::string, double>& par){
//                  return &par.first;
//              }
//    );

//    std::for_each(execut,
//                  words_to_delete.begin(), words_to_delete.end(),
//                  [this, document_id](const std::string* str_v){
//                      word_to_document_freqs_[*str_v].erase(document_id);
//    });

//    documents_words_freqs_.erase(document_id);
//    documents_.erase(document_id);
//    document_ids_.erase(document_id);
//}
////-------------------------------------------------------------------------------------------------------------
//template <typename ExecutPolic>
//SearchServer::Query SearchServer::ParseQuery(ExecutPolic execut_polic, const std::string& text) const {
//    Query result = {};
//    std::vector <std::string> vec_uniq(SplitIntoWords(text));
//    //DelCopyElemVec(execut_polic, vec_uniq);
////    for (const std::string& word : vec_uniq) {
////        QueryWord query_word = ParseQueryWord(word);
////        if (!query_word.is_stop) {
////            if (query_word.is_minus) {
////                result.minus_words.push_back(query_word.data);
////            } else {
////                result.plus_words.push_back(query_word.data);
////            }
////        }
////    }
//    std::for_each(execut_polic,
//             vec_uniq.begin(), vec_uniq.end(),
//             [this, &result](const std::string& word){
//        QueryWord query_word = ParseQueryWord(word);
//        if (!query_word.is_stop) {
//            if (query_word.is_minus) {
//                result.minus_words.push_back(query_word.data);
//            } else {
//                result.plus_words.push_back(query_word.data);
//            }
//        }
//    });
//    return result;
//}
////-------------------------------------------------------------------------------------------------------------
//template <typename ExecutPolic>
//void SearchServer::DelCopyElemVec(ExecutPolic execut_polic, std::vector<std::string> &vec) const{
//    std::sort(execut_polic, vec.begin(), vec.end());
//    auto last = std::unique(execut_polic, vec.begin(), vec.end());
//    vec.erase(last, vec.end());
//}
////-------------------------------------------------------------------------------------------------------------
//template <typename ExecutPolic>
//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(ExecutPolic execut_polic, const std::string& raw_query, int document_id) const {
//    if(!document_ids_.count(document_id)){
//        throw std::out_of_range("id fail");
//    }
//    Query query = ParseQuery(execut_polic, raw_query);
//    DelCopyElemVec(execut_polic, query.minus_words);
//    bool is_was_minus = any_of(execut_polic,
//              query.minus_words.begin(), query.minus_words.end(),
//              [this, document_id](const std::string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return true;
//            }
//        }
//        return false;
//    } );

//    std::vector<std::string> matched_words;
//    if(is_was_minus){
//        return {matched_words, documents_.at(document_id).status};
//    }
//    matched_words.reserve(documents_words_freqs_.at(document_id).size());
//    DelCopyElemVec(execut_polic, query.plus_words);
//    for_each(execut_polic,
//              query.plus_words.begin(), query.plus_words.end(),
//             [this, document_id, &matched_words](const std::string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                matched_words.push_back(word);
//            }
//        }
//    });

//    return {matched_words, documents_.at(document_id).status};
//}
////-------------------------------------------------------------------------------------------------------------
