#include <algorithm>
#include "string_processing.h"
using namespace std;
//-------------------------------------------------------------------------------------------------------------
std::vector<std::string_view> SplitIntoWords(std::string_view str) {
    vector<string_view> result;
    str.remove_prefix(min(str.find_first_not_of(" "), str.size()));
    const int64_t pos_end = str.npos;

    while (!str.empty()) {
        int64_t space = str.find(' ');
        result.push_back(space == pos_end ? str.substr(0) : str.substr(0, space));
        str.remove_prefix(result.back().size());
        str.remove_prefix(min(str.find_first_not_of(" "), str.size()));
    }

    return result;
}
//-------------------------------------------------------------------------------------------------------------
