#include <execution>
#include <algorithm>
#include <string>
#include <functional>

#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries){
    vector<vector<Document>> result(queries.size());
    transform(execution::par,
              queries.begin(), queries.end(),
              result.begin(),
              //SearchServer::FindTopDocuments
              [&search_server](string querie) { return search_server.FindTopDocuments(querie); }
    );
    return result;
}
