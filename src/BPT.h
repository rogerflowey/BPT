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
      std::pair<PageRef<LeafNode>,index_type> cur_pos;
      sjtu::vector<std::pair<PageRef<InnerNode>,index_type>> parents;
      
    };

    enum class OperationType { FIND, INSERT, DELETE };
    FindResult find_pos(const Key& key,OperationType type) {
      hash_t h = hash(key);
      sjtu::vector<std::pair<PageRef<InnerNode>,index_type>> parents;
      PageRef<InnerNode> cur = root_.get_ref();
      for(int i=0;i<layer;++i) {
        index_type index = cur->search(h);
        page_id_t next = cur->at(index).second;
        if(!(type==OperationType::FIND)) {
          if((type==OperationType::INSERT&&cur->is_upper_safe())||(type==OperationType::DELETE&&cur->is_lower_safe())) {
            parents.clear();
            parents.emplace_back(std::move(cur),index);
          }
        }
        cur = std::move(PagePtr<InnerNode>{next,&manager_}.get_ref());
      }
      index_type index = cur->search(h);
      page_id_t next = cur->at(index).second;
      auto temp = PagePtr<LeafNode>{next,&manager_}.get_ref();

      if(!(type==OperationType::FIND)) {
        if((type==OperationType::INSERT&&temp->is_upper_safe())||(type==OperationType::DELETE&&temp->is_lower_safe())) {
          parents.clear();
        }
      }

      index_type id = temp->search(h);
      return {{std::move(temp),id},parents};
    }

  public:
    explicit BPT(std::string& file_name):manager_(file_name),root_(0,&manager_) {
      auto cfg = PagePtr<BPT_config>{0,&manager_}.get_ref();
      root_=PagePtr<InnerNode>{cfg->root_id,&manager_};
    }
    sjtu::vector<Value> find(const Key& key) {
      auto result = find_pos(key,OperationType::FIND);
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
      auto [pos,parents] = find_pos(key,OperationType::INSERT);
      pos.first->insert_at(pos.second,{hash(key),value});
      if(parents.empty()) {
        return;
      }
      //split on the route
      auto page_ref = pos.first->split(allocate<LeafNode>(&manager_));
      auto page_id = page_ref->self_id_;
      auto first_key = page_ref->data_[0].first;
      for(auto& [parent_node,index]:parents) {
        parent_node->insert_at(index,{first_key,page_id});
        if(parent_node->current_size_>=LeafNode::SPLIT_T) {
          auto inner_ref = parent_node->split(allocate<InnerNode>(&manager_));
          page_id = inner_ref->self_id_;
          first_key = inner_ref->data_[0].first;
        } else {//最上面的不应该需要Split,除非是根节点
           return;
        }
      }
      auto split_node = root_.get_ref()->split(allocate<InnerNode>(&manager_));
      auto new_ptr = allocate<InnerNode>(&manager_);
      auto new_root = new_ptr.make_ref(std::make_unique<InnerNode>(InnerNode{new_ptr.page_id(),2,{{1,root_.page_id()},{split_node->data_[0],split_node->self_id_}}}));

    }


  };

}
