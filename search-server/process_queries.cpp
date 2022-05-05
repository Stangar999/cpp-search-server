#include <execution>
#include <algorithm>
#include <string>
#include <functional>
#include <numeric>
#include <list>

#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries){
    vector<vector<Document>> result(queries.size());
    transform(execution::par,
              queries.begin(), queries.end(),
              result.begin(),
              [&search_server](string querie) { return search_server.FindTopDocuments(querie); }
    );
    return result;
}

list<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries){
    list<Document> result;
        for(const vector<Document>& vec : ProcessQueries(search_server, queries) ){
            for(const Document& doc : vec){
                result.push_back(doc);
            }
        }
        return result;
}
//    reduce(execution::par,
//           tmp.begin(), tmp.end(),
//           result,
//           [&result](const vector<Document>& vec) {result.insert(result.cend(),
//                                                                  vec.begin(),
//                                                                  vec.end()); }
//    );
//    for(const vector<Document>& vec : ProcessQueries(search_server, queries) ){
//        for(const Document& doc : vec){
//            result.push_back(doc);
//        }
//    }
//    return result;
//}
//    list<int> am_size;
//    transform_inclusive_scan(execution::par,
//                             tmp.begin(), tmp.end(),
//                             am_size.begin(),
//                             plus<>{},
//                             [](const vector<Document>& vec) { return vec.size(); }
//    );
//   vector<Document> result;
//   result.reserve(am_size.back());
//   for(const vector<Document>& vec : tmp){
//       for(const Document& doc : vec){
//           result.push_back(doc);
//       }
//   }
//    cout << am_size.back();
//    auto func = [](const vector<Document>& vec) {
//        return vec.size();
//    };
//    vector<Document> result(am_size.back());
//    transform_inclusive_scan(execution::par,
//                             tmp.begin(), tmp.end(),
//                             result.begin(),
//                             plus<>{},
//                             func//[](const vector<Document>& vec) { return vec.size(); }
//    );

