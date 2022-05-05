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

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

//    void RemoveDocument(std::execution::parallel_policy, int document_id);

//    void RemoveDocument(std::execution::sequenced_policy, int document_id);

    template <typename Execution>
    void RemoveDocument(Execution execut, int document_id);

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
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string& text) const;

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
template <typename Execution>
void SearchServer::RemoveDocument(Execution execut, int document_id){
    if(!document_ids_.count(document_id)){
        return;
    }

    std::vector<std::string> words_to_delete;
    words_to_delete.reserve(1000000);
    for(const auto& [word, _]: documents_words_freqs_[document_id]) {
        words_to_delete.push_back(word);
    }

    std::vector<bool> tmp(100000);

    transform(execut,
              words_to_delete.begin(), words_to_delete.end(),
              tmp.begin(),
              [this, document_id] (const std::string& word) {
        word_to_document_freqs_[word].erase(document_id);
        return true;
    });

    documents_words_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

//    if(!document_ids_.count(document_id)){
//        return;
//    }
//    std::vector<std::string> words_to_delete;
//    words_to_delete.reserve(100000);
//    for(const auto& [word, _]: documents_words_freqs_[document_id]) {
//        words_to_delete.push_back(word);
//    }

//    std::vector<bool> tmp(100000);
//    //tmp.reserve(100000);

//    transform(execut,
//              words_to_delete.begin(), words_to_delete.end(),
//              tmp.begin(),
//              [this, document_id] (const std::string& word) {
//        word_to_document_freqs_[word].erase(document_id);
//        return true;
//    });

//    documents_words_freqs_.erase(document_id);
//    documents_.erase(document_id);
//    document_ids_.erase(document_id);
//}
