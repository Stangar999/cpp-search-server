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


