#pragma once
#include <map>
#include <vector>
#include <mutex>

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        Value& ref_to_value;
        //std::lock_guard<std::mutex> guard;
        std::mutex &m_;
        ~Access(){
            m_.unlock();
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : bucket_count_(bucket_count)
        , vec_maps_(bucket_count)
        , vec_mut_(bucket_count)
    {};

    Access operator[](const Key& key){
        Key key_to_key = key % bucket_count_;
        vec_mut_[key_to_key].lock();
        //std::cout << std::this_thread::get_id() << std::endl;
        return {vec_maps_[key_to_key][key], vec_mut_[key_to_key]};
    };

    std::map<Key, Value> BuildOrdinaryMap(){
        std::map<Key, Value> result;
        for(auto& map : vec_maps_){
            for(const auto& [key, _] : map){
                result[key] = (*this)[key].ref_to_value;
            }
        }
        return result;
    };

private:
    size_t bucket_count_;
    std::vector<std::map<Key, Value>> vec_maps_;
    std::vector<std::mutex> vec_mut_;
};
