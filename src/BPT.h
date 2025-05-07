#pragma once
#include <algorithm>
#include <limits>


#include "src/disk/IO_manager.h"
#include "src/disk/IO_utils.h"
#include "thirdparty/vector/vector.hpp"
#include "src/utils/utils.h"
#include "Node.h"

namespace RFlowey {
  template<typename Key,typename Value,typename KeyHash = std::hash<Key>,typename ValueHash = std::hash<Value>>
  //using Key = string<64>;
  //using Value = int;

  class BPT {
    using key_type = pair<hash_t,hash_t>;
    using value_type = pair<Key, Value>;
    using InnerNode = BPTNode<key_type, page_id_t, Inner>;
    using LeafNode = BPTNode<key_type, value_type, Leaf>;

    KeyHash key_hash{};
    ValueHash value_hash{};


    MemoryManager manager_;
    PagePtr<InnerNode> root_;
    int layer = 0;

    struct BPT_config {
      bool is_set;
      int layer;
      page_id_t root_id;
    };

    struct FindResult {
      pair<PageRef<LeafNode>, index_type> cur_pos;
      sjtu::vector<pair<PageRef<InnerNode>, index_type> > parents;
    };

    enum class OperationType { FIND, INSERT, DELETE };

    FindResult find_pos(const key_type &key, OperationType type) {
#ifdef BPT_TEST
      assert(root_.page_id() != INVALID_PAGE_ID && root_.page_id() != 0 && "find_pos called with invalid root");
      assert(layer >= 0 && "find_pos called with invalid layer");
#endif
      sjtu::vector<pair<PageRef<InnerNode>, index_type> > parents;
      PageRef<InnerNode> cur = root_.get_ref();

#ifdef BPT_TEST
      assert(cur->self_id_ == root_.page_id());
      assert(cur->current_size_ > 0 && "Root node should not be empty unless tree is just one leaf (layer 0 special case)");
#endif
      for (int i = 0; i < layer; ++i) {
#ifdef BPT_TEST
        assert(cur->current_size_ > 0 && "Inner node on path is empty");
#endif
        index_type index = cur->search(key);
#ifdef BPT_TEST
        assert(index != static_cast<index_type>(INVALID_PAGE_ID) && \
               "BPTNode::search returned INVALID_PAGE_ID in an InnerNode");
        assert(index < cur->current_size_ && \
               "Search index out of bounds in inner node after valid return.");
#endif
        page_id_t next = cur->at(index).second;
        if ((type == OperationType::INSERT && cur->is_upper_safe()) ||
          (type == OperationType::DELETE && cur->is_lower_safe())) {
          parents.clear();
          parents.emplace_back(std::move(cur), index);
        }
        cur = std::move(PagePtr<InnerNode>{next, &manager_}.get_ref());
      }
      index_type index = cur->search(key);
      page_id_t next = cur->at(index).second;
      auto temp = PagePtr<LeafNode>{next, &manager_}.get_ref();
      if ((type == OperationType::INSERT && temp->is_upper_safe()) ||
        (type == OperationType::DELETE && temp->is_lower_safe())) {
        parents.clear();
      }

      index_type id = temp->search(key);
#ifdef BPT_TEST
      assert(id != static_cast<index_type>(INVALID_PAGE_ID));
#endif
      return {{std::move(temp), id}, std::move(parents)};
    }

  public:
    explicit BPT(const std::string &file_name): manager_(file_name),root_(INVALID_PAGE_ID,nullptr) {//root not right now
    if(manager_.is_new) {
#ifdef BPT_TEST
      std::cerr << "Initializing new BPT database..." << std::endl;
      assert(root_.page_id() == INVALID_PAGE_ID && "Root should be invalid before new DB init");
#endif
      PagePtr<InnerNode> new_root_ptr = allocate<InnerNode>(&manager_);
      PagePtr<LeafNode> first_leaf_ptr = allocate<LeafNode>(&manager_);

      this->layer = 0;
      this->root_ = new_root_ptr;
#ifdef BPT_TEST
      assert(this->root_.page_id() != INVALID_PAGE_ID && this->root_.page_id() != 0);
      assert(first_leaf_ptr.page_id() != INVALID_PAGE_ID && first_leaf_ptr.page_id() != 0);
      assert(this->root_.page_id() != first_leaf_ptr.page_id());
#endif

      typename LeafNode::value_type initial_leaf_data[1] = {{{0,0},{Key{},Value{}}}};
      auto temp_leaf_ref = first_leaf_ptr.make_ref(LeafNode{first_leaf_ptr.page_id(),1,initial_leaf_data});
#ifdef BPT_TEST
      assert(temp_leaf_ref->current_size_ == 1);
      assert(temp_leaf_ref->self_id_ == first_leaf_ptr.page_id());
#endif

      InnerNode::value_type initial_root_data[1] = { {{0,0}, first_leaf_ptr.page_id()} };
      auto temp_root_ref = new_root_ptr.make_ref(InnerNode{new_root_ptr.page_id(), 1, initial_root_data});
#ifdef BPT_TEST
      assert(temp_root_ref->current_size_ == 1);
      assert(temp_root_ref->at(0).second == first_leaf_ptr.page_id());
      assert(temp_root_ref->self_id_ == new_root_ptr.page_id());
#endif

    } else {

      PagePtr<BPT_config> cfg_ptr{1, &manager_};
      auto cfg_ref = cfg_ptr.get_ref();
#ifdef BPT_TEST
      assert(cfg_ref->layer >= 0 && "Loaded layer should be non-negative");
      assert(cfg_ref->root_id != INVALID_PAGE_ID && "Loaded root_id should be valid");
      assert(cfg_ref->root_id != 0 && "Loaded root_id should not be config page 0");
      std::cerr << "Loading existing BPT database..." << std::endl;
#endif
      this->root_ = PagePtr<InnerNode>{cfg_ref->root_id, &manager_};
      this->layer = cfg_ref->layer;
#ifdef BPT_TEST
      assert(this->layer >= 0);
      assert(this->root_.page_id() != INVALID_PAGE_ID && this->root_.page_id() != 0);

      auto root_check_ref = this->root_.get_ref();
      assert(root_check_ref->self_id_ == this->root_.page_id());
#endif
    }


  }
    ~BPT() {
#ifdef BPT_TEST
      std::cerr << "BPT Destructor: Saving config. Layer=" << layer
                << ", RootID=" << (root_.page_id() == INVALID_PAGE_ID ? -1 : root_.page_id()) << std::endl;
      assert(layer >= -1); // layer could be -1 if tree was never properly initialized
      if (layer >= 0) { // If tree was meant to be valid
        assert(root_.page_id() != INVALID_PAGE_ID && root_.page_id() != 0 && "Attempting to save invalid root_id");
      }
#endif
      if (root_.page_id() != INVALID_PAGE_ID && root_.page_id() != 0) { // Only save if root seems valid
        BPT_config cfg_to_save = {true, layer, root_.page_id()};
        PagePtr<BPT_config>{0, &manager_}.make_ref(std::move(cfg_to_save)); // Use std::move for unique_ptr if make_ref takes it
      } else {
#ifdef BPT_TEST
        std::cerr << "BPT Destructor: Root is invalid, not saving config to page 0." << std::endl;
#endif
      }
    }

    /**
     * @return a vector of the values correspond to the key;(sorted by the hash of value)
     */
    sjtu::vector<Value> find(const Key &key) {
      key_type inner_key = {key_hash(key),0};
      key_type upper = {key_hash(key)+1,0};
      auto result = find_pos(inner_key, OperationType::FIND);
      auto leaf = std::move(result.cur_pos.first);
      auto index = result.cur_pos.second;
      sjtu::vector<Value> temp;
      while (true) {
        if (leaf->at(index).first >= upper) {
          break;
        }
        auto value = leaf->at(index).second;
        if (value.first == key) {
          temp.push_back(value.second);
        }
        ++index;
        if (index == LeafNode::SIZEMAX && leaf->next_node_id_ != INVALID_PAGE_ID) {
          index = 0;
          leaf = PagePtr<LeafNode>{leaf->next_node_id_, &manager_}.get_ref();
        }
      }
      return temp;
    }

    void insert(const Key &key, const Value &value) {
      key_type inner_key = {key_hash(key), std::hash<Value>{}(value)};
      auto [pos,parents] = find_pos(inner_key, OperationType::INSERT);
      pos.first->insert_at(pos.second, {{key_hash(key),value_hash(value)}, {key, value}});
      if (parents.empty()) {
        return;
      }
      //split on the route
      auto page_ref = pos.first->split(allocate<LeafNode>(&manager_));
      auto page_id = page_ref->self_id_;
      auto first_key = page_ref->get_first();
      while (!parents.empty()) {
        auto parent_node = std::move(parents.back().first);
        auto index = std::move(parents.back().second);
        parents.pop_back();
        parent_node->insert_at(index, {first_key, page_id});
        if (parent_node->current_size_>=InnerNode::SPLIT_T) {
          auto inner_ref = parent_node->split(allocate<InnerNode>(&manager_));
          page_id = inner_ref->self_id_;
          first_key = inner_ref->get_first();
        } else {
#ifdef BPT_TEST
          assert(root_.page_id()!=parent_node->get_self());
#endif
          //最上面的不应该需要Split,除非是根节点
          return;
        }
      }
#ifdef BPT_TEST
      assert(root_.get_ref()->current_size_>=InnerNode::SPLIT_T);
#endif
      auto split_node = root_.get_ref()->split(allocate<InnerNode>(&manager_));
      auto new_ptr = allocate<InnerNode>(&manager_);
      InnerNode::value_type temp_data[2] = {{{0,0}, root_.page_id()}, {split_node->get_first(), split_node->get_self()}};
      auto new_root = new_ptr.make_ref(InnerNode{new_ptr.page_id(), 2, temp_data});
      root_ = new_ptr;
      ++layer;
    }
  };
}