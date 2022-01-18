/*
A basic LRU hash implementation.
*/

#ifndef _LRU_INCLUDED_
#define _LRU_INCLUDED_

#include <unordered_map>
#include <list>
#include <cstddef>
#include <stdexcept>

namespace cache {

template<typename key_type, typename val_type>
class lru {
public:
    // key_value_pair_type is a type for the combined key value pair
    typedef typename std::pair<key_type, val_type> key_value_pair_type;
    typedef typename std::list<key_value_pair_type>::iterator list_iter_type;

    lru(size_t max_entries) :
        _max_entries(max_entries) {
    }
    
    void add(const key_type& key, const val_type& value) {
        // add places a key, value pair at the front of the cache
        // if the cache is full, then the last item is evicted from the cache
        auto item = _items_map.find(key);
        _items_list.push_front(key_value_pair_type(key, value));
        if (item != _items_map.end()) {
            _items_list.erase(item->second);
            _items_map.erase(item);
        }

        _items_map[key] = _items_list.begin();

        if (_items_map.size() > _max_entries) {
            auto last = _items_list.end();
            last--;
            _items_map.erase(last->first);
            _items_list.pop_back();
        }
    }

    const val_type& get(const key_type& key) {
        // get retrieves the value associated with a key in the cache
        auto item = _items_map.find(key);
        if (item == _items_map.end()) {
            throw std::range_error("No such item in the cache.");
        } else {
            _items_list.splice(_items_list.begin(), _items_list, item->second);
            return item->second->second;
        }
    }

    bool exists(const key_type& key) const {
        // exists determines if item is in the map, we use unordered_map.find()
        // because it should be close to O(1) on average.
        return _items_map.find(key) != _items_map.end();
    }

    size_t size() const {
        // get the number of items in the map
        return _items_map.size();
    }

private:
    // _items_list stores an ordered list of the items in the cache
    std::list<key_value_pair_type> _items_list;
    // _items_map is an unordered hash map of the items in the cache
    std::unordered_map<key_type, list_iter_type> _items_map;
    // _max_entries is the maximum size of the cache
    size_t _max_entries;
};

} // namespace cache

#endif // _LRU_INCLUDED_