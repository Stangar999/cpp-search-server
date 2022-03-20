#include <cmath>
#include <numeric>

#include "search_server.h"
#include "string_processing.h"

using namespace std;
//-------------------------------------------------------------------------------------------------------------
SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}
//-------------------------------------------------------------------------------------------------------------
void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status,
                               const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("(document_id < 0) || (documents_.count(document_id) > 0)"s);
    }
    vector<string> words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.push_back(document_id);
}
//-------------------------------------------------------------------------------------------------------------
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query,
        [status](int, DocumentStatus document_status, int) {
            return document_status == status;});
}
//-------------------------------------------------------------------------------------------------------------
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}
//-------------------------------------------------------------------------------------------------------------
int SearchServer::GetDocumentCount() const {
    return documents_.size();
}
//-------------------------------------------------------------------------------------------------------------
int SearchServer::GetDocumentId(int index) const {
    return document_ids_.at(index);
}
//-------------------------------------------------------------------------------------------------------------
std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
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
//-------------------------------------------------------------------------------------------------------------
bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}
//-------------------------------------------------------------------------------------------------------------
std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
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
SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
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
SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
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
//-------------------------------------------------------------------------------------------------------------
double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
//-------------------------------------------------------------------------------------------------------------
