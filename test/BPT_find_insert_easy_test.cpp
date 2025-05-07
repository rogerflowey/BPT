#include <iostream>
#include <vector>       // For std::vector to store keys/values for verification
#include <string>       // For std::string for key construction helper
#include <algorithm>    // For std::find (used on sjtu::vector)
#include <cstdio>       // For std::remove (to clean up database file)
#include <cassert>      // For assert()

// Define BPT_TEST before including BPT.h to enable internal BPT assertions
#define BPT_TEST

// Assuming BPT.h and its dependencies are in paths findable by the compiler.
// The include paths match those in your BPT.h file.
// common.h is included via Node.h.
#include "src/BPT.h"
#include "src/utils/utils.h" // For RFlowey::string and RFlowey::hash

// --- Custom Hashers for BPT ---
// BPT's KeyHash template parameter expects a type with operator()
// that takes const Key& and returns RFlowey::hash_t.
struct String64Hasher {
    RFlowey::hash_t operator()(const RFlowey::string<64>& s) const {
        // RFlowey::hash is defined in "src/utils/utils.h" and returns unsigned long long
        // which matches RFlowey::hash_t from "common.h".
        return RFlowey::hash(s);
    }
};

// BPT's ValueHash template parameter, similar to KeyHash.
struct IntHasher {
    RFlowey::hash_t operator()(const int& v) const {
        // std::hash<int> returns size_t. RFlowey::hash_t is unsigned long long.
        // This conversion is generally safe.
        return static_cast<RFlowey::hash_t>(std::hash<int>{}(v));
    }
};

// Helper function to create RFlowey::string<64> keys for testing
RFlowey::string<64> make_rflowey_key(const char* prefix, int id) {
    char buffer[64];
    // Ensure null termination by snprintf and that it fits RFlowey::string<64>
    snprintf(buffer, sizeof(buffer), "%s%d", prefix, id);
    return RFlowey::string<64>(buffer);
}

// Helper function to check if a value exists in an sjtu::vector
// This avoids std::sort if sjtu::vector iterators are not fully compatible
bool sjtu_vector_contains(const sjtu::vector<int>& vec, int value) {
    for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] == value) {
            return true;
        }
    }
    return false;
}


// Test function for core BPT operations (insert, find, splits)
void test_bpt_core_operations(const std::string& db_filename) {
    std::cout << "--- Test: Removing old database file: " << db_filename << " ---" << std::endl;
    std::remove(db_filename.c_str()); // Clean up any previous test run's file

    std::cout << "--- Test: Initializing BPT ---" << std::endl;
    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);

    std::cout << "--- Test: Basic Inserts ---" << std::endl;
    RFlowey::string<64> key_apple("apple");
    RFlowey::string<64> key_banana("banana");
    RFlowey::string<64> key_orange("orange");

    bpt.insert(key_apple, 10);
    bpt.insert(key_banana, 20); // First insert of banana, 20
    bpt.insert(key_orange, 30);

    std::cout << "--- Test: Basic Finds ---" << std::endl;
    sjtu::vector<int> values; // Using sjtu::vector as per BPT's return type

    values = bpt.find(key_apple);
    assert(values.size() == 1 && "Find apple: count mismatch");
    if (!values.empty()) assert(values[0] == 10 && "Find apple: value mismatch");

    values = bpt.find(key_banana);
    assert(values.size() == 1 && "Find banana: count mismatch"); // Expecting 1 after initial insert
    if (!values.empty()) assert(values[0] == 20 && "Find banana: value mismatch");

    values = bpt.find(key_orange);
    assert(values.size() == 1 && "Find orange: count mismatch");
    if (!values.empty()) assert(values[0] == 30 && "Find orange: value mismatch");

    RFlowey::string<64> key_grape("grape"); // A non-existent key
    values = bpt.find(key_grape);
    assert(values.empty() && "Find non-existent grape: should return empty vector");

    std::cout << "--- Test: Insert Duplicate Key (different value) ---" << std::endl;
    bpt.insert(key_apple, 100); // "apple" already exists with value 10
    values = bpt.find(key_apple);
    assert(values.size() == 2 && "Find apple after duplicate (diff value) insert: count mismatch");
    if (values.size() == 2) {
        // Order of values from find is not strictly guaranteed by BPT find,
        // so check for presence of both expected values.
        assert(sjtu_vector_contains(values, 10) && "Find apple after duplicate: missing value 10");
        assert(sjtu_vector_contains(values, 100) && "Find apple after duplicate: missing value 100");
    }

    // "Insert Duplicate Key (same value)" test section removed.
    // key_banana now only has one entry: (banana, 20)

    std::cout << "--- Test: Triggering Leaf Node Split ---" << std::endl;
    // From common.h: PAGESIZE = 4096, SPLIT_RATE = 3.0 / 4.
    // LeafNode::value_type is pair<pair<hash_t,hash_t>, pair<Key, Value>>
    // sizeof(pair<hash_t,hash_t>) = 16 bytes (for two ulonglong)
    // sizeof(pair<RFlowey::string<64>, int>) approx 64 (string data) + 4 (int) + padding ~ 68-72 bytes.
    // Total sizeof(LeafNode::value_type) approx 16 + 72 = 88 bytes.
    // SIZEMAX_Leaf = (4096 - 128) / 88 - 1 = 3968 / 88 - 1 = 45.09 - 1 = 44.
    // SPLIT_T_Leaf = (3.0/4.0) * 44 = 33.
    // A leaf node splits when its current_size_ >= SPLIT_T (i.e., at 33 elements).
    // Current items: apple (2), banana (1), orange (1) = 4 items, likely in one leaf.
    // To trigger a split, we need to add roughly 33 more items. Let's add 40 to be safe and cause multiple splits.

    std::vector<RFlowey::string<64>> bulk_inserted_keys; // Using std::vector for test harness storage
    std::vector<int> bulk_inserted_values;

    for (int i = 0; i < 40; ++i) {
        RFlowey::string<64> current_key = make_rflowey_key("item_", i);
        int current_value = i * 1000; // Unique values
        bpt.insert(current_key, current_value);
        bulk_inserted_keys.push_back(current_key);
        bulk_inserted_values.push_back(current_value);
    }

    std::cout << "--- Test: Find After Leaf Split ---" << std::endl;
    // Verify some of the initially inserted items are still findable
    values = bpt.find(key_apple);
    assert(values.size() == 2 && "Find apple post-split: count mismatch");
    if (values.size() == 2) { // Re-verify values
        assert(sjtu_vector_contains(values, 10) && "Find apple post-split: missing value 10");
        assert(sjtu_vector_contains(values, 100) && "Find apple post-split: missing value 100");
    }

    // Verify some of the bulk-inserted items
    for (int i = 0; i < 5; ++i) { // Check a few from the beginning of bulk insert
        values = bpt.find(bulk_inserted_keys[i]);
        assert(values.size() == 1 && "Find bulk item (early) post-split: count mismatch");
        if(!values.empty()) assert(values[0] == bulk_inserted_values[i] && "Find bulk item (early) post-split: value mismatch");
    }
    for (int i = 35; i < 40; ++i) { // Check a few from the end of bulk insert
        values = bpt.find(bulk_inserted_keys[i]);
        assert(values.size() == 1 && "Find bulk item (late) post-split: count mismatch");
        if(!values.empty()) assert(values[0] == bulk_inserted_values[i] && "Find bulk item (late) post-split: value mismatch");
    }

    std::cout << "--- Test: BPT destructor will save data. (End of core operations) ---" << std::endl;
    // When bpt goes out of scope, its destructor is called, which should save the BPT's state (root_id, layer).
}

// Test function for BPT persistence
void test_bpt_persistence(const std::string& db_filename) {
    std::cout << "--- Test: Persistence (re-opening BPT from file: " << db_filename << ") ---" << std::endl;

    // Create a new BPT instance, which should load from the existing db_filename
    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt_loaded(db_filename);

    std::cout << "--- Test: Verifying loaded data ---" << std::endl;
    sjtu::vector<int> values;

    // Verify items inserted in the previous test phase
    RFlowey::string<64> key_apple("apple");
    values = bpt_loaded.find(key_apple);
    assert(values.size() == 2 && "Persistence: Find apple count mismatch");
    if (values.size() == 2) {
        assert(sjtu_vector_contains(values, 10) && "Persistence: Find apple missing value 10");
        assert(sjtu_vector_contains(values, 100) && "Persistence: Find apple missing value 100");
    }

    RFlowey::string<64> key_banana("banana");
    values = bpt_loaded.find(key_banana);
    // After removing the "duplicate same value" insert, key_banana should only have one value (20).
    assert(values.size() == 1 && "Persistence: Find banana count mismatch");
     if (values.size() == 1) {
        assert(values[0] == 20 && "Persistence: Find banana value mismatch");
    }

    // Verify one of the bulk-inserted items
    RFlowey::string<64> bulk_key_to_check = make_rflowey_key("item_", 25); // An item from the middle of bulk insert
    int expected_bulk_value = 25 * 1000;
    values = bpt_loaded.find(bulk_key_to_check);
    assert(values.size() == 1 && "Persistence: Find bulk item_25 count mismatch");
    if(!values.empty()) assert(values[0] == expected_bulk_value && "Persistence: Find bulk item_25 value mismatch");

    std::cout << "--- Test: Further inserts on loaded BPT ---" << std::endl;
    RFlowey::string<64> key_watermelon("watermelon");
    bpt_loaded.insert(key_watermelon, 500);
    values = bpt_loaded.find(key_watermelon);
    assert(values.size() == 1 && "Persistence: Insert and find new item (watermelon) count mismatch");
    if(!values.empty()) assert(values[0] == 500 && "Persistence: Insert and find new item (watermelon) value mismatch");

    std::cout << "--- Test: Persistence test completed. BPT destructor will save again. ---" << std::endl;
}


int main() {
    const std::string test_db_filename = "bpt_test_suite.dat"; // Unique name for this test suite's DB file

    try {
        std::cout << "====== Starting BPT Core Operations Test ======" << std::endl;
        test_bpt_core_operations(test_db_filename);
        std::cout << "====== BPT Core Operations Test Passed ======" << std::endl;

        std::cout << "\n====== Starting BPT Persistence Test ======" << std::endl;
        test_bpt_persistence(test_db_filename);
        std::cout << "====== BPT Persistence Test Passed ======" << std::endl;

        std::cout << "\nAll BPT tests completed successfully." << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        std::cerr << "Test failed with std::exception: " << e.what() << std::endl;
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        std::remove(test_db_filename.c_str()); // Attempt to clean up on failure
        return 1; // Indicate failure
    } catch (...) {
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        std::cerr << "Test failed with an unknown exception." << std::endl;
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        std::remove(test_db_filename.c_str()); // Attempt to clean up on failure
        return 1; // Indicate failure
    }

    // Final cleanup after all tests pass
    std::cout << "--- Main: Cleaning up database file: " << test_db_filename << " ---" << std::endl;
    std::remove(test_db_filename.c_str());
    return 0; // Indicate success
}