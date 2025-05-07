#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cassert>
#include <map> // For verification

// Define BPT_SMALL_SIZE to use smaller SIZEMAX for easier split testing
#define BPT_SMALL_SIZE
// Define BPT_TEST before including BPT.h to enable internal BPT assertions
#define BPT_TEST

#include "src/BPT.h"
#include "src/utils/utils.h" // For RFlowey::string and RFlowey::hash

// --- Custom Hashers (same as before) ---
struct String64Hasher {
    RFlowey::hash_t operator()(const RFlowey::string<64>& s) const {
        return RFlowey::hash(s);
    }
};

struct IntHasher {
    RFlowey::hash_t operator()(const int& v) const {
        return static_cast<RFlowey::hash_t>(std::hash<int>{}(v));
    }
};

// --- Helper Functions (same as before) ---
RFlowey::string<64> make_rflowey_key(const char* prefix, int id) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s%d", prefix, id);
    return RFlowey::string<64>(buffer);
}

bool sjtu_vector_contains(const sjtu::vector<int>& vec, int value) {
    for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] == value) {
            return true;
        }
    }
    return false;
}

// Helper to verify BPT content against a reference map
void verify_bpt_content(
    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher>& bpt,
    const std::map<RFlowey::string<64>, std::vector<int>>& reference_map,
    const std::string& test_stage_msg) {

    std::cout << "--- Verifying BPT content: " << test_stage_msg << " ---" << std::endl;

    for (const auto& pair : reference_map) {
        const RFlowey::string<64>& key = pair.first;
        const std::vector<int>& expected_values_std_vec = pair.second; // std::vector from map

        sjtu::vector<int> found_values_sjtu_vec = bpt.find(key);

        // Sort both vectors for consistent comparison if order isn't guaranteed by BPT find
        std::vector<int> found_values_std_vec_sorted;
        for(size_t i = 0; i < found_values_sjtu_vec.size(); ++i) {
            found_values_std_vec_sorted.push_back(found_values_sjtu_vec[i]);
        }
        std::sort(found_values_std_vec_sorted.begin(), found_values_std_vec_sorted.end());

        std::vector<int> expected_values_std_vec_sorted = expected_values_std_vec;
        std::sort(expected_values_std_vec_sorted.begin(), expected_values_std_vec_sorted.end());

        assert(found_values_std_vec_sorted.size() == expected_values_std_vec_sorted.size() &&
               ("Value count mismatch for key: " + std::string(key.c_str()) + " at stage: " + test_stage_msg).c_str());

        for (size_t i = 0; i < expected_values_std_vec_sorted.size(); ++i) {
            assert(found_values_std_vec_sorted[i] == expected_values_std_vec_sorted[i] &&
                   ("Value mismatch for key: " + std::string(key.c_str()) + " at stage: " + test_stage_msg).c_str());
        }
    }
    std::cout << "--- Verification successful: " << test_stage_msg << " ---" << std::endl;
}
// You'll need to add a debug method to BPT to get the total number of unique keys
// For example, in BPT.h:
// size_t get_num_keys_in_map_for_debug() const { /* ... traverse and count unique keys ... */ return count; }
// For now, I'll comment out the part that uses it if you don't have it.
// A simpler way is to just iterate through the reference map and check each key in BPT.

void test_bpt_insert_find_split_small(const std::string& db_filename) {
    std::cout << "====== Starting BPT Insert/Find/Split Test (Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());

    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
    // Use a std::map to keep track of expected key-values for easier verification
    std::map<RFlowey::string<64>, std::vector<int>> reference_map;

    // SIZEMAX = 8, SPLIT_T = 6.
    // A leaf node splits when its current_size_ becomes >= SPLIT_T (6) due to an insert.
    // Or if it's full (8 items) and tries to insert the 9th.
    // Let's assume split happens when current_size would exceed SIZEMAX, or when it hits SPLIT_T and is no longer "safe".
    // For simplicity, let's aim to fill a node to SIZEMAX (8 items) then insert one more to guarantee a split.
    // Or, insert up to SPLIT_T (6 items). The 7th item should cause a split.

    std::cout << "--- Test: Basic Inserts (No Split Expected) ---" << std::endl;
    // Insert 5 items (SPLIT_T - 1)
    for (int i = 0; i < 5; ++i) {
        RFlowey::string<64> key = make_rflowey_key("key_", i);
        int value = i * 10;
        bpt.insert(key, value);
        reference_map[key].push_back(value);
    }
    verify_bpt_content(bpt, reference_map, "After 5 inserts");

    std::cout << "--- Test: Trigger First Leaf Split ---" << std::endl;
    // Insert items 5, 6, 7. Assuming SIZEMAX=8, SPLIT_T=6.
    // Item 5 (key_5): total 6 items. Leaf might split here or on next.
    // Item 6 (key_6): total 7 items. Leaf should split.
    // Item 7 (key_7): total 8 items. If not split before, splits now.
    // Let's insert up to 8 items to fill one leaf, then the 9th will cause a split.
    // Or, if SPLIT_T=6 is the hard trigger, inserting the 6th item (key_5) will cause a split.

    // Let's assume split happens when current_size reaches SPLIT_T (6)
    RFlowey::string<64> key5 = make_rflowey_key("key_", 5); // This is the 6th item
    int val5 = 50;
    std::cout << "Inserting 6th item (key_5), expecting potential leaf split..." << std::endl;
    bpt.insert(key5, val5);
    reference_map[key5].push_back(val5);
    verify_bpt_content(bpt, reference_map, "After 6th insert (potential first leaf split)");
    // At this point, the initial leaf (containing key_0 to key_5) should have split.
    // Root should now point to two leaves.

    std::cout << "--- Test: Insert More to Fill New Leaves & Potentially Trigger More Leaf Splits ---" << std::endl;
    // Insert items up to key_11 (total 12 items).
    // If first split resulted in two leaves with ~3 items each.
    // key_0, key_1, key_2 | key_3, key_4, key_5
    // Adding 6 more items (key_6 to key_11)
    for (int i = 6; i < 12; ++i) { // 6 more items
        RFlowey::string<64> key = make_rflowey_key("key_", i);
        int value = i * 10;
        std::cout << "Inserting item " << i << " (" << key.c_str() << ")" << std::endl;
        bpt.insert(key, value);
        reference_map[key].push_back(value);
    }
    verify_bpt_content(bpt, reference_map, "After 12 inserts (multiple leaf splits expected)");
    // With 12 items, and leaves splitting at 6, we expect 2 or 3 leaves.
    // e.g., L1: 0,1,2 | L2: 3,4,5 | L3: 6,7,8 | L4: 9,10,11 (if splits are perfectly balanced and SPLIT_T is strict)
    // Or L1: 0-5 (splits) -> L1a: 0-2, L1b: 3-5. Root: {key_3}
    // Insert 6-11:
    // key_6,7,8 into L1b (now 3-8, 6 items) -> splits -> L1b': 3-5, L1c: 6-8. Root: {key_3, key_6}
    // key_9,10,11 into L1c (now 6-11, 6 items) -> splits -> L1c': 6-8, L1d: 9-11. Root: {key_3, key_6, key_9}
    // We should have multiple entries in the root node now.

    std::cout << "--- Test: Trigger Inner Node Split (and Root Split) ---" << std::endl;
    // Inner node SPLIT_T is also 6 (assuming SIZEMAX=8 for all node types for this test).
    // The root is an inner node. It needs 6 child pointers (meaning 6 entries) to split.
    // Each leaf split (that isn't absorbed by a non-full parent) adds an entry to the parent.
    // We have keys 0-11.
    // Initial state: Root -> Leaf1 (empty sentinel)
    // After key_0 to key_5 inserted, Leaf1 has 6 items.
    // Insert key_5: Leaf1 splits into LeafA (key_0,1,2) and LeafB (key_3,4,5).
    //               Root now has 2 children: { {{0,0}, LeafA_id}, {key_3_hash_pair, LeafB_id} } (2 entries)
    //
    // To get 6 entries in the root, we need 5 more promoted keys.
    // This means 5 more leaf splits that propagate up.
    // Current items: 12 (key_0 to key_11).
    // Let's assume leaves hold ~3-5 items after splits.
    // We need to fill the root to 6 entries. It currently has ~3-4 entries from the 12 items.
    // Let's add items until the root has 6 entries and then one more to split it.
    // If each leaf split adds one to root, and root splits at 6 entries, we need 6 leaf splits.
    // Each leaf holds ~SPLIT_T/2 items after split (e.g. 3).
    // So, roughly 6 * 3 = 18 items might trigger a root split.
    // Let's insert up to key_23 (24 items). This should be plenty.
    int total_items_for_root_split = 24; // Should be enough for SIZEMAX=8, SPLIT_T=6
    for (int i = 12; i < total_items_for_root_split; ++i) {
        RFlowey::string<64> key = make_rflowey_key("key_", i);
        int value = i * 10;
        std::cout << "Inserting item " << i << " (" << key.c_str() << ") for inner/root split" << std::endl;
        bpt.insert(key, value);
        reference_map[key].push_back(value);
    }
    verify_bpt_content(bpt, reference_map, "After " + std::to_string(total_items_for_root_split) + " inserts (inner/root split expected)");
    // After this, the BPT layer should have increased.

    std::cout << "--- Test: Duplicate Key Inserts After Splits ---" << std::endl;
    RFlowey::string<64> key_dup1 = make_rflowey_key("key_", 1); // Exists
    RFlowey::string<64> key_dup7 = make_rflowey_key("key_", 7); // Exists
    RFlowey::string<64> key_dup15 = make_rflowey_key("key_", 15); // Exists

    bpt.insert(key_dup1, 111); reference_map[key_dup1].push_back(111);
    bpt.insert(key_dup7, 777); reference_map[key_dup7].push_back(777);
    bpt.insert(key_dup15, 1555); reference_map[key_dup15].push_back(1555);

    verify_bpt_content(bpt, reference_map, "After duplicate key inserts post-splits");

    std::cout << "--- Test: Finding Non-Existent Keys After Splits ---" << std::endl;
    RFlowey::string<64> key_non_exist1 = make_rflowey_key("key_", 999);
    RFlowey::string<64> key_non_exist2("z_non_existent");
    sjtu::vector<int> values;
    values = bpt.find(key_non_exist1);
    assert(values.empty() && "Find non-existent key_999 failed");
    values = bpt.find(key_non_exist2);
    assert(values.empty() && "Find non-existent z_non_existent failed");

    std::cout << "====== BPT Insert/Find/Split Test (Small SIZEMAX) Passed ======" << std::endl;
}


int main() {
    const std::string test_db_filename = "bpt_small_test.dat";


    test_bpt_insert_find_split_small(test_db_filename);
    std::cout << "\nAll BPT small tests completed successfully." << std::endl;

    std::cout << "--- Main: Cleaning up database file: " << test_db_filename << " ---" << std::endl;
    std::remove(test_db_filename.c_str());
    return 0;
}