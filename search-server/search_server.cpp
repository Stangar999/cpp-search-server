#include <cmath>
#include <numeric>

#include "search_server.h"
#include "string_processing.h"

using namespace std;
//-------------------------------------------------------------------------------------------------------------
SearchServer::SearchServer(const string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}
//-------------------------------------------------------------------------------------------------------------
SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}
//-------------------------------------------------------------------------------------------------------------
void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status,
                               const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("(document_id < 0) || (documents_.count(document_id) > 0)"s);
    }
    vector<string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (string_view word : words) {
        auto par = words_.insert(string(word));
            word_to_document_freqs_[*par.first][document_id] += inv_word_count;
            documents_words_freqs_[document_id][*par.first] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}
//-------------------------------------------------------------------------------------------------------------
std::vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, status);
}
//-------------------------------------------------------------------------------------------------------------
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}
//-------------------------------------------------------------------------------------------------------------
int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}
//-------------------------------------------------------------------------------------------------------------
std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.cbegin();
}
//-------------------------------------------------------------------------------------------------------------
std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.cend();
}
//-------------------------------------------------------------------------------------------------------------
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    if(!document_ids_.count(document_id)){
        throw out_of_range("id fail");
    }
    Query query = ParseQuery(raw_query);

    for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {{}, documents_.at(document_id).status};
        }
    }

    vector<string_view> matched_words;
    for (string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return {matched_words, documents_.at(document_id).status};
}
//-------------------------------------------------------------------------------------------------------------
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, const std::string_view raw_query, int document_id) const {
    if(!document_ids_.count(document_id)){
        throw std::out_of_range("id fail");
    }
    Query query = ParseQuery(raw_query, false);
    bool is_was_minus = any_of( execution::par,
                                query.minus_words.begin(), query.minus_words.end(),
                                [this, document_id](const std::string_view& word){
        if (word_to_document_freqs_.count(word) != 0) {
            if (word_to_document_freqs_.at(word).count(document_id)) {
                return true;
            }
        }
        return false;
    } );

    if(is_was_minus){
        return {{}, documents_.at(document_id).status};
    }
    std::vector<std::string_view> matched_words(query.plus_words.size());
    std::vector<std::string_view>::iterator it_last_elem = std::copy_if(
                 execution::par,
                 query.plus_words.begin(), query.plus_words.end(),
                 matched_words.begin(),
                 [this, document_id](const std::string_view& word){
        if (word_to_document_freqs_.count(word) != 0) {
            if (word_to_document_freqs_.at(word).count(document_id)) {
                return true;
            }
        }
        return false;
    });
    matched_words.erase(it_last_elem, matched_words.end());
    DelCopyElemVec(matched_words);
    return {matched_words, documents_.at(document_id).status};
}
//-------------------------------------------------------------------------------------------------------------
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy, const string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}
//-------------------------------------------------------------------------------------------------------------
const std::map<string_view, double> &SearchServer::GetWordFrequencies(int document_id) const
{
    if(!documents_words_freqs_.count(document_id)){ //документа с таким id нет
        static std::map<std::string_view, double> empty;
        return empty;
    }
    return documents_words_freqs_.at(document_id);
}
//-------------------------------------------------------------------------------------------------------------
void SearchServer::RemoveDocument(int document_id)
{
    if(!document_ids_.count(document_id)){
        return;
    }
    for(const auto& [word, _] : documents_words_freqs_.at(document_id)){
        word_to_document_freqs_.at(word).erase(document_id);
    }
    documents_words_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}
//-------------------------------------------------------------------------------------------------------------
bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}
//-------------------------------------------------------------------------------------------------------------
bool SearchServer::IsValidWord(string_view word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}
//-------------------------------------------------------------------------------------------------------------
vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("!IsValidWord(word)"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}
//-------------------------------------------------------------------------------------------------------------
int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(),ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}
//-------------------------------------------------------------------------------------------------------------
SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
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
//-------------------------------------------------------------------------------------------------------------
SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool is_del_copy) const {
    Query result = {};
    vector <string_view> vec_uniq(SplitIntoWords(text));
    if(is_del_copy){
        DelCopyElemVec(vec_uniq);
    }
    for (string_view word : vec_uniq) {
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}
//-------------------------------------------------------------------------------------------------------------
void SearchServer::DelCopyElemVec(vector<string_view> &vec) const
{
    sort(vec.begin(), vec.end());
    auto last = unique(vec.begin(), vec.end());
    vec.erase(last, vec.end());
}
//-------------------------------------------------------------------------------------------------------------
double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
//-------------------------------------------------------------------------------------------------------------
