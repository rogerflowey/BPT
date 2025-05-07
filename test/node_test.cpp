#include <iostream>
#include <vector>
#include <string>
#include <cstring>   // For std::memcpy, std::memset, strnlen, strcmp, strncmp
#include <algorithm> // For std::min
#include <cassert>
#include <stdexcept>
#include <memory>    // For std::unique_ptr, std::make_unique, std::shared_ptr
#include <fstream>   // For std::fstream (though not directly used in main, SimpleDiskManager uses it)

#define BPT_TEST

#include "src/common.h"
#include "src/utils/utils.h"       // For RFlowey::string
#include "src/disk/IO_manager.h"     // For IOManager, MemoryManager, SimpleDiskManager
#include "src/disk/IO_utils.h"       // For Page, PagePtr, PageRef, allocate
#include "src/Node.h"         // For BPTNode
#include "src/disk/serialize.h"      // For your new Serialize/Deserialize

#include <iostream> // For std::cout, std::endl

// Forward declaration of BPTNode if this function is in a separate file
// or ensure BPTNode definition is visible before this function.
// namespace RFlowey {
// template<typename Key, typename Value, PAGETYPE type_param> class BPTNode;
// }

// Assuming RFlowey namespace and necessary types (page_id_t, index_type, PAGETYPE)
// and BPTNode structure are defined and visible.
template<typename NodeT> // NodeT would be something like RFlowey::BPTNode<int, int, RFlowey::Leaf>
void set_node_size_and_fill_sequential(NodeT& node, size_t target_size, int start_key = 1, int val_offset = 0) {
    // Ensure target_size does not exceed the node's actual capacity (SIZEMAX)
    // NodeT::SIZEMAX should be accessible.
    if (target_size > NodeT::SIZEMAX) {
        // Optionally print SIZEMAX for debugging
        // std::cerr << "Error: target_size (" << target_size
        //           << ") exceeds NodeT::SIZEMAX (" << NodeT::SIZEMAX << ")" << std::endl;
        throw std::overflow_error("Target size exceeds SIZEMAX in test helper set_node_size_and_fill_sequential");
    }

    // Fill the data array directly. This is for test setup and bypasses insert_at logic.
    // This assumes 'data_' member of NodeT is accessible (e.g., public, or this is a friend function/class).
    // If 'data_' is private, NodeT would need a method to allow this kind of bulk setup.
    for (size_t i = 0; i < target_size; ++i) {
        // Assuming NodeT::value_type is compatible with {int, int} initialization
        // and Key/Value types of NodeT are compatible with int.
        node.data_[i] = {start_key + static_cast<int>(i), start_key + static_cast<int>(i) + val_offset};
    }

    // Set the current size of the node.
    // This assumes 'current_size_' member of NodeT is accessible.
    node.current_size_ = target_size;
}
template<typename Key, typename Value, RFlowey::PAGETYPE type_param>
void print_bpt_node_details(const RFlowey::BPTNode<Key, Value, type_param>& node, std::ostream& os = std::cout) {
    os << "Node ID: " << node.self_id_
              << ", Prev ID: " << node.prev_node_id_
              << ", Next ID: " << node.next_node_id_
              << ", Size: " << node.current_size_ << "/" << RFlowey::BPTNode<Key, Value, type_param>::SIZEMAX << std::endl;
    os << "Data: [";
    for (RFlowey::index_type i = 0; i < node.current_size_; ++i) {
        // Assuming data_ is public or you have a way to access elements, like an at() method.
        // If data_ is public:
        os << "{" << node.data_[i].first << "," << node.data_[i].second << "}";
        // If BPTNode has an at(index_type) const method:
        // os << "{" << node.at(i).first << "," << node.at(i).second << "}";
        if (i < node.current_size_ - 1) {
            os << ", ";
        }
    }
    os << "]" << std::endl;
}

// Example of how you might call it from your test, using a PageRef:
//
// {
//     PageRef<TestNode> node_ref = some_page_ptr.get_ref();
//     std::cout << "  Node details (Page ID " << node_ref->get_self() << "):" << std::endl;
//     print_bpt_node_details(*node_ref, std::cout); // Dereference PageRef to get BPTNode object
// }
//
// Or if you have a direct BPTNode object (less likely with PagePtr/PageRef model for main data):
// TestNode my_node_object(123);
// // ... populate my_node_object ...
// print_bpt_node_details(my_node_object, std::cout);

// --- Main Test Function ---
int main() {
    using namespace RFlowey; // Assuming all your classes are in this namespace
    using TestNode = BPTNode<int, int, PAGETYPE::Leaf>;
    using ValueT = pair<int, int>;

    std::cout << "BPTNode Test Suite with IOManager and PageAble Serialize/Deserialize" << std::endl;
    // These SIZEMAX, SPLIT_T, MERGE_T values depend on sizeof(ValueT) and PAGESIZE.
    // For <int, int>, sizeof(ValueT) is likely 8.
    // PAGESIZE = 4096. SIZEMAX_CALC = (4096 - 128) / 8 - 1 = 3968 / 8 - 1 = 496 - 1 = 495.
    // SIZEMAX = 495. SPLIT_T = 3/4 * 495 = 371. MERGE_T = 1/4 * 495 = 123.
    std::cout << "Expected SIZEMAX (for Key=int, Value=int): " << TestNode::SIZEMAX
              << ", SPLIT_T: " << TestNode::SPLIT_T
              << ", MERGE_T: " << TestNode::MERGE_T << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    // Initialize IOManager (MemoryManager for this test)
    // MemoryManager might print debug info for its operations if you've added them.
    std::unique_ptr<IOManager> manager = std::make_unique<MemoryManager>(); // 100 pages capacity

    // Test: Constructor (ID only) - using PagePtr/PageRef
    std::cout << "Test: Constructor (ID only via PagePtr/PageRef)... ";
    PagePtr<TestNode> node1_ptr = allocate<TestNode>(manager.get()); // E.g., Allocates page 0. MemoryManager::NewPage() called.
    {
        // PagePtr::make_ref calls BPTNode(page_id_t) constructor.
        // Then Serialize is called.
        PageRef<TestNode> node1_ref = node1_ptr.make_ref(node1_ptr.page_id());
        assert(node1_ref->get_self() == node1_ptr.page_id());
        assert(node1_ref->current_size_ == 0);
        assert(node1_ref->prev_node_id_ == INVALID_PAGE_ID);
        assert(node1_ref->next_node_id_ == INVALID_PAGE_ID);
        // node1_ref goes out of scope. If dirty, Serialize and Page::flush (MemoryManager::WritePage) called.
    }
    std::cout << "Passed." << std::endl;

    // Test: Constructor (ID, size, data) - using PagePtr/PageRef
    std::cout << "Test: Constructor (ID, size, data via PagePtr/PageRef)... ";
    ValueT init_data[] = {{1,10}, {3,30}, {5,50}};
    PagePtr<TestNode> node2_ptr = allocate<TestNode>(manager.get()); // E.g., Allocates page 1. MemoryManager::NewPage() called.
    {
        // PagePtr::make_ref calls BPTNode(page_id_t, size_t, data_type[]) constructor.
        PageRef<TestNode> node2_ref = node2_ptr.make_ref(node2_ptr.page_id(), 3, init_data);
        assert(node2_ref->get_self() == node2_ptr.page_id());
        assert(node2_ref->current_size_ == 3);
        assert(node2_ref->at(0).first == 1 && node2_ref->at(0).second == 10);
        assert(node2_ref->at(1).first == 3 && node2_ref->at(1).second == 30);
        assert(node2_ref->at(2).first == 5 && node2_ref->at(2).second == 50);
    }
    std::cout << "Passed." << std::endl;

    // Test: search (read back node2 from memory)
    std::cout << "Test: search... ";
    {
        // PagePtr::get_ref calls MemoryManager::ReadPage, then Deserialize.
        PageRef<TestNode> node2_read_ref = node2_ptr.get_ref();
        assert(node2_read_ref->search(0) == static_cast<index_type>(INVALID_PAGE_ID));
        assert(node2_read_ref->search(1) == 0);
        assert(node2_read_ref->search(3) == 1);
        assert(node2_read_ref->search(5) == 2);
        assert(node2_read_ref->search(6) == 2);
    }
    PagePtr<TestNode> empty_node_ptr = allocate<TestNode>(manager.get()); // E.g., page 2
    {
        PageRef<TestNode> empty_node_ref = empty_node_ptr.make_ref(empty_node_ptr.page_id());
        assert(empty_node_ref->search(10) == static_cast<index_type>(INVALID_PAGE_ID));
    }
    std::cout << "Passed." << std::endl;

    // Test: at, head, get_first (on a mutable ref)
    std::cout << "Test: at, head, get_first... ";
    {
        PageRef<TestNode> node2_mut_ref = node2_ptr.get_ref(); // Get mutable ref to page 1
        assert(node2_mut_ref->at(1).first == 3 && node2_mut_ref->at(1).second == 30);
        assert(node2_mut_ref->head(1) == 3);
        node2_mut_ref->head(1) = 33; // Modify, PageRef becomes dirty
        assert(node2_mut_ref->at(1).first == 33 && node2_mut_ref->at(1).second == 30);
        node2_mut_ref->head(1) = 3; // Change back
        assert(node2_mut_ref->get_first() == 1);
        // node2_mut_ref goes out of scope, flushes changes to MemoryManager.
    }
    std::cout << "Passed." << std::endl;

    // Test: insert_at
    std::cout << "Test: insert_at... ";
    PagePtr<TestNode> node3_ptr = allocate<TestNode>(manager.get()); // E.g., page 3
    {
        PageRef<TestNode> node3_ref = node3_ptr.make_ref(node3_ptr.page_id());
        node3_ref->insert_at(static_cast<index_type>(INVALID_PAGE_ID), {2,20}); // Insert at index 0
        assert(node3_ref->current_size_ == 1 && node3_ref->at(0).first == 2);
        node3_ref->insert_at(0, {4,40}); // Insert at index 1 (after item at index 0)
        assert(node3_ref->current_size_ == 2 && node3_ref->at(1).first == 4);
        node3_ref->insert_at(static_cast<index_type>(INVALID_PAGE_ID), {1,10}); // Insert at index 0
        assert(node3_ref->current_size_ == 3 && node3_ref->at(0).first == 1 && node3_ref->at(1).first == 2 && node3_ref->at(2).first == 4);
        node3_ref->insert_at(1, {3,30}); // Insert at index 2 (after item at index 1, which is {2,20})
        assert(node3_ref->current_size_ == 4 && node3_ref->at(2).first == 3);
        // node3_ref->print_node(); // Expected: [{1,10}, {2,20}, {3,30}, {4,40}]
    }
    std::cout << "Passed." << std::endl;

    // Test: erase
    std::cout << "Test: erase... ";
    {
        PageRef<TestNode> node3_ref = node3_ptr.get_ref(); // Get ref to page 3
        // Node3: [{1,10}, {2,20}, {3,30}, {4,40}]
        node3_ref->erase(0); // Erase {1,10}
        assert(node3_ref->current_size_ == 3 && node3_ref->at(0).first == 2 && node3_ref->at(1).first == 3 && node3_ref->at(2).first == 4);
        node3_ref->erase(2); // Erase {4,40} (original index 2 of current data)
        assert(node3_ref->current_size_ == 2 && node3_ref->at(0).first == 2 && node3_ref->at(1).first == 3);
        node3_ref->erase(0); // Erase {2,20}
        assert(node3_ref->current_size_ == 1 && node3_ref->at(0).first == 3);
        node3_ref->erase(0); // Erase {3,30}
        assert(node3_ref->current_size_ == 0);
    }
    std::cout << "Passed." << std::endl;

    // Test: split
    std::cout << "Test: split..." << std::endl;
    PagePtr<TestNode> node_to_split_ptr = allocate<TestNode>(manager.get()); // E.g., page 4
    page_id_t original_node_id = node_to_split_ptr.page_id();
    page_id_t next_node_for_original_id = 301; // Dummy next ID for original node before split

    // Populate node_to_split_ptr
    {
        PageRef<TestNode> node_to_split_ref = node_to_split_ptr.make_ref(original_node_id);
        node_to_split_ref->next_node_id_ = next_node_for_original_id;
        ValueT split_data[] = {{1,10}, {2,20}, {3,30}, {4,40}, {5,50}};
        // Manually populate data for split test (BPTNode constructor doesn't take raw array directly for make_ref)
        // Or, insert them one by one if BPTNode constructor used by make_ref doesn't support this.
        // For this test, we'll assume BPTNode has a way to be populated or we get a ref and fill it.
        // The BPTNode(page_id, size, data) constructor is used by make_ref.
        // Let's re-initialize it properly for the test.
    } // Old ref dies
    { // Create and populate the node to be split
        PageRef<TestNode> node_to_split_ref = node_to_split_ptr.make_ref(original_node_id); // Re-create with just ID
        node_to_split_ref->next_node_id_ = next_node_for_original_id;
        ValueT split_data[] = {{1,10}, {2,20}, {3,30}, {4,40}, {5,50}};
        // Copy data directly for test setup (this bypasses insert_at logic for setup simplicity)
        std::memcpy(node_to_split_ref->data_, split_data, 5 * sizeof(ValueT));
        node_to_split_ref->current_size_ = 5;

        std::cout << "  Node before split (Page ID " << original_node_id << "):" << std::endl;
        print_bpt_node_details(*node_to_split_ref,std::cout);
    } // Flushes the populated node_to_split_ptr

    // Allocate a page for the new sibling
    PagePtr<TestNode> new_sibling_page_ptr = allocate<TestNode>(manager.get()); // E.g., page 5
    page_id_t new_sibling_id = new_sibling_page_ptr.page_id();

    {
        PageRef<TestNode> original_node_ref = node_to_split_ptr.get_ref(); // Get ref to original node (page 4)

        // Perform split. original_node_ref is modified.
        // new_sibling_page_ptr is used to create the new sibling node on disk/memory.
        // The split method returns a PageRef to this newly created and persisted sibling.
        PageRef<TestNode> new_sibling_ref = original_node_ref->split(new_sibling_page_ptr);

        std::cout << "  Original node after split (Page ID " << original_node_ref->get_self() << "):" << std::endl;
        print_bpt_node_details(*original_node_ref,std::cout);

        std::cout << "  New sibling node after split (Page ID " << new_sibling_ref->get_self() << "):" << std::endl;
        print_bpt_node_details(*new_sibling_ref,std::cout);

        // Assertions for original node (left child after split)
        // Original size 5, mid = 5/2 = 2. Original keeps 2 elements.
        assert(original_node_ref->current_size_ == 2);
        assert(original_node_ref->at(0).first == 1 && original_node_ref->at(1).first == 2);
        assert(original_node_ref->get_self() == original_node_id);
        assert(original_node_ref->next_node_id_ == new_sibling_id); // Points to new sibling

        // Assertions for new sibling node (right child after split)
        // New sibling gets 5 - 2 = 3 elements.
        assert(new_sibling_ref->current_size_ == 3);
        assert(new_sibling_ref->at(0).first == 3 && new_sibling_ref->at(1).first == 4 && new_sibling_ref->at(2).first == 5);
        assert(new_sibling_ref->get_self() == new_sibling_id);
        assert(new_sibling_ref->prev_node_id_ == original_node_id); // Points back to original
        assert(new_sibling_ref->next_node_id_ == next_node_for_original_id); // Inherits original's next

        // new_sibling_ref will flush page 5 (new sibling data)
        // original_node_ref will flush page 4 (modified original node data)
    }
    std::cout << "Test: split... Passed." << std::endl;


    // Test RFlowey::string (assuming original definitions from your first code block)
    std::cout << "Test: RFlowey::string... ";
    RFlowey::string<10> rfs1; // Default constructor
    assert(rfs1.empty() && rfs1.length() == 0 && RFlowey::string<10>::capacity() == 10);

    RFlowey::string<10> rfs2("hello"); // Constructor from char*
    assert(!rfs2.empty() && rfs2.length() == 5 && std::strcmp(rfs2.data(), "hello") == 0);
    // Note: rfs2.get_str() == "hello" depends on safe get_str or luck if not null-terminated by assign.
    // Assuming original assign: std::min(len, N), memcpy, then memset if bytes_to_copy < N.
    // For "hello", len=5, N=10. bytes_to_copy=5. memcpy "hello". memset a+5 to 0 for N-5 bytes. So it's null-terminated.
    assert(rfs2.get_str() == "hello");


    RFlowey::string<5> rfs3("world123"); // N=5. input "world123" (len 8)
    // assign: bytes_to_copy = std::min(8, 5) = 5. memcpy "world". a becomes {'w','o','r','l','d'}.
    // bytes_to_copy < N (5 < 5) is false. No memset. Not null-terminated by assign.
    assert(rfs3.length() == 5); // strnlen(a, 5) will return 5 if no null within first 5.
    // rfs3.get_str() behavior:
    // Original: std::string(a). If a is not null-terminated, UB.
    // If it happens to work (e.g. next byte in memory is 0), it might be "world".
    // For robust testing, a safer get_str() like std::string(a, length()) is needed.
    // We'll test strncmp which is safer for fixed-size non-null-terminated char arrays.
    assert(std::strncmp(rfs3.data(), "world", 5) == 0);
    // If your get_str() is std::string(a, strnlen(a,N)), then this would pass:
    // assert(rfs3.get_str() == "world");


    rfs1 = "test"; // operator=(const char*)
    assert(rfs1.length() == 4 && rfs1.get_str() == "test");

    RFlowey::string<3> rfs_full("abc"); // N=3, input "abc" (len 3)
    // assign: bytes_to_copy = std::min(3,3)=3. memcpy "abc". a is {'a','b','c'}. No memset.
    assert(rfs_full.length() == 3);
    assert(std::strncmp(rfs_full.data(), "abc", 3) == 0);
    // assert(rfs_full.get_str() == "abc"); // Same caveat as rfs3.get_str()
    assert(rfs_full.a[0] == 'a' && rfs_full.a[1] == 'b' && rfs_full.a[2] == 'c');

    // Test hash (original hash function)
    // Hashes all N characters. If hash result is 0, it becomes 114514.
    unsigned long long hash_full_val = RFlowey::hash(rfs_full); // Hashes 'a','b','c'
    assert(hash_full_val != 0);
    // It might become 114514 if the hash of 'a','b','c' was 0. This is unlikely but possible.

    RFlowey::string<3> rfs_empty_str; // Default constructed, a is {0,0,0}
    unsigned long long hash_empty_val = RFlowey::hash(rfs_empty_str); // Hashes {0,0,0}
    // Loop: hash += 0; hash *= 37; -> hash remains 0.
    // Then if (hash == 0), hash becomes 114514.
    assert(hash_empty_val == 114514);
    std::cout << "Passed." << std::endl;

        // --- Tests for SIZEMAX, SPLIT_T, MERGE_T ---
    std::cout << "Test: Node capacity (SIZEMAX)... ";
    {
        PagePtr<TestNode> cap_node_ptr = allocate<TestNode>(manager.get());
        PageRef<TestNode> cap_ref = cap_node_ptr.make_ref(cap_node_ptr.page_id());

        // Fill to SIZEMAX - 1
        set_node_size_and_fill_sequential(*cap_ref, TestNode::SIZEMAX - 1);
        assert(cap_ref->current_size_ == TestNode::SIZEMAX - 1);

        // Insert one more to reach SIZEMAX
        cap_ref->insert_at(cap_ref->current_size_ - 1, {TestNode::SIZEMAX + 10, TestNode::SIZEMAX + 10}); // Insert at end
        assert(cap_ref->current_size_ == TestNode::SIZEMAX);

        // Attempt to insert when full
        bool threw = false;
        try {
            cap_ref->insert_at(cap_ref->current_size_ - 1, {TestNode::SIZEMAX + 20, TestNode::SIZEMAX + 20});
        } catch (const std::overflow_error&) {
            threw = true;
        }
        assert(threw);
        assert(cap_ref->current_size_ == TestNode::SIZEMAX);
    }
    std::cout << "Passed." << std::endl;

    std::cout << "Test: is_upper_safe() around SPLIT_T... ";
    {
        PagePtr<TestNode> split_t_node_ptr = allocate<TestNode>(manager.get());
        PageRef<TestNode> st_ref = split_t_node_ptr.make_ref(split_t_node_ptr.page_id());

        if (TestNode::SPLIT_T > 2) { // Ensure SPLIT_T is large enough for this test
            set_node_size_and_fill_sequential(*st_ref, TestNode::SPLIT_T - 2);
            assert(st_ref->is_upper_safe()); // current_size_ < SPLIT_T - 1 is true

            st_ref->data_[TestNode::SPLIT_T - 2] = {TestNode::SPLIT_T - 2 + 1, TestNode::SPLIT_T - 2 + 1};
            st_ref->current_size_ = TestNode::SPLIT_T - 1;
            assert(!st_ref->is_upper_safe()); // current_size_ < SPLIT_T - 1 is false
        } else {
            std::cout << "Skipped (SPLIT_T too small for detailed check). ";
        }
    }
    std::cout << "Passed." << std::endl;

    std::cout << "Test: is_lower_safe() around MERGE_T... ";
    {
        PagePtr<TestNode> merge_t_node_ptr = allocate<TestNode>(manager.get());
        PageRef<TestNode> mt_ref = merge_t_node_ptr.make_ref(merge_t_node_ptr.page_id());

        if (TestNode::MERGE_T >= 0 && TestNode::MERGE_T < TestNode::SIZEMAX - 2) { // Ensure MERGE_T is valid for this test
            set_node_size_and_fill_sequential(*mt_ref, TestNode::MERGE_T + 2);
            assert(mt_ref->is_lower_safe()); // current_size_ > MERGE_T + 1 is true

            mt_ref->current_size_ = TestNode::MERGE_T + 1;
            assert(!mt_ref->is_lower_safe()); // current_size_ > MERGE_T + 1 is false

            if (TestNode::MERGE_T > 0) {
                mt_ref->current_size_ = TestNode::MERGE_T;
                 assert(!mt_ref->is_lower_safe()); // current_size_ > MERGE_T + 1 is false
            }
        } else {
            std::cout << "Skipped (MERGE_T not in testable range). ";
        }
    }
    std::cout << "Passed." << std::endl;

    std::cout << "Test: split() a full node (SIZEMAX)... ";
    {
        PagePtr<TestNode> full_node_ptr = allocate<TestNode>(manager.get());
        PageRef<TestNode> full_ref = full_node_ptr.make_ref(full_node_ptr.page_id());
        set_node_size_and_fill_sequential(*full_ref, TestNode::SIZEMAX, 1000);
        full_ref->next_node_id_ = 999; // Dummy next

        PagePtr<TestNode> sibling_ptr = allocate<TestNode>(manager.get());
        PageRef<TestNode> sibling_ref = full_ref->split(sibling_ptr);

        size_t expected_left_size = TestNode::SIZEMAX / 2;
        size_t expected_right_size = TestNode::SIZEMAX - expected_left_size;

        assert(full_ref->current_size_ == expected_left_size);
        assert(sibling_ref->current_size_ == expected_right_size);
        assert(full_ref->next_node_id_ == sibling_ptr.page_id());
        assert(sibling_ref->prev_node_id_ == full_node_ptr.page_id());
        assert(sibling_ref->next_node_id_ == 999);
        // Check first element of sibling
        assert(sibling_ref->at(0).first == 1000 + static_cast<int>(expected_left_size));
    }
    std::cout << "Passed." << std::endl;

    std::cout << "Test: split() an empty node... ";
    {
        PagePtr<TestNode> empty_split_ptr = allocate<TestNode>(manager.get());
        PageRef<TestNode> empty_split_ref = empty_split_ptr.make_ref(empty_split_ptr.page_id());
        assert(empty_split_ref->current_size_ == 0);

        PagePtr<TestNode> sibling_ptr = allocate<TestNode>(manager.get());
        PageRef<TestNode> sibling_ref = empty_split_ref->split(sibling_ptr);

        assert(empty_split_ref->current_size_ == 0); // 0 / 2 = 0
        assert(sibling_ref->current_size_ == 0);   // 0 - 0 = 0
    }
    std::cout << "Passed." << std::endl;

    std::cout << "Test: split() a node with 1 element... ";
    {
        PagePtr<TestNode> one_el_ptr = allocate<TestNode>(manager.get());
        PageRef<TestNode> one_el_ref = one_el_ptr.make_ref(one_el_ptr.page_id());
        set_node_size_and_fill_sequential(*one_el_ref, 1, 77);

        PagePtr<TestNode> sibling_ptr = allocate<TestNode>(manager.get());
        PageRef<TestNode> sibling_ref = one_el_ref->split(sibling_ptr);

        assert(one_el_ref->current_size_ == 0); // 1 / 2 = 0
        assert(sibling_ref->current_size_ == 1);   // 1 - 0 = 1
        assert(sibling_ref->at(0).first == 77);
    }
    std::cout << "Passed." << std::endl;

    std::cout << "Test: erase last element... ";
    {
        PagePtr<TestNode> erase_last_ptr = allocate<TestNode>(manager.get());
        PageRef<TestNode> el_ref = erase_last_ptr.make_ref(erase_last_ptr.page_id());
        set_node_size_and_fill_sequential(*el_ref, 3, 1); // {1,1},{2,2},{3,3}

        el_ref->erase(2); // Erase {3,3}
        assert(el_ref->current_size_ == 2);
        assert(el_ref->at(0).first == 1);
        assert(el_ref->at(1).first == 2);
    }
    std::cout << "Passed." << std::endl;

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "All BPTNode tests with IOManager completed." << std::endl;
    std::cout << "Note: RFlowey::string::get_str() tests assume it handles non-null-terminated internal buffers safely (e.g., by using length())." << std::endl;


    return 0;
}