#pragma once
#include <algorithm>

#include "Node.h"
#include "src/disk/IO_manager.h"
#include "src/disk/IO_utils.h"
#include "thirdparty/vector/vector.hpp"
#include "src/utils/utils.h"


namespace RFlowey {

  //template<typename Key,typename Value>
  using Key = std::string;
  using Value = int;
  class BPT {

    using InnerNode = BPTNode<hash_t,page_id_t,Inner>;
    using LeafNode = BPTNode<hash_t,Value,Leaf>;

    SimpleDiskManager manager_;
    PagePtr<InnerNode> root_;
    int layer=0;

    struct BPT_config {
      bool is_set;
      int layer;
      page_id_t root_id;

    };

    struct FindResult {
      pair<PageRef<LeafNode>,index_type> cur_pos;
      sjtu::vector<pair<PageRef<InnerNode>,index_type>> parents;
      
    };

    FindResult find_pos(const Key& key) {
      hash_t h = hash(key);
      sjtu::vector<pair<PageRef<InnerNode>,index_type>> parents;
      PageRef<InnerNode> cur = root_.get_ref();
      for(int i=0;i<layer;++i) {
        index_type index = cur->search(h);
        page_id_t next = cur->at(index).second;
        if(cur->is_safe()) {
          parents.clear();
          parents.emplace_back(std::move(cur),index);
        }
        cur = std::move(PagePtr<InnerNode>{next,&manager_}.get_ref());
      }
      index_type index = cur->search(h);
      page_id_t next = cur->at(index).second;
      auto temp = PagePtr<LeafNode>{next,&manager_}.get_ref();
      if(temp->is_safe()) {
        parents.clear();
      }
      index_type id = temp->search(h);
      return {{std::move(temp),id},parents};
    }

  public:
    BPT(std::string& file_name):manager_(file_name),root_(0,&manager_) {
      auto cfg = PagePtr<BPT_config>{0,&manager_}.get_ref();
      root_=PagePtr<InnerNode>{cfg->root_id,&manager_};
    }
    sjtu::vector<Value> find(const Key& key) {
      auto result = find_pos(key);
      auto leaf = std::move(result.cur_pos.first);
      auto index = result.cur_pos.second;
      sjtu::vector<Value> temp;
      while (true) {
        if(leaf->at(index).first!=hash(key)) {
          break;
        }
        temp.push_back(leaf->at(index).second);
        ++index;
        if (index==LeafNode::SIZEMAX) {
          index=0;
          leaf = PagePtr<LeafNode>{leaf->next_node_id_,&manager_}.get_ref();
        }
      }
      std::sort(temp.begin(),temp.end());
      return temp;
    }

    void insert(const Key& key,const Value& value) {
      auto [pos,parents] = find_pos(key);
      pos.first->insert_at(pos.second,{hash(key),value});
      if(parents.empty()) {
        return;
      }
      //split on the route
      auto page_id = manager_.NewPage();
      auto page_ref = PagePtr<LeafNode>{page_id,&manager_}.make_ref(pos.first->split(page_id));
      auto first_key = page_ref->data_[0].first;
      for(auto& [parent_node,index]:parents) {
        parent_node->insert_at(index,{first_key,page_id});
        if(parent_node->current_size_>=LeafNode::SPLIT_T) {
          page_id = manager_.NewPage();
          page_ref = PagePtr<LeafNode>{page_id,&manager_}.make_ref(parent_node->split(page_id));
          first_key = page_ref->data_[0].first;
        } else {
           return;
        }
      }
      //split root
      page_id = manager_.NewPage();
      page_ref = PagePtr<LeafNode>{page_id,&manager_}.make_ref(root_.get_ref()->split(page_id));
      first_key = page_ref->data_[0].first;

      auto new_root_id = manager_.NewPage();
      auto new_root = std::make_unique<LeafNode>();

    }


  };

}
