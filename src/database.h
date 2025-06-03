#pragma once
#include <functional>
#include <string>
#include <functional>
#include "BPT.h"

template<typename Key,typename Value>
class SingleMap{
public:
  using BPlusTree = RFlowey::BPT<Key,Value>;


  BPlusTree bpt;
  SingleMap(const std::string& path):bpt(path) {
    bpt.insert({},{});
  }
  void insert(const Key& key,const Value& value ) {
    bpt.insert(key,value);
  }
  void erase(const Key& key) {
    bpt.erase(key);
  }
  sjtu::vector<RFlowey::pair<Key,Value>> find_range(const Key& start,const Key& end) {
    return bpt.range_find(start,end);
  }
};

template<typename Key,typename Value,typename Hash = std::hash<Key>>
class HashedSingleMap{
public:
  using hash_t = long long;
  using BPlusTree = RFlowey::BPT<hash_t,RFlowey::pair<Key,Value>>;


  BPlusTree bpt;
  Hash hash_func{};
  HashedSingleMap(const std::string& path):bpt(path) {
    bpt.insert({},{{},{}});
  }
  ~HashedSingleMap() = default;
  void insert(const Key& key,const Value& value ) {
    bpt.insert(hash_func(key),value);
  }
  void erase(const Key& key) {
    bpt.erase(hash_func(key));
  }
  std::optional<Value> find(const Key& key) {
    return bpt.find(hash_func(key));
  }
};

template<typename Key,typename Value>
class OrderedMultiMap{
public:
  using BPlusTree = RFlowey::BPT<RFlowey::pair<Key,Value>,RFlowey::Nothing>;


  BPlusTree bpt;
  OrderedMultiMap(const std::string& path):bpt(path) {
    bpt.insert({{},{}},{});
  }
  void insert(const Key& key,const Value& value ) {
    bpt.insert({key,value},{});
  }
  void erase(const Key& key,const Value& value) {
    bpt.erase({key,value});
  }
  sjtu::vector<RFlowey::pair<Key,Value>> find_range(const Key& start,const Key& end) {
    auto result = bpt.range_find(start,end);
    sjtu::vector<RFlowey::pair<Key,Value>> return_val;
    for(auto& i:result) {
      return_val.push_back(i.first);
    }
    return return_val;
  }
};


template<typename Key,typename Value, typename Hash = std::hash<Key>>
class OrderedHashMap{
public:
  using hash_t = long long;
  using BPlusTree = RFlowey::BPT<RFlowey::pair<hash_t,Value>,RFlowey::Nothing>;


  BPlusTree bpt;
  Hash hash_func{};
  explicit OrderedHashMap(const std::string& path):bpt(path) {
    bpt.insert({{},{}},{});
  }
  ~OrderedHashMap() = default;
  void insert(const Key& key,const Value& value ) {
    bpt.insert({hash_func(key),value},{});
  }
  void erase(const Key& key,const Value& value) {
    bpt.erase({hash_func(key),value});
  }
  sjtu::vector<Value> find(const Key& key) {
    auto result = bpt.range_find({hash_func(key),{}},{hash_func(key)+1,{}});
    sjtu::vector<Value> return_val;
    for(auto& i:result) {
      return_val.push_back(i.first.second);
    }
    return return_val;
  }
};
