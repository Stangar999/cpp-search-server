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
        documents_words_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
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
std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    if(!document_ids_.count(document_id)){
        throw out_of_range("id fail");
    }
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
std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, const std::string& raw_query, int document_id) const {
    if(!document_ids_.count(document_id)){
        throw std::out_of_range("id fail");
    }
    Query query = ParseQuery(raw_query, false);
    DelCopyElemVec(query.minus_words);
    bool is_was_minus = any_of( execution::seq,
                                query.minus_words.begin(), query.minus_words.end(),
                                [this, document_id](const std::string& word){
        if (word_to_document_freqs_.count(word) != 0) { // TODO можно ли в разных потоках читать так ?
            if (word_to_document_freqs_.at(word).count(document_id)) { // TODO можно ли в разных потоках читать так ?
                return true;
            }
        }
        return false;
    } );


    if(is_was_minus){
        return {{}, documents_.at(document_id).status};
    }

    std::vector<std::string> matched_words(query.plus_words.size());
    DelCopyElemVec(query.plus_words);

    std::vector<std::string>::iterator it_last_elem = std::copy_if(
                 execution::seq,
                 query.plus_words.begin(), query.plus_words.end(),
                 matched_words.begin(),
                 [this, document_id](const std::string& word){
        if (word_to_document_freqs_.count(word) != 0) {
            if (word_to_document_freqs_.at(word).count(document_id)) {
                return true;
            }
        }
        return false;
    });

    matched_words.erase(it_last_elem, matched_words.end());
    return {matched_words, documents_.at(document_id).status};
}
//-------------------------------------------------------------------------------------------------------------
std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy, const std::string& raw_query, int document_id) const {
    if(!document_ids_.count(document_id)){
        throw out_of_range("id fail");
    }
    return MatchDocument(raw_query, document_id);
}
//-------------------------------------------------------------------------------------------------------------
//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, const std::string& raw_query, int document_id) const {
//    if(!document_ids_.count(document_id)){
//        throw out_of_range("id fail");
//    }
//    Query query = ParseQuery(raw_query, execution::par);
//    DelCopyElemVec(query.minus_words, execution::par);
//    bool is_was_minus = any_of(execution::par,
//              query.minus_words.begin(), query.minus_words.end(),
//              [this, document_id](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return true;
//            }
//        }
//        return false;
//    } );

//    vector<string> matched_words;
//    if(is_was_minus){
//        return {matched_words, documents_.at(document_id).status};
//    }
//    //matched_words.reserve(documents_words_freqs_.at(document_id).size());
//    DelCopyElemVec(query.plus_words, execution::par);
//    for_each(execution::par,
//              query.plus_words.begin(), query.plus_words.end(),
//             [this, document_id, &matched_words](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                matched_words.push_back(word);
//            }
//        }
//    });

//    return {matched_words, documents_.at(document_id).status};
//}
//-------------------------------------------------------------------------------------------------------------
//    Query query = ParseQuery(raw_query);
//    //DelCopyElemVec(query.minus_words);
//    bool is_was_minus = any_of(execution::seq,
//              query.minus_words.begin(), query.minus_words.end(),
//              [this, document_id](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return true;
//            }
//        }
//        return false;
//    } );

//    vector<string> matched_words;
//    if(is_was_minus){
//        return {matched_words, documents_.at(document_id).status};
//    }
//    matched_words.reserve(documents_words_freqs_.at(document_id).size());
//    //DelCopyElemVec(query.plus_words);
//    for_each(execution::seq,
//              query.plus_words.begin(), query.plus_words.end(),
//             [this, document_id, &matched_words](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                matched_words.push_back(word);
//            }
//        }
//    });

//    return {matched_words, documents_.at(document_id).status};
//}
//-------------------------------------------------------------------------------------------------------------
//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, const std::string& raw_query, int document_id) const {
//    if(!document_ids_.count(document_id)){
//        throw out_of_range("id fail");
//    }
//    Query query = ParseQuery(execution::seq, raw_query);
//    DelCopyElemVec(execution::seq, query.minus_words);
//    bool is_was_minus = any_of(execution::seq,
//              query.minus_words.begin(), query.minus_words.end(),
//              [this, document_id](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return true;
//            }
//        }
//        return false;
//    } );

//    vector<string> matched_words;
//    if(is_was_minus){
//        return {matched_words, documents_.at(document_id).status};
//    }
//    matched_words.reserve(documents_words_freqs_.at(document_id).size());
//    DelCopyElemVec(execution::seq, query.plus_words);
//    for_each(execution::seq,
//              query.plus_words.begin(), query.plus_words.end(),
//             [this, document_id, &matched_words](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                matched_words.push_back(word);
//            }
//        }
//    });

//    return {matched_words, documents_.at(document_id).status};
//}
//-------------------------------------------------------------------------------------------------------------
//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, const std::string& raw_query, int document_id) const {
//    if(!document_ids_.count(document_id)){
//        throw out_of_range("id fail");
//    }
//    Query query = ParseQuery(raw_query);
//    DelCopyElemVec(query.minus_words);
//    bool is_was_minus = any_of(execution::par,
//              query.minus_words.begin(), query.minus_words.end(),
//              [this, document_id](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return true;
//            }
//        }
//        return false;
//    } );

//    vector<string> matched_words;
//    if(is_was_minus){
//        return {matched_words, documents_.at(document_id).status};
//    }
//    matched_words.reserve(documents_words_freqs_.at(document_id).size());
//    DelCopyElemVec(query.plus_words);
//    for_each(execution::par,
//              query.plus_words.begin(), query.plus_words.end(),
//             [this, document_id, &matched_words](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                matched_words.push_back(word);
//            }
//        }
//    });

//    return {matched_words, documents_.at(document_id).status};
//}

//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, const std::string& raw_query, int document_id) const {
//    return MatchDocument(raw_query, document_id);
//}
//    transform(execution::par,
//              query.plus_words.begin(), query.plus_words.end(),
//              matched_words.begin(),
//              [this, document_id](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return word;
//            }
//        }
//    });

//    for (const string& word : query.plus_words) {
//        if (word_to_document_freqs_.count(word) == 0) {
//            continue;
//        }
//        if (word_to_document_freqs_.at(word).count(document_id)) {
//            matched_words.push_back(word);
//        }
//    }
//    for (const string& word : query.minus_words) {
//        if (word_to_document_freqs_.count(word) == 0) {
//            continue;
//        }
//        if (word_to_document_freqs_.at(word).count(document_id)) {
//            matched_words.clear();
//            break;
//        }
//    }
//-------------------------------------------------------------------------------------------------------------
//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy, const std::string& raw_query, int document_id) const {
//    if(!document_ids_.count(document_id)){
//        throw out_of_range("id fail");
//    }
//    Query query = ParseQuery(raw_query);
//    bool is_was_minus = any_of(execution::seq,
//              query.minus_words.begin(), query.minus_words.end(),
//              [this, document_id](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return true;
//            }
//        }
//        return false;
//    } );

//    vector<string> matched_words;
//    if(is_was_minus){
//        return {matched_words, documents_.at(document_id).status};
//    }
//    matched_words.reserve(documents_words_freqs_.at(document_id).size());
//    for_each(execution::seq,
//              query.plus_words.begin(), query.plus_words.end(),
//             [this, document_id, &matched_words](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                matched_words.push_back(word);
//            }
//        }
//    });

//    return {matched_words, documents_.at(document_id).status};
//}
//-------------------------------------------------------------------------------------------------------------
//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy, const std::string& raw_query, int document_id) const {
//    return MatchDocument(raw_query, document_id);
//}
//-------------------------------------------------------------------------------------------------------------
const std::map<string, double> &SearchServer::GetWordFrequencies(int document_id) const
{
    if(!documents_words_freqs_.count(document_id)){ //документа с таким id нет
        static std::map<std::string, double> empty;
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
bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}
//-------------------------------------------------------------------------------------------------------------
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
SearchServer::Query SearchServer::ParseQuery(const std::string& text, bool is_del_copy) const {
    Query result = {};
    vector <string> vec_uniq(SplitIntoWords(text));
    if(is_del_copy){
        DelCopyElemVec(vec_uniq);
    }
    for (const string& word : vec_uniq) {
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
void SearchServer::DelCopyElemVec(std::vector<string> &vec) const
{
    sort(vec.begin(), vec.end());
    auto last = unique(vec.begin(), vec.end());
    vec.erase(last, vec.end());
}
//-------------------------------------------------------------------------------------------------------------
double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
//-------------------------------------------------------------------------------------------------------------
//#include <cmath>
//#include <numeric>

//#include "search_server.h"
//#include "string_processing.h"

//using namespace std;
////-------------------------------------------------------------------------------------------------------------
//SearchServer::SearchServer(const string& stop_words_text)
//    : SearchServer(SplitIntoWords(stop_words_text))
//{
//}
////-------------------------------------------------------------------------------------------------------------
//void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status,
//                               const std::vector<int>& ratings) {
//    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
//        throw invalid_argument("(document_id < 0) || (documents_.count(document_id) > 0)"s);
//    }
//    vector<string> words = SplitIntoWordsNoStop(document);

//    const double inv_word_count = 1.0 / words.size();
//    for (const string& word : words) {
//        word_to_document_freqs_[word][document_id] += inv_word_count;
//        documents_words_freqs_[document_id][word] += inv_word_count;
//    }
//    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
//    document_ids_.insert(document_id);
//}
////-------------------------------------------------------------------------------------------------------------
//std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
//    return FindTopDocuments(
//        raw_query,
//        [status](int, DocumentStatus document_status, int) {
//            return document_status == status;});
//}
////-------------------------------------------------------------------------------------------------------------
//std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
//    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
//}
////-------------------------------------------------------------------------------------------------------------
//int SearchServer::GetDocumentCount() const {
//    return static_cast<int>(documents_.size());
//}
////-------------------------------------------------------------------------------------------------------------
//std::set<int>::const_iterator SearchServer::begin() const {
//    return document_ids_.cbegin();
//}
////-------------------------------------------------------------------------------------------------------------
//std::set<int>::const_iterator SearchServer::end() const {
//    return document_ids_.cend();
//}
////-------------------------------------------------------------------------------------------------------------
//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
//    if(!document_ids_.count(document_id)){
//        throw out_of_range("id fail");
//    }
//    Query query = ParseQuery(raw_query);
//    vector<string> matched_words;
//    for (const string& word : query.plus_words) {
//        if (word_to_document_freqs_.count(word) == 0) {
//            continue;
//        }
//        if (word_to_document_freqs_.at(word).count(document_id)) {
//            matched_words.push_back(word);
//        }
//    }
//    for (const string& word : query.minus_words) {
//        if (word_to_document_freqs_.count(word) == 0) {
//            continue;
//        }
//        if (word_to_document_freqs_.at(word).count(document_id)) {
//            matched_words.clear();
//            break;
//        }
//    }

//    return {matched_words, documents_.at(document_id).status};
//}
////-------------------------------------------------------------------------------------------------------------
////std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, const std::string& raw_query, int document_id) const {
////    if(!document_ids_.count(document_id)){
////        throw out_of_range("id fail");
////    }
////    Query query = ParseQuery(raw_query, execution::par);
////    DelCopyElemVec(query.minus_words, execution::par);
////    bool is_was_minus = any_of(execution::par,
////              query.minus_words.begin(), query.minus_words.end(),
////              [this, document_id](const string& word){
////        if (word_to_document_freqs_.count(word) != 0) {
////            if (word_to_document_freqs_.at(word).count(document_id)) {
////                return true;
////            }
////        }
////        return false;
////    } );

////    vector<string> matched_words;
////    if(is_was_minus){
////        return {matched_words, documents_.at(document_id).status};
////    }
////    //matched_words.reserve(documents_words_freqs_.at(document_id).size());
////    DelCopyElemVec(query.plus_words, execution::par);
////    for_each(execution::par,
////              query.plus_words.begin(), query.plus_words.end(),
////             [this, document_id, &matched_words](const string& word){
////        if (word_to_document_freqs_.count(word) != 0) {
////            if (word_to_document_freqs_.at(word).count(document_id)) {
////                matched_words.push_back(word);
////            }
////        }
////    });

////    return {matched_words, documents_.at(document_id).status};
////}

//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy, const std::string& raw_query, int document_id) const {
//    if(!document_ids_.count(document_id)){
//        throw out_of_range("id fail");
//    }
//    Query query = ParseQuery(raw_query);
//    //DelCopyElemVec(query.minus_words);
//    bool is_was_minus = any_of(execution::seq,
//              query.minus_words.begin(), query.minus_words.end(),
//              [this, document_id](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return true;
//            }
//        }
//        return false;
//    } );

//    vector<string> matched_words;
//    if(is_was_minus){
//        return {matched_words, documents_.at(document_id).status};
//    }
//    matched_words.reserve(documents_words_freqs_.at(document_id).size());
//    //DelCopyElemVec(query.plus_words);
//    for_each(execution::seq,
//              query.plus_words.begin(), query.plus_words.end(),
//             [this, document_id, &matched_words](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                matched_words.push_back(word);
//            }
//        }
//    });

//    return {matched_words, documents_.at(document_id).status};
//}

//std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, const std::string& raw_query, int document_id) const {
//    if(!document_ids_.count(document_id)){
//        throw out_of_range("id fail");
//    }
//    Query query = ParseQuery(execution::seq, raw_query);
//    DelCopyElemVec(execution::seq, query.minus_words);
//    bool is_was_minus = any_of(execution::seq,
//              query.minus_words.begin(), query.minus_words.end(),
//              [this, document_id](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                return true;
//            }
//        }
//        return false;
//    } );

//    vector<string> matched_words;
//    if(is_was_minus){
//        return {matched_words, documents_.at(document_id).status};
//    }
//    matched_words.reserve(documents_words_freqs_.at(document_id).size());
//    DelCopyElemVec(execution::seq, query.plus_words);
//    for_each(execution::seq,
//              query.plus_words.begin(), query.plus_words.end(),
//             [this, document_id, &matched_words](const string& word){
//        if (word_to_document_freqs_.count(word) != 0) {
//            if (word_to_document_freqs_.at(word).count(document_id)) {
//                matched_words.push_back(word);
//            }
//        }
//    });

//    return {matched_words, documents_.at(document_id).status};
//}

////std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, const std::string& raw_query, int document_id) const {
////    if(!document_ids_.count(document_id)){
////        throw out_of_range("id fail");
////    }
////    Query query = ParseQuery(raw_query);
////    DelCopyElemVec(query.minus_words);
////    bool is_was_minus = any_of(execution::par,
////              query.minus_words.begin(), query.minus_words.end(),
////              [this, document_id](const string& word){
////        if (word_to_document_freqs_.count(word) != 0) {
////            if (word_to_document_freqs_.at(word).count(document_id)) {
////                return true;
////            }
////        }
////        return false;
////    } );

////    vector<string> matched_words;
////    if(is_was_minus){
////        return {matched_words, documents_.at(document_id).status};
////    }
////    matched_words.reserve(documents_words_freqs_.at(document_id).size());
////    DelCopyElemVec(query.plus_words);
////    for_each(execution::par,
////              query.plus_words.begin(), query.plus_words.end(),
////             [this, document_id, &matched_words](const string& word){
////        if (word_to_document_freqs_.count(word) != 0) {
////            if (word_to_document_freqs_.at(word).count(document_id)) {
////                matched_words.push_back(word);
////            }
////        }
////    });

////    return {matched_words, documents_.at(document_id).status};
////}

////std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy, const std::string& raw_query, int document_id) const {
////    return MatchDocument(raw_query, document_id);
////}
////    transform(execution::par,
////              query.plus_words.begin(), query.plus_words.end(),
////              matched_words.begin(),
////              [this, document_id](const string& word){
////        if (word_to_document_freqs_.count(word) != 0) {
////            if (word_to_document_freqs_.at(word).count(document_id)) {
////                return word;
////            }
////        }
////    });

////    for (const string& word : query.plus_words) {
////        if (word_to_document_freqs_.count(word) == 0) {
////            continue;
////        }
////        if (word_to_document_freqs_.at(word).count(document_id)) {
////            matched_words.push_back(word);
////        }
////    }
////    for (const string& word : query.minus_words) {
////        if (word_to_document_freqs_.count(word) == 0) {
////            continue;
////        }
////        if (word_to_document_freqs_.at(word).count(document_id)) {
////            matched_words.clear();
////            break;
////        }
////    }
////-------------------------------------------------------------------------------------------------------------
////std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy, const std::string& raw_query, int document_id) const {
////    if(!document_ids_.count(document_id)){
////        throw out_of_range("id fail");
////    }
////    Query query = ParseQuery(raw_query);
////    bool is_was_minus = any_of(execution::seq,
////              query.minus_words.begin(), query.minus_words.end(),
////              [this, document_id](const string& word){
////        if (word_to_document_freqs_.count(word) != 0) {
////            if (word_to_document_freqs_.at(word).count(document_id)) {
////                return true;
////            }
////        }
////        return false;
////    } );

////    vector<string> matched_words;
////    if(is_was_minus){
////        return {matched_words, documents_.at(document_id).status};
////    }
////    matched_words.reserve(documents_words_freqs_.at(document_id).size());
////    for_each(execution::seq,
////              query.plus_words.begin(), query.plus_words.end(),
////             [this, document_id, &matched_words](const string& word){
////        if (word_to_document_freqs_.count(word) != 0) {
////            if (word_to_document_freqs_.at(word).count(document_id)) {
////                matched_words.push_back(word);
////            }
////        }
////    });

////    return {matched_words, documents_.at(document_id).status};
////}
////-------------------------------------------------------------------------------------------------------------
////std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy, const std::string& raw_query, int document_id) const {
////    return MatchDocument(raw_query, document_id);
////}
////-------------------------------------------------------------------------------------------------------------
//const std::map<string, double> &SearchServer::GetWordFrequencies(int document_id) const
//{
//    if(!documents_words_freqs_.count(document_id)){ //документа с таким id нет
//        static std::map<std::string, double> empty;
//        return empty;
//    }
//    return documents_words_freqs_.at(document_id);
//}
////-------------------------------------------------------------------------------------------------------------
//void SearchServer::RemoveDocument(int document_id)
//{
//    if(!document_ids_.count(document_id)){
//        return;
//    }
//    for(const auto& [word, _] : documents_words_freqs_.at(document_id)){
//        word_to_document_freqs_.at(word).erase(document_id);
//    }
//    documents_words_freqs_.erase(document_id);
//    documents_.erase(document_id);
//    document_ids_.erase(document_id);
//}
////-------------------------------------------------------------------------------------------------------------
//bool SearchServer::IsStopWord(const std::string& word) const {
//    return stop_words_.count(word) > 0;
//}
////-------------------------------------------------------------------------------------------------------------
//bool SearchServer::IsValidWord(const std::string& word) {
//    return none_of(word.begin(), word.end(), [](char c) {
//        return c >= '\0' && c < ' ';
//    });
//}
////-------------------------------------------------------------------------------------------------------------
//std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
//    vector<string> words;
//    for (const string& word : SplitIntoWords(text)) {
//        if (!IsValidWord(word)) {
//            throw invalid_argument("!IsValidWord(word)"s);
//        }
//        if (!IsStopWord(word)) {
//            words.push_back(word);
//        }
//    }
//    return words;
//}
////-------------------------------------------------------------------------------------------------------------
//int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
//    if (ratings.empty()) {
//        return 0;
//    }
//    int rating_sum = accumulate(ratings.begin(),ratings.end(), 0);
//    return rating_sum / static_cast<int>(ratings.size());
//}
////-------------------------------------------------------------------------------------------------------------
//SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
//    if (text.empty()) {
//        throw invalid_argument("text.empty()");
//    }
//    bool is_minus = false;
//    if (text[0] == '-') {
//        is_minus = true;
//        text = text.substr(1);
//    }
//    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
//        throw invalid_argument("text.empty() || text[0] == '-' || !IsValidWord(text)");
//    }
//    return QueryWord{text, is_minus, IsStopWord(text)};
//}
////-------------------------------------------------------------------------------------------------------------
//SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
//    Query result = {};
//    vector <string> vec_uniq(SplitIntoWords(text));
//    DelCopyElemVec(vec_uniq);
//    for (const string& word : vec_uniq) {
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
////-------------------------------------------------------------------------------------------------------------
//void SearchServer::DelCopyElemVec(std::vector<string> &vec) const
//{
//    sort(vec.begin(), vec.end());
//    auto last = unique(vec.begin(), vec.end());
//    vec.erase(last, vec.end());
//}
////-------------------------------------------------------------------------------------------------------------
//double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
//    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
//}
////-------------------------------------------------------------------------------------------------------------
