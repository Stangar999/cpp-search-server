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
#include <type_traits>

#include "document.h"
#include "log_duration.h"
#include "string_processing.h"
#include "concurrent_map.h"

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

    explicit SearchServer(const std::string_view stop_words_text);

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status,
                                   const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& , const std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& , const std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& , const std::string_view raw_query) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename ExecutPolic>
    void RemoveDocument(ExecutPolic execut_polic, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string, std::less<>> stop_words_;
    /** Хранит string, все осталные контейнеры используют string_view на эти string */
    std::set<std::string> words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> documents_words_freqs_;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const ;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery( std::string_view text, bool is_del_copy = true) const;

    /** Удаляет повторяющиеся элементы из вектора, вектор получается отсортированным */
    void DelCopyElemVec(std::vector<std::string_view>& vec) const;

    double ComputeWordInverseDocumentFreq(const std::string_view &word) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&&, const Query& query, DocumentPredicate document_predicate) const;
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
template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& execpolicy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    auto matched_documents = FindAllDocuments(execpolicy, ParseQuery(raw_query), document_predicate);
    std::sort(execpolicy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
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
template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& execpolicy, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execpolicy, raw_query,
                            [status](int, DocumentStatus document_status, int) {
                                return document_status == status;} );
}
//-------------------------------------------------------------------------------------------------------------
template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& execpolicy, const std::string_view raw_query) const {
    return FindTopDocuments(execpolicy, raw_query, DocumentStatus::ACTUAL);
}
//-------------------------------------------------------------------------------------------------------------
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}
//-------------------------------------------------------------------------------------------------------------
template<typename ExecutionPolicy,typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& execpolicy, const Query &query, DocumentPredicate document_predicate) const
{
    const bool is_excpolicy_par = std::is_same_v <std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>;
    size_t bucket_count = 1;
    if constexpr ( is_excpolicy_par ){
        bucket_count = 64;
    }
    ConcurrentMap<int, double> document_to_relevance(bucket_count);
    for_each(execpolicy,
             query.plus_words.begin(), query.plus_words.end(),
             [document_predicate, &document_to_relevance, this](std::string_view word)
        {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
    );

    for_each(execpolicy,
             query.minus_words.begin(), query.minus_words.end(),
             [&document_to_relevance, this](std::string_view word)
        {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
    );
    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
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

   std::vector<const std::string_view*> words_to_delete(documents_words_freqs_.at(document_id).size());

    transform(execut,
              documents_words_freqs_.at(document_id).begin(), documents_words_freqs_.at(document_id).end(),
              words_to_delete.begin(),
              [](const std::pair<const std::string_view, double>& par){
                  return &par.first;
              }
    );

    std::for_each(execut,
                  words_to_delete.begin(), words_to_delete.end(),
                  [this, document_id](const std::string_view* str_v){
                      word_to_document_freqs_[*str_v].erase(document_id);
    });

    documents_words_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}
//-------------------------------------------------------------------------------------------------------------
