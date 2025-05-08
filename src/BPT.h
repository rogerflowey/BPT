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


    SimpleDiskManager manager_;
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
      page_id_t next = root_.page_id();
      index_type index;

      for (int i = 0; i <= layer; ++i) {
        PageRef<InnerNode> cur = std::move(PagePtr<InnerNode>{next, &manager_}.get_ref());
#ifdef BPT_TEST
        assert(cur->current_size_ > 0 && "Inner node on path is empty");
#endif
        index = cur->search(key);
#ifdef BPT_TEST
        assert(index != static_cast<index_type>(INVALID_PAGE_ID) && \
               "BPTNode::search returned INVALID_PAGE_ID in an InnerNode");
        assert(index < cur->current_size_ && \
               "Search index out of bounds in inner node after valid return.");
#endif
        next = cur->at(index).second;
        if(type!=OperationType::FIND) {
          if ((type == OperationType::INSERT && cur->is_upper_safe()) ||
            (type == OperationType::DELETE && cur->is_lower_safe())) {
            parents.clear();
            }
          parents.emplace_back(std::move(cur), index);
        }
      }
      auto temp = PagePtr<LeafNode>{next, &manager_}.get_ref();
      if ((type == OperationType::INSERT && temp->is_upper_safe()) ||
        (type == OperationType::DELETE && temp->is_lower_safe())) {
        parents.clear();
      }

      index_type id = temp->search(key);

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
        PagePtr<BPT_config>{1, &manager_}.make_ref(std::move(cfg_to_save)); // Use std::move for unique_ptr if make_ref takes it
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
      if(index==INVALID_PAGE_ID) {
        index=0;
      }
      sjtu::vector<Value> temp;
      while (index<leaf->current_size_) {
        if (leaf->at(index).first >= upper) {
          break;
        }
        auto value = leaf->at(index).second;
        if (value.first == key) {
          temp.push_back(value.second);
        }
        ++index;
        if (index == leaf->current_size_ && leaf->next_node_id_ != INVALID_PAGE_ID) {
          index = 0;
          leaf = PagePtr<LeafNode>{leaf->next_node_id_, &manager_}.get_ref();
        }
      }
      return temp;
    }

    void insert(const Key &key, const Value &value) {
      key_type inner_key = {key_hash(key), value_hash(value)};
      auto [pos,parents] = find_pos(inner_key, OperationType::INSERT);
      pos.first->insert_at(pos.second, {{key_hash(key),value_hash(value)}, {key, value}});
#ifdef BPT_TEST
      pos.first->current_size_<=LeafNode::SPLIT_T;
#endif
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
          assert(parents.empty());
#endif
          //最上面的不应该需要Split,除非是根节点
          return;
        }
      }
      //root分裂了，增加新root
      auto new_ptr = allocate<InnerNode>(&manager_);
      InnerNode::value_type temp_data[2] = {{{0,0}, root_.page_id()}, {first_key,page_id}};
      auto new_root = new_ptr.make_ref(InnerNode{new_ptr.page_id(), 2, temp_data});
      root_ = new_ptr;
      ++layer;
    }


    bool erase(const Key& key, const Value& value) {
      key_type inner_key = {key_hash(key), value_hash(value)};
      auto [pos,parents] = find_pos(inner_key, OperationType::DELETE);
      if(pos.second>=pos.first->current_size_||pos.first->at(pos.second).first!=inner_key) {
        return false;
      }
      pos.first->erase(pos.second);
      if(parents.empty()) {
        return true;
      }
      if(!pos.first->merge(&manager_)) {
        return true;
      }
      while (!parents.empty()) {
        auto parent_node = std::move(parents.back().first);
        auto index = std::move(parents.back().second);
        parents.pop_back();
        parent_node->erase(index);
        if (parent_node->current_size_<=InnerNode::MERGE_T&&parent_node->prev_node_id_!=INVALID_PAGE_ID) {
          if(!parent_node->merge(&manager_)) {
            break;
          }
        }else {
          break;
        }
      }
      auto root = root_.get_ref();
      if(root->current_size_==1&&layer>0) {
        root_ = PagePtr<InnerNode>{root->data_[0].second,&manager_};
        --layer;
        manager_.DeletePage(root->get_self());
      }
      return true;
    }

    public: // Add to public section of BPT

  void print_tree_structure() {
    std::cout << "\n====== B+Tree Structure ======" << std::endl;
    if (root_.page_id() == INVALID_PAGE_ID || root_.page_id() == 0) {
      std::cout << "Tree is empty or root is invalid." << std::endl;
      return;
    }
    std::cout << "Layer: " << layer << std::endl;
    std::cout << "Root Page ID: " << root_.page_id() << std::endl;
    print_node_recursive(root_.page_id(), 0, true); // true for is_inner_node_root
    std::cout << "==============================\n" << std::endl;
  }

private: // Add to private section of BPT

  // Recursive helper to print nodes
  void print_node_recursive(page_id_t page_id, int current_depth, bool is_inner_node) {
    if (page_id == INVALID_PAGE_ID) {
      std::cout << std::string(current_depth * 4, ' ') << "INVALID_PAGE_ID" << std::endl;
      return;
    }

    std::string indent = std::string(current_depth * 4, ' ');

    if (is_inner_node) {
      try {
        PageRef<InnerNode> node_ref = PagePtr<InnerNode>{page_id, &manager_}.get_ref();
        std::cout << indent << "InnerNode (ID: " << node_ref->self_id_
                  << ", Size: " << node_ref->current_size_
                  << ", Prev: " << node_ref->prev_node_id_
                  << ", Next: " << node_ref->next_node_id_ << "):" << std::endl;

        for (size_t i = 0; i < node_ref->current_size_; ++i) {
          auto entry = node_ref->at(i); // pair<key_type, page_id_t>
          std::cout << indent << "  [" << i << "] Key: (" << entry.first.first << "," << entry.first.second << ")"
                    << " -> Child PID: " << entry.second << std::endl;
          // Recursively print child, unless it's the last level before leaves
          if (current_depth < layer) { // Only recurse if this inner node is not pointing to leaves
             print_node_recursive(entry.second, current_depth + 1, true);
          } else { // This inner node points to leaves
             print_node_recursive(entry.second, current_depth + 1, false);
          }
        }
      } catch (const std::exception& e) {
          std::cerr << indent << "Error accessing InnerNode ID " << page_id << ": " << e.what() << std::endl;
      }
    } else { // It's a LeafNode
      try {
        PageRef<LeafNode> node_ref = PagePtr<LeafNode>{page_id, &manager_}.get_ref();
        std::cout << indent << "LeafNode (ID: " << node_ref->self_id_
                  << ", Size: " << node_ref->current_size_
                  << ", Prev: " << node_ref->prev_node_id_
                  << ", Next: " << node_ref->next_node_id_ << "):" << std::endl;

        for (size_t i = 0; i < node_ref->current_size_; ++i) {
          auto entry = node_ref->at(i); // pair<key_type, value_type (pair<Key,Value>)>
          // Assuming Key and Value are printable or have a .c_str() or similar
          // For simplicity, just printing the key_type for now.
          // You'll want to customize this to print your actual Key and Value if possible.
          std::cout << indent << "  [" << i << "] KeyType: (" << entry.first.first << "," << entry.first.second << ")"
                    << " -> Value: (KeyHash: " << key_hash(entry.second.first) << ", ValHash: " << value_hash(entry.second.second)
                    << ", OrigKey: " << entry.second.first.c_str() /* Adjust if Key not string */
                    << ", OrigVal: " << entry.second.second /* Adjust if Value not int */ << ")" << std::endl;
        }
      } catch (const std::exception& e) {
          std::cerr << indent << "Error accessing LeafNode ID " << page_id << ": " << e.what() << std::endl;
      }
    }
  }
  };
}