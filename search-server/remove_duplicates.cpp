#include "remove_duplicates.h"
#include <set>

using namespace std;

void RemoveDuplicates(SearchServer& search_server){
    vector<int> list_del;
    set<set<string_view>> words_multiset;
    for(int id : search_server){
        set<string_view> temp;
        for(const auto& [word, _] : search_server.GetWordFrequencies(id)){ // TODO как не получать warning unused variable _
            temp.insert(word);
        }
        const auto& [_, flag] = words_multiset.insert(temp);
        if(flag == false){ // если вставка не удалась запоминаем id
            list_del.push_back(id);
        }
    }
    for(int id : list_del){
        search_server.RemoveDocument(id);
        cout << "Found duplicate document id " << id << endl;
    }
}
