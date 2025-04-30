#ifndef NODE_H
#define NODE_H

#include <optional>
#include <variant>

#include "BPT.h"
#include "disk/IO_manager.h"
#include "src/utils/utils.h"
#include "src/common.h"
#include "disk/serialize.h"


namespace RFlowey {
  class BPT;

  enum PAGETYPE:char {
    Leaf, Inner
  };

  template<typename Key, typename Value,PAGETYPE type>
  class BPTNode {
    using value_type = std::pair<Key,Value>;
    static constexpr int SIZEMAX = (PAGESIZE - 128) / sizeof(value_type) - 1;
    static constexpr int SPLIT_T = SPLIT_RATE*SIZEMAX;
    static constexpr int MERGE_T = MERGE_RATE*SIZEMAX;
    static_assert(SIZEMAX>=10);

    page_id_t self_id_;
    page_id_t prev_node_id_;
    page_id_t next_node_id_;
    size_t current_size_;

    value_type data_[SIZEMAX];

    friend BPT;

  public:
    BPTNode(page_id_t self_id,size_t current_size,value_type data[]):self_id_(self_id),current_size_(current_size) {
      std::memcpy(data_,data,current_size*sizeof(value_type));
    }
    /**
     * @brief binary search for the key
     * @return the last index < key
     */
    [[nodiscard]] index_type search(const Key &key) const {
      index_type l = 0, r = current_size_;
      while (l < r) {
        index_type mid = l + (r - l) / 2;
        if (data_[mid].first < key) {
          l = mid + 1;
        } else {
          r = mid;
        }
      }
      return l-1;
    }

    [[nodiscard]] value_type at(index_type pos) const {
      return data_[pos];
    }

    /**
     * @brief insert key,value after pos
     * @return need
     */
    bool insert_at(index_type pos,const value_type& value) {
      std::memmove(data_+pos+1,data_+pos,sizeof(value_type)*(current_size_-pos-1));
      data_[pos+1] = value;
      current_size_++;
    }

    /**
     * @brief erase the data at pos
     */
    bool erase(index_type pos) {
      std::memmove(data_+pos,data_+pos+1,sizeof(value_type)*(current_size_-pos));
      current_size_--;
    }

    [[nodiscard]] bool is_safe() const {
      return current_size_<SPLIT_T-1 && current_size_>MERGE_T+1;
    }
    [[nodiscard]] bool is_upper_safe() const {
      return current_size_<SPLIT_T-1;
    }
    [[nodiscard]] bool is_lower_safe() const {
      return current_size_>MERGE_T+1;
    }

    PageRef<BPTNode> split(const PagePtr<BPTNode>& ptr) {
      std::unique_ptr<BPTNode> temp = std::make_unique<BPTNode>(*this);

      temp->prev_node_id_ = self_id_;
      next_node_id_ = ptr.page_id();
      temp->self_id_ = ptr.page_id();

      int mid = current_size_/2;
      std::memmove(temp->data_,temp->data_+mid,current_size_-mid);
      temp->current_size_ = current_size_-mid;
      current_size_ = mid;

      return ptr.make_ref(std::move(temp));
    }

  };
}

#endif //NODE_H
