#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cassert>
#include <map> // For verification
#include <random> // For random operations in comprehensive test
#include <set>    // For keeping track of keys in comprehensive test

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

// Helper to verify BPT content against a reference map
void verify_bpt_content(
    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher>& bpt,
    const std::map<RFlowey::string<64>, std::vector<int>>& reference_map,
    const std::string& test_stage_msg)
    {

    std::cout << "--- Verifying BPT content: " << test_stage_msg << " ---" << std::endl;

    for (const auto& pair : reference_map) {
        const RFlowey::string<64>& key = pair.first;
        std::vector<int> expected_values_std_vec = pair.second;

        sjtu::vector<int> found_values_sjtu_vec = bpt.find(key);

        std::vector<int> found_values_std_vec_sorted;
        for(size_t i = 0; i < found_values_sjtu_vec.size(); ++i) {
            found_values_std_vec_sorted.push_back(found_values_sjtu_vec[i]);
        }
        std::sort(found_values_std_vec_sorted.begin(), found_values_std_vec_sorted.end());
        std::sort(expected_values_std_vec.begin(), expected_values_std_vec.end());

        if (found_values_std_vec_sorted.size() != expected_values_std_vec.size()) {
            std::cerr << "Value count mismatch for key: " << key.c_str()
                      << " at stage: " << test_stage_msg
                      << ". Expected: " << expected_values_std_vec.size()
                      << ", Found: " << found_values_std_vec_sorted.size() << std::endl;
            std::cerr << "Expected values: ";
            for(int v : expected_values_std_vec) std::cerr << v << " ";
            std::cerr << std::endl;
            std::cerr << "Found values: ";
            for(int v : found_values_std_vec_sorted) std::cerr << v << " ";
            std::cerr << std::endl;
        }

        assert(found_values_std_vec_sorted.size() == expected_values_std_vec.size() &&
               ("Value count mismatch for key: " + std::string(key.c_str()) + " at stage: " + test_stage_msg).c_str());

        for (size_t i = 0; i < expected_values_std_vec.size(); ++i) {
             if (found_values_std_vec_sorted[i] != expected_values_std_vec[i]) {
                std::cerr << "Value mismatch for key: " << key.c_str()
                          << " at stage: " << test_stage_msg
                          << ". Index " << i << ": Expected " << expected_values_std_vec[i]
                          << ", Found: " << found_values_std_vec_sorted[i] << std::endl;
             }
            assert(found_values_std_vec_sorted[i] == expected_values_std_vec[i] &&
                   ("Value mismatch for key: " + std::string(key.c_str()) + " at stage: " + test_stage_msg).c_str());
        }
    }

    // Check for keys in BPT that are not in reference_map (requires BPT iteration or known key set)
    // This is complex without a BPT iterator. For now, we assume that if all reference keys are found
    // correctly, and non-existent keys (tested elsewhere) are not found, it's largely correct.
    // A more robust check would involve iterating all keys in the BPT.

    std::cout << "--- Verification successful: " << test_stage_msg << " ---" << std::endl;
}


// This map will store the state of the BPT to be used by the persistence test
std::map<RFlowey::string<64>, std::vector<int>> global_reference_map_for_persistence;

void test_bpt_insert_find_split_small(const std::string& db_filename) {
    std::cout << "====== Starting BPT Insert/Find/Split Test (Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());

    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
    global_reference_map_for_persistence.clear();

    // With BPT_SMALL_SIZE, Node.h: SIZEMAX = 12. SPLIT_T = (3/4)*12 - 1 = 9 - 1 = 8.
    // A node splits if current_size_ (after insert) becomes SPLIT_T (8) or more, due to:
    //   is_upper_safe() is `current_size_ < SPLIT_T - 1` (i.e., `< 7`).
    //   So, if current_size_ is 7 or 8 (or more), it's not upper_safe, `parents` not cleared.
    //   `split()` is called. `split()` asserts `current_size_ >= SPLIT_T` (i.e. `current_size_ >= 8`).
    //   This means an insert making current_size_ = 7 will call split() and fail its assertion.
    //   An insert making current_size_ = 8 will call split() and pass its assertion.
    // The test needs to be written carefully around this. The "6th item" causing split in the original
    // comment `SIZEMAX = 8, SPLIT_T = 6` would mean an insert making size 6 (SPLIT_T) splits.
    // Here, an insert making size 8 (SPLIT_T) should split.
    // Leaf node starts with 1 dummy entry.
    // To reach size 8: dummy + 7 actual items. So, inserting the 7th actual item.

    std::cout << "--- Test: Basic Inserts (No Split Expected based on SPLIT_T=8) ---" << std::endl;
    // Insert 6 items (key_0 to key_5). Leaf size will be 1 (dummy) + 6 = 7.
    // If current_size=7, is_upper_safe (7 < 7) is false. split() is called.
    // split() asserts (7 >= 8), which fails.
    // So, we can only insert up to 5 items safely before this bug is hit.
    // Or, the BPT code handles the SPLIT_T-1 case differently (it doesn't seem to).
    // Let's insert 6 items. key_0 to key_5. Leaf size becomes 1+6=7.
    // This should NOT cause a split that passes assertions.
    // If the BPT code is buggy and calls split() on size 7, it will fail an internal BPT_TEST assertion.
    // For this test to pass *without triggering the bug*, we insert up to size 6.
    for (int i = 0; i < 6; ++i) { // Insert 6 items (key_0 to key_5). Leaf size = 1 (dummy) + 6 = 7.
                                 // This is SPLIT_T-1. This is the problematic case for the BPT code.
                                 // If the BPT code calls split() here, its assert(current_size_ >= SPLIT_T) will fail.
                                 // Let's assume for now the test expects this NOT to split successfully.
        RFlowey::string<64> key = make_rflowey_key("key_", i);
        int value = i * 10;
        bpt.insert(key, value);
        global_reference_map_for_persistence[key].push_back(value);
    }
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After 6 inserts (leaf size 7, SPLIT_T-1)");


    std::cout << "--- Test: Trigger First Leaf Split (inserting 7th item, key_6, to make leaf size 8=SPLIT_T) ---" << std::endl;
    RFlowey::string<64> key6 = make_rflowey_key("key_", 6); // 7th actual item
    int val6 = 60;
    std::cout << "Inserting 7th actual item (key_6), leaf size becomes 8. Expecting leaf split..." << std::endl;
    bpt.insert(key6, val6);
    global_reference_map_for_persistence[key6].push_back(val6);
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After 7th insert (key_6) (first leaf split at size SPLIT_T)");

    std::cout << "--- Test: Insert More (More Leaf Splits) ---" << std::endl;
    // Each leaf can hold (SPLIT_T/2) items after split. SPLIT_T=8, so mid=4.
    // L0: dummy, k0,k1,k2 (size 4). L1: k3,k4,k5,k6 (size 4). Root has 2 ptrs.
    // To fill L1 and cause it to split: need 4 more items for L1.
    // Items key_7, key_8, key_9, key_10. (4 items)
    // Total items: key_0 .. key_10 (11 items).
    for (int i = 7; i < 11; ++i) {
        RFlowey::string<64> key = make_rflowey_key("key_", i);
        int value = i * 10;
        bpt.insert(key, value);
        global_reference_map_for_persistence[key].push_back(value);
    }
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After 11 inserts (more leaf splits)");

    std::cout << "--- Test: Trigger Inner Node Split (and Root Split) ---" << std::endl;
    // InnerNode SPLIT_T is also 8. Root needs 8 children to split.
    // Each leaf split adds one child pointer to parent.
    // Initial root: 2 children. Needs 6 more splits to have 8 children.
    // Each leaf holds ~4 items. So 6 more leaves means ~24 more items.
    // Total items for root split: 11 (current) + 24 = 35. (key_0 to key_34)
    // This is approximate. Let's try a number that should be sufficient.
    // The original test used 24 total items. Let's try that.
    int total_items_for_root_split = 24; // key_0 to key_23
    for (int i = 11; i < total_items_for_root_split; ++i) {
        RFlowey::string<64> key = make_rflowey_key("key_", i);
        int value = i * 10;
        bpt.insert(key, value);
        global_reference_map_for_persistence[key].push_back(value);
    }
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After " + std::to_string(total_items_for_root_split) + " inserts (inner/root split)");

    std::cout << "--- Test: Duplicate Key Inserts After Splits ---" << std::endl;
    std::vector<std::pair<int, int>> duplicates_to_insert = {
        {1, 111}, {7, 777}, {15, 1555}, {0, 8}, {20, 20202}
    };
    for(const auto& p : duplicates_to_insert) {
        RFlowey::string<64> key = make_rflowey_key("key_", p.first);
        bpt.insert(key, p.second);
        global_reference_map_for_persistence[key].push_back(p.second);
    }
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After duplicate key inserts post-splits");

    std::cout << "--- Test: Finding Non-Existent Keys After Splits ---" << std::endl;
    RFlowey::string<64> key_non_exist1 = make_rflowey_key("key_", 999);
    RFlowey::string<64> key_non_exist2("z_non_existent");
    sjtu::vector<int> values;
    values = bpt.find(key_non_exist1);
    assert(values.empty() && "Find non-existent key_999 failed");
    values = bpt.find(key_non_exist2);
    assert(values.empty() && "Find non-existent z_non_existent failed");

    std::cout << "====== BPT Insert/Find/Split Test (Small SIZEMAX) Passed ======" << std::endl;
    std::cout << "--- BPT destructor will save data to " << db_filename << " ---" << std::endl;
}

void test_bpt_persistence_small(const std::string& db_filename) {
    std::cout << "\n====== Starting BPT Persistence Test (Small SIZEMAX) ======" << std::endl;

    std::cout << "--- Test: Re-opening BPT from file: " << db_filename << " ---" << std::endl;
    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt_loaded(db_filename);

    std::cout << "--- Test: Verifying loaded data against reference map ---" << std::endl;
    assert(!global_reference_map_for_persistence.empty() && "Reference map for persistence test is empty!");
    verify_bpt_content(bpt_loaded, global_reference_map_for_persistence, "Loaded data from file");

    std::cout << "--- Test: Further inserts on loaded BPT ---" << std::endl;
    RFlowey::string<64> key_new1("new_alpha");
    RFlowey::string<64> key_new2("new_beta");
    bpt_loaded.insert(key_new1, 1001); global_reference_map_for_persistence[key_new1].push_back(1001);
    bpt_loaded.insert(key_new2, 2002); global_reference_map_for_persistence[key_new2].push_back(2002);

    RFlowey::string<64> existing_key_for_dup = make_rflowey_key("key_", 10);
    if (global_reference_map_for_persistence.count(existing_key_for_dup)) { // Ensure key exists before adding duplicate
        bpt_loaded.insert(existing_key_for_dup, 101010);
        global_reference_map_for_persistence[existing_key_for_dup].push_back(101010);
    } else {
        std::cout << "Warning: key_10 not found in reference map during persistence test, skipping duplicate insert for it." << std::endl;
    }


    verify_bpt_content(bpt_loaded, global_reference_map_for_persistence, "After inserts on loaded BPT");

    std::cout << "====== BPT Persistence Test (Small SIZEMAX) Passed ======" << std::endl;
    std::cout << "--- BPT_loaded destructor will save data again to " << db_filename << " ---" << std::endl;
}

void test_bpt_multiple_values_per_key(const std::string& db_filename_prefix) {
    const std::string db_filename = db_filename_prefix + "_multi_value.dat";
    std::cout << "\n====== Starting BPT Multiple Values Per Key Test (Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());

    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
    std::map<RFlowey::string<64>, std::vector<int>> reference_map;

    RFlowey::string<64> key_multi("multi_key");
    RFlowey::string<64> key_alpha("alpha_key");
    RFlowey::string<64> key_beta("beta_key");

    std::cout << "--- Test: Inserting multiple values for a single key ---" << std::endl;
    bpt.insert(key_multi, 100); reference_map[key_multi].push_back(100);
    bpt.insert(key_multi, 200); reference_map[key_multi].push_back(200);
    bpt.insert(key_multi, 50);  reference_map[key_multi].push_back(50);
    bpt.insert(key_multi, 150); reference_map[key_multi].push_back(150);
    verify_bpt_content(bpt, reference_map, "After 4 inserts for key_multi");

    std::cout << "--- Test: Inserting other keys around the multi-value key ---" << std::endl;
    bpt.insert(key_alpha, 10); reference_map[key_alpha].push_back(10);
    bpt.insert(key_beta, 20);  reference_map[key_beta].push_back(20);
    verify_bpt_content(bpt, reference_map, "After inserting alpha and beta keys");

    std::cout << "--- Test: Adding more values to key_multi ---" << std::endl;
    bpt.insert(key_multi, 300); reference_map[key_multi].push_back(300);
    bpt.insert(key_multi, 25);  reference_map[key_multi].push_back(25);
    verify_bpt_content(bpt, reference_map, "After adding 2 more values to key_multi (total 6 for key_multi)");

    std::cout << "--- Test: Forcing splits by inserting many other unique keys ---" << std::endl;
    for (int i = 0; i < 30; ++i) {
        RFlowey::string<64> key = make_rflowey_key("filler_", i);
        int value = i * 1000;
        bpt.insert(key, value);
        reference_map[key].push_back(value);
    }
    verify_bpt_content(bpt, reference_map, "After inserting 30 filler keys (splits expected)");

    std::cout << "--- Test: Verifying multi_key values again after splits ---" << std::endl;
    sjtu::vector<int> multi_values_sjtu = bpt.find(key_multi);
    std::vector<int> multi_values_std;
    for (size_t i = 0; i < multi_values_sjtu.size(); ++i) {
        multi_values_std.push_back(multi_values_sjtu[i]);
    }
    std::sort(multi_values_std.begin(), multi_values_std.end());

    std::vector<int> expected_multi_values = reference_map[key_multi];
    std::sort(expected_multi_values.begin(), expected_multi_values.end());

    assert(multi_values_std.size() == 6 && "key_multi count mismatch after splits");
    assert(multi_values_std == expected_multi_values && "key_multi values mismatch after splits");
    std::cout << "key_multi still has all 6 correct values after splits." << std::endl;

    std::cout << "--- Test: Inserting a duplicate value for key_multi ---" << std::endl;
    bpt.insert(key_multi, 200);
    reference_map[key_multi].push_back(200);
    verify_bpt_content(bpt, reference_map, "After inserting a duplicate value (200) for key_multi");

    sjtu::vector<int> multi_values_after_dup = bpt.find(key_multi);
    assert(multi_values_after_dup.size() == 7 && "key_multi count mismatch after duplicate value insert");

    std::cout << "====== BPT Multiple Values Per Key Test Passed ======" << std::endl;
}

void test_bpt_pressure(const std::string& db_filename_prefix) {
    const std::string db_filename = db_filename_prefix + "_pressure_large.dat";
    std::cout << "\n====== Starting BPT Pressure Test (Large Scale, Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());

    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
    std::map<RFlowey::string<64>, std::vector<int>> reference_map;

    const int num_items_sequential = 1000;
    const int num_items_reverse = 800;
    const int num_items_random = 1200;
    const int print_interval = num_items_sequential / 10;

    std::cout << "--- Pressure Test: Sequential Insertions (" << num_items_sequential << " items) ---" << std::endl;
    for (int i = 0; i < num_items_sequential; ++i) {
        RFlowey::string<64> key = make_rflowey_key("seq_", i);
        int value = i;
        if (i > 0 && i % print_interval == 0) {
            std::cout << "Inserting sequential item " << i << "/" << num_items_sequential << std::endl;
        }
        bpt.insert(key, value);
        reference_map[key].push_back(value);
    }
    std::cout << "Sequential insertions complete. Verifying..." << std::endl;
    verify_bpt_content(bpt, reference_map, "After " + std::to_string(num_items_sequential) + " sequential inserts");

    std::cout << "--- Pressure Test: Reverse Sequential Insertions (" << num_items_reverse << " items) ---" << std::endl;
    for (int i = 0; i < num_items_reverse; ++i) {
        RFlowey::string<64> key = make_rflowey_key("zrev_", num_items_reverse - 1 - i);
        int value = (num_items_reverse - 1 - i) + 10000;
        if (i > 0 && i % (num_items_reverse / 10) == 0) {
            std::cout << "Inserting reverse item " << (num_items_reverse - 1 - i) << "/" << num_items_reverse << std::endl;
        }
        bpt.insert(key, value);
        reference_map[key].push_back(value);
    }
    std::cout << "Reverse sequential insertions complete. Verifying..." << std::endl;
    verify_bpt_content(bpt, reference_map, "After " + std::to_string(num_items_reverse) + " reverse inserts");

    std::cout << "--- Pressure Test: Random Order Insertions (" << num_items_random << " items) ---" << std::endl;
    std::vector<int> random_indices(num_items_random);
    for(int i=0; i<num_items_random; ++i) random_indices[i] = i;

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(random_indices.begin(), random_indices.end(), g);

    for (int i = 0; i < num_items_random; ++i) {
        int k_idx = random_indices[i];
        RFlowey::string<64> key = make_rflowey_key("rand_", k_idx);
        int value = k_idx + 20000;
        if (i > 0 && i % (num_items_random / 10) == 0) {
            std::cout << "Inserting random item " << i << "/" << num_items_random << " (index " << k_idx << ")" << std::endl;
        }
        bpt.insert(key, value);
        reference_map[key].push_back(value);
    }
    std::cout << "Random order insertions complete. Verifying..." << std::endl;
    verify_bpt_content(bpt, reference_map, "After " + std::to_string(num_items_random) + " random inserts");

    std::cout << "--- Pressure Test: Adding Duplicates to Randomly Spread Keys (many values) ---" << std::endl;
    std::vector<RFlowey::string<64>> keys_for_many_duplicates;
    if (num_items_sequential > 50) keys_for_many_duplicates.push_back(make_rflowey_key("seq_", 50));
    if (num_items_reverse > 50) keys_for_many_duplicates.push_back(make_rflowey_key("zrev_", 50));
    if (num_items_random > 50 && random_indices.size() > 50) keys_for_many_duplicates.push_back(make_rflowey_key("rand_", random_indices[50]));

    if (keys_for_many_duplicates.empty() && !reference_map.empty()) {
        keys_for_many_duplicates.push_back(reference_map.begin()->first);
    }

    for(const auto& key_to_spam : keys_for_many_duplicates) {
        if (reference_map.count(key_to_spam)) {
            std::cout << "Adding 10 more values to existing key: " << key_to_spam.c_str() << std::endl;
            for (int j = 0; j < 10; ++j) {
                int new_value = reference_map[key_to_spam][0] + 70000 + j;
                bpt.insert(key_to_spam, new_value);
                reference_map[key_to_spam].push_back(new_value);
            }
        }
    }
    std::cout << "Duplicate value insertions complete. Verifying..." << std::endl;
    verify_bpt_content(bpt, reference_map, "After adding many duplicate values to spread keys");

    std::cout << "Total unique keys in reference map: " << reference_map.size() << std::endl;
    size_t total_values = 0;
    for(const auto& pair_ : reference_map) { // Changed pair to pair_ to avoid conflict with outer scope
        total_values += pair_.second.size();
    }
    std::cout << "Total key-value pairs in reference map: " << total_values << std::endl;

    std::cout << "====== BPT Pressure Test (Large Scale) Passed ======" << std::endl;
}

// NEW ERASE TEST
void test_bpt_erase_small(const std::string& db_filename) {
    std::cout << "\n====== Starting BPT Erase Test (Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());

    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
    // Use global_reference_map_for_persistence for this test sequence
    global_reference_map_for_persistence.clear();

    // SIZEMAX = 12, SPLIT_T = 8, MERGE_T = 2
    // Leaf node starts with 1 dummy entry.
    // A node is underfull if current_size_ <= MERGE_T (2).
    // is_lower_safe(): current_size_ > MERGE_T+1 (i.e. >3) OR prev_node_id_ == INVALID_PAGE_ID.
    // So, if current_size_ is 3, 2, 1, 0 AND prev_node_id_ exists, it's not lower safe.

    std::cout << "--- Erase Test: Initial Population ---" << std::endl;
    // Populate to create a few nodes, well above MERGE_T
    // Insert 10 items (key_0 to key_9). Should cause splits.
    // key_0..key_6 -> size 8 -> split L0 (dummy,k0,k1,k2), L1 (k3,k4,k5,k6)
    // key_7 -> L1 (k3,k4,k5,k6,k7) size 5
    // key_8 -> L1 (k3,k4,k5,k6,k7,k8) size 6
    // key_9 -> L1 (k3,k4,k5,k6,k7,k8,k9) size 7
    // key_10 -> L1 size 8 -> split L1 into L1' (k3,k4,k5,k6), L2 (k7,k8,k9,k10)
    // Root: { {0,0}, L0_id }, { hash(k3), L1'_id }, { hash(k7), L2_id }
    // L0: size 4. L1': size 4. L2: size 4. All are > MERGE_T+1 (3). So they are lower_safe.
    for (int i = 0; i < 11; ++i) { // key_0 to key_10
        RFlowey::string<64> key = make_rflowey_key("key_", i);
        int value = i * 10;
        bpt.insert(key, value);
        global_reference_map_for_persistence[key].push_back(value);
    }
    // Add a duplicate value
    bpt.insert(make_rflowey_key("key_", 5), 555);
    global_reference_map_for_persistence[make_rflowey_key("key_", 5)].push_back(555);
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After initial population for erase test");

    std::cout << "--- Erase Test: Erase non-existent key ---" << std::endl;
    bool erased = bpt.erase(make_rflowey_key("non_existent_key", 0), 0);
    assert(!erased && "Erase non-existent key should return false");
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After trying to erase non-existent key");

    std::cout << "--- Erase Test: Erase non-existent value for existing key ---" << std::endl;
    erased = bpt.erase(make_rflowey_key("key_", 5), 9999); // key_5 exists, value 9999 does not
    assert(!erased && "Erase non-existent value for existing key should return false");
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After trying to erase non-existent value");

    std::cout << "--- Erase Test: Erase existing value (key_5, 50) from node L1' (k3,k4,k5,k6) ---" << std::endl;
    // L1' (k3,k4,k5,k6) has key_5 with value 50 and 555. Size of L1' is 4.
    // Erasing (key_5, 50). L1' still has (k3,30), (k4,40), (k5,555), (k6,60). Size 4. No underflow.
    RFlowey::string<64> key_to_erase1 = make_rflowey_key("key_", 5);
    int val_to_erase1 = 50;
    erased = bpt.erase(key_to_erase1, val_to_erase1);
    assert(erased && "Erase existing (key_5, 50) failed");
    auto& vec1 = global_reference_map_for_persistence[key_to_erase1];
    vec1.erase(std::remove(vec1.begin(), vec1.end(), val_to_erase1), vec1.end());
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After erasing (key_5, 50)");

    std::cout << "--- Erase Test: Erase another existing value (key_5, 555), making key_5 empty ---" << std::endl;
    // L1' now has (k3,30), (k4,40), (k5,555), (k6,60). Size 4.
    // Erasing (key_5, 555). L1' becomes (k3,30), (k4,40), (k6,60). Size 3.
    // Size 3 is MERGE_T+1. Still lower_safe.
    int val_to_erase2 = 555;
    erased = bpt.erase(key_to_erase1, val_to_erase2);
    assert(erased && "Erase existing (key_5, 555) failed");
    auto& vec2 = global_reference_map_for_persistence[key_to_erase1];
    vec2.erase(std::remove(vec2.begin(), vec2.end(), val_to_erase2), vec2.end());
    if (vec2.empty()) {
        global_reference_map_for_persistence.erase(key_to_erase1);
    }
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After erasing (key_5, 555)");
    // Now key_5 should not be found or have an empty list of values.
    sjtu::vector<int> key5_vals = bpt.find(key_to_erase1);
    assert(key5_vals.empty() && "Key_5 should have no values after erasing both");


    std::cout << "--- Erase Test: Erase (key_3, 30) from L1' (k3,k4,k6) -> L1' (k4,k6) ---" << std::endl;
    // L1' has (k3,30), (k4,40), (k6,60). Size 3.
    // Erasing (key_3, 30). L1' becomes (k4,40), (k6,60). Size 2.
    // Size 2 is MERGE_T. This node is now AT MERGE_T.
    // If prev_node_id_ exists (L0 is prev of L1'), it's not lower_safe.
    // This is where the BPT's incomplete erase underflow logic might be problematic.
    // The BPT::erase might not correctly merge/redistribute L1'.
    RFlowey::string<64> key_to_erase3 = make_rflowey_key("key_", 3);
    int val_to_erase3 = 30;
    erased = bpt.erase(key_to_erase3, val_to_erase3);
    assert(erased && "Erase existing (key_3, 30) failed");
    auto& vec3 = global_reference_map_for_persistence[key_to_erase3];
    vec3.erase(std::remove(vec3.begin(), vec3.end(), val_to_erase3), vec3.end());
    if (vec3.empty()) {
        global_reference_map_for_persistence.erase(key_to_erase3);
    }
    std::cout << "Verifying after erasing (key_3, 30) - node L1' size becomes MERGE_T (2). Potential underflow issues." << std::endl;
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After erasing (key_3, 30), L1' at MERGE_T");

    std::cout << "--- Erase Test: Erase (key_4, 40) from L1' (k4,k6) -> L1' (k6) ---" << std::endl;
    // L1' has (k4,40), (k6,60). Size 2.
    // Erasing (key_4, 40). L1' becomes (k6,60). Size 1.
    // Size 1 is < MERGE_T. Definitely underfull.
    RFlowey::string<64> key_to_erase4 = make_rflowey_key("key_", 4);
    int val_to_erase4 = 40;
    erased = bpt.erase(key_to_erase4, val_to_erase4);
    assert(erased && "Erase existing (key_4, 40) failed");
    auto& vec4 = global_reference_map_for_persistence[key_to_erase4];
    vec4.erase(std::remove(vec4.begin(), vec4.end(), val_to_erase4), vec4.end());
    if (vec4.empty()) {
        global_reference_map_for_persistence.erase(key_to_erase4);
    }
    std::cout << "Verifying after erasing (key_4, 40) - node L1' size becomes 1 (underfull). Expecting BPT issues if not handled." << std::endl;
    verify_bpt_content(bpt, global_reference_map_for_persistence, "After erasing (key_4, 40), L1' underfull");

    std::cout << "====== BPT Erase Test (Small SIZEMAX) Potentially Passed (depends on underflow handling) ======" << std::endl;
    std::cout << "--- BPT destructor will save data to " << db_filename << " ---" << std::endl;
}

void test_bpt_big_erase(const std::string& db_filename_prefix) {
    const std::string db_filename = db_filename_prefix + "_big_erase.dat";
    std::cout << "\n====== Starting BPT Big Erase Test (Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());

    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
    std::map<RFlowey::string<64>, std::vector<int>> reference_map;

    const int num_items_to_insert = 40;

    std::cout << "--- Big Erase Test: Population Phase ---" << std::endl;
    for (int i = 0; i < num_items_to_insert; ++i) {
        RFlowey::string<64> key = make_rflowey_key("bigk_", i);
        int value = i * 100;
        bpt.insert(key, value);
        reference_map[key].push_back(value);
    }
    verify_bpt_content(bpt, reference_map, "After population of " + std::to_string(num_items_to_insert) + " items");
    bpt.print_tree_structure(); // Print after population

    std::cout << "--- Big Erase Test: Erasure Phase (from end to start) ---" << std::endl;
    for (int i = num_items_to_insert - 1; i >= 0; --i) {
        RFlowey::string<64> key_to_erase = make_rflowey_key("bigk_", i);
        int value_to_erase = i * 100;

        std::cout << "\nAttempting to erase: " << key_to_erase.c_str() << " -> " << value_to_erase
                  << " (Item " << i << ")" << std::endl;

        // --- Pre-erase check: Can we find the item we are about to erase? ---
        sjtu::vector<int> values_before_erase = bpt.find(key_to_erase);
        bool found_in_bpt_before_erase = false;
        for (size_t v_idx = 0; v_idx < values_before_erase.size(); ++v_idx) {
            if (values_before_erase[v_idx] == value_to_erase) {
                found_in_bpt_before_erase = true;
                break;
            }
        }
        if (!found_in_bpt_before_erase) {
            std::cerr << "ERROR: Pre-erase check FAILED. Item " << key_to_erase.c_str() << " -> " << value_to_erase
                      << " not found in BPT right before trying to erase it." << std::endl;
            bpt.print_tree_structure(); // Print tree on pre-erase find failure
            // You might want to assert here or throw to stop the test immediately
            assert(found_in_bpt_before_erase && "Pre-erase find check failed.");
        } else {
            std::cout << "Pre-erase check PASSED for " << key_to_erase.c_str() << std::endl;
        }
        // --- End Pre-erase check ---


        bool erased = bpt.erase(key_to_erase, value_to_erase);
        bpt.print_tree_structure();

        if (!erased) {
            std::cerr << "ERROR: bpt.erase returned false for " << key_to_erase.c_str() << " -> " << value_to_erase << std::endl;
            bpt.print_tree_structure(); // Print tree on erase failure
        }
        // This is your original assertion that fails:
        assert(erased && ("Failed to erase existing item: " + std::string(key_to_erase.c_str())).c_str());


        // Update reference map
        auto it_ref = reference_map.find(key_to_erase);
        if (it_ref != reference_map.end()) {
            auto& vec = it_ref->second;
            vec.erase(std::remove(vec.begin(), vec.end(), value_to_erase), vec.end());
            if (vec.empty()) {
                reference_map.erase(it_ref);
            }
        } else {
            // This should not happen if 'erased' was true
            std::cerr << "Warning: Erased item from BPT but it was not in reference_map: " << key_to_erase.c_str() << std::endl;
        }

        // --- Post-erase check: Verify the specific erased item is gone ---
        sjtu::vector<int> values_after_erase = bpt.find(key_to_erase);
        bool still_found_in_bpt_after_erase = false;
        for (size_t v_idx = 0; v_idx < values_after_erase.size(); ++v_idx) {
            if (values_after_erase[v_idx] == value_to_erase) {
                still_found_in_bpt_after_erase = true;
                break;
            }
        }
        if (still_found_in_bpt_after_erase) {
             std::cerr << "ERROR: Post-erase check FAILED. Item " << key_to_erase.c_str() << " -> " << value_to_erase
                      << " was still found in BPT after erase returned true." << std::endl;
            bpt.print_tree_structure();
            assert(!still_found_in_bpt_after_erase && "Post-erase find check failed.");
        }

    }
    std::cout << "--- Final verification after all erasures ---" << std::endl;
    verify_bpt_content(bpt, reference_map, "After erasing all items");
    assert(reference_map.empty() && "Reference map should be empty after all erasures");
    bpt.print_tree_structure(); // Print final empty tree structure

    // ... (rest of your test, e.g., inserting into empty tree) ...
    std::cout << "====== BPT Big Erase Test Passed ======" << std::endl;
}

// Test 2: Erase and Insert Mixed
void test_bpt_erase_insert_mixed(const std::string& db_filename_prefix) {
    const std::string db_filename = db_filename_prefix + "_mixed_ops.dat";
    std::cout << "\n====== Starting BPT Erase/Insert Mixed Test (Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());

    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
    std::map<RFlowey::string<64>, std::vector<int>> reference_map;

    auto do_insert = [&](const RFlowey::string<64>& k, int v, const std::string& step_msg) {
        std::cout << "MIXED_OP: INSERT " << k.c_str() << " -> " << v << " (" << step_msg << ")" << std::endl;
        bpt.insert(k, v);
        reference_map[k].push_back(v);
        verify_bpt_content(bpt, reference_map, step_msg);
    };

    auto do_erase = [&](const RFlowey::string<64>& k, int v, const std::string& step_msg) {
        std::cout << "MIXED_OP: ERASE " << k.c_str() << " -> " << v << " (" << step_msg << ")" << std::endl;
        bool erased = bpt.erase(k, v);
        assert(erased && ("Failed to erase " + std::string(k.c_str()) + "->" + std::to_string(v) + " at step: " + step_msg).c_str());

        auto& vec = reference_map[k];
        vec.erase(std::remove(vec.begin(), vec.end(), v), vec.end());
        if (vec.empty()) {
            reference_map.erase(k);
        }
        verify_bpt_content(bpt, reference_map, step_msg);
    };

    // SIZEMAX=12, SPLIT_T=8, MERGE_T=2
    // Leaf node starts with 1 sentinel.
    // After split, nodes have 8/2 = 4 items.

    std::cout << "--- Mixed Test: Initial Population (15 items: k0-k14) ---" << std::endl;
    // This should result in: Root -> L0, L1, L2, L3 (each leaf size 4, Root size 4)
    for (int i = 0; i < 15; ++i) {
        do_insert(make_rflowey_key("k", i), i * 10, "Populate k" + std::to_string(i));
    }
    // Expected structure:
    // L0: (sentinel, k0, k1, k2)
    // L1: (k3, k4, k5, k6)
    // L2: (k7, k8, k9, k10)
    // L3: (k11, k12, k13, k14)
    // Root: (ptr_L0), {hash(k3), ptr_L1}, {hash(k7), ptr_L2}, {hash(k11), ptr_L3}

    std::cout << "\n--- Mixed Test: Step 1: Erase k6 (L1 size 4 -> 3) ---" << std::endl;
    do_erase(make_rflowey_key("k", 6), 60, "Erase k6"); // L1: (k3,k4,k5), size 3 (MERGE_T+1, safe)

    std::cout << "\n--- Mixed Test: Step 2: Erase k5 (L1 size 3 -> 2, merge with L0) ---" << std::endl;
    do_erase(make_rflowey_key("k", 5), 50, "Erase k5");
    // L1: (k3,k4), size 2 (MERGE_T). Merges with L0 (sent,k0,k1,k2).
    // L0_merged: (sent,k0,k1,k2,k3,k4), size 6.
    // Root: (ptr_L0_merged), {hash(k7), ptr_L2}, {hash(k11), ptr_L3}, size 3.

    std::cout << "\n--- Mixed Test: Step 3: Insert k2a (into L0_merged, size 6 -> 7) ---" << std::endl;
    do_insert(make_rflowey_key("k02a_", 0), 25, "Insert k02a_0"); // Assuming k02a hashes between k2 and k3
    // L0_merged: (sent,k0,k1,k2, k02a_0, k3,k4), size 7.

    std::cout << "\n--- Mixed Test: Step 4: Insert k2b (into L0_merged, size 7 -> 8, split L0_merged) ---" << std::endl;
    do_insert(make_rflowey_key("k02b_", 0), 27, "Insert k02b_0"); // Assuming k02b hashes between k02a_0 and k3
    // L0_merged splits: L0_new (sent,k0,k1,k2), L1_new (k02a_0, k02b_0, k3,k4). Both size 4.
    // Root: (ptr_L0_new), {hash(k02a_0), ptr_L1_new}, {hash(k7), ptr_L2}, {hash(k11), ptr_L3}, size 4.

    std::cout << "\n--- Mixed Test: Step 5: Erase k11, k12 (L3 size 4 -> 2, merge with L2) ---" << std::endl;
    do_erase(make_rflowey_key("k", 11), 110, "Erase k11"); // L3: (k12,k13,k14), size 3
    do_erase(make_rflowey_key("k", 12), 120, "Erase k12"); // L3: (k13,k14), size 2. Merges with L2.
    // L2: (k7,k8,k9,k10). L3: (k13,k14).
    // L2_merged: (k7,k8,k9,k10,k13,k14), size 6.
    // Root: (ptr_L0_new), {hash(k02a_0), ptr_L1_new}, {hash(k7), ptr_L2_merged}, size 3.

    std::cout << "\n--- Mixed Test: Step 6: Insert duplicate for k7 (into L2_merged, size 6 -> 7) ---" << std::endl;
    do_insert(make_rflowey_key("k", 7), 777, "Insert duplicate k7->777");
    // L2_merged: (k7,k7(dup),k8,k9,k10,k13,k14), size 7.

    std::cout << "\n--- Mixed Test: Step 7: Erase all from L1_new (k02a_0, k02b_0, k3, k4) -> L1_new merges with L0_new ---" << std::endl;
    do_erase(make_rflowey_key("k02a_", 0), 25, "Erase k02a_0"); // L1_new size 3
    do_erase(make_rflowey_key("k02b_", 0), 27, "Erase k02b_0"); // L1_new size 2
    do_erase(make_rflowey_key("k", 3), 30, "Erase k3");       // L1_new size 1
    do_erase(make_rflowey_key("k", 4), 40, "Erase k4");       // L1_new size 0 (empty of user data). Merges.
    // L0_new: (sent,k0,k1,k2). L1_new becomes empty.
    // L0_final: (sent,k0,k1,k2), size 4.
    // Root: (ptr_L0_final), {hash(k7), ptr_L2_merged}, size 2.

    std::cout << "\n--- Mixed Test: Step 8: Erase all from L2_merged -> L2_merged merges with L0_final ---" << std::endl;
    do_erase(make_rflowey_key("k", 7), 70, "Erase k7->70");
    do_erase(make_rflowey_key("k", 7), 777, "Erase k7->777");
    do_erase(make_rflowey_key("k", 8), 80, "Erase k8");
    do_erase(make_rflowey_key("k", 9), 90, "Erase k9");
    do_erase(make_rflowey_key("k", 10), 100, "Erase k10");
    do_erase(make_rflowey_key("k", 13), 130, "Erase k13");
    do_erase(make_rflowey_key("k", 14), 140, "Erase k14"); // L2_merged becomes empty. Merges.
    // L0_final: (sent,k0,k1,k2). L2_merged becomes empty.
    // L0_super_final: (sent,k0,k1,k2), size 4.
    // Root: (ptr_L0_super_final), size 1. (Layer 0, root points to single leaf, stable).

    std::cout << "\n--- Mixed Test: Step 9: Erase all from L0_super_final (k0,k1,k2) ---" << std::endl;
    do_erase(make_rflowey_key("k", 0), 0, "Erase k0");
    do_erase(make_rflowey_key("k", 1), 10, "Erase k1");
    do_erase(make_rflowey_key("k", 2), 20, "Erase k2");
    // L0_super_final: (sentinel), size 1. (Underfull, but only leaf, stable).
    // Root: (ptr_L0_super_final), size 1.
    assert(reference_map.empty() && "Reference map should be empty after all mixed erasures");

    std::cout << "\n--- Mixed Test: Step 10: Final check, insert one item ---" << std::endl;
    do_insert(make_rflowey_key("final", 0), 9999, "Insert final_0 into empty tree");

    std::cout << "====== BPT Erase/Insert Mixed Test Passed ======" << std::endl;
}

void test_bpt_erase_insert_mixed_verbose(const std::string& db_filename_prefix) {
    const std::string db_filename = db_filename_prefix + "_mixed_ops_verbose.dat";
    std::cout << "\n====== Starting BPT Erase/Insert Mixed Test (VERBOSE - Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());

    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
    std::map<RFlowey::string<64>, std::vector<int>> reference_map;

    auto do_insert_verbose = [&](const RFlowey::string<64>& k, int v, const std::string& step_msg) {
        std::cout << "\nMIXED_OP_VERBOSE: INSERT " << k.c_str() << " -> " << v << " (" << step_msg << ")" << std::endl;
        bpt.insert(k, v);
        reference_map[k].push_back(v);
        verify_bpt_content(bpt, reference_map, step_msg + " (after insert)");
        bpt.print_tree_structure(); // Print tree after insert and verification
    };

    auto do_erase_verbose = [&](const RFlowey::string<64>& k, int v, const std::string& step_msg) {
        std::cout << "\nMIXED_OP_VERBOSE: ERASE " << k.c_str() << " -> " << v << " (" << step_msg << ")" << std::endl;

        // Pre-erase find check (optional but good for debugging find itself)
        sjtu::vector<int> vals_before = bpt.find(k);
        bool found_before = false;
        for(size_t i=0; i<vals_before.size(); ++i) if(vals_before[i] == v) found_before = true;
        if(!found_before) {
            std::cerr << "WARNING: Pre-erase find for " << k.c_str() << "->" << v << " failed within ERASE op." << std::endl;
        }

        bool erased = bpt.erase(k, v); // This is where the find_pos assertion might fail
        bpt.print_tree_structure();
        if (!erased) {
             std::cerr << "ERROR: bpt.erase(" << k.c_str() << ", " << v << ") returned false at step: " << step_msg << std::endl;
        }
        assert(erased && ("Failed to erase " + std::string(k.c_str()) + "->" + std::to_string(v) + " at step: " + step_msg).c_str());

        auto it_ref = reference_map.find(k);
        if (it_ref != reference_map.end()) {
            auto& vec = it_ref->second;
            vec.erase(std::remove(vec.begin(), vec.end(), v), vec.end());
            if (vec.empty()) {
                reference_map.erase(it_ref);
            }
        }
        verify_bpt_content(bpt, reference_map, step_msg + " (after erase)");
         // Print tree after erase and verification
    };

    // SIZEMAX=12, SPLIT_T=8, MERGE_T=2
    std::cout << "--- Mixed Test (VERBOSE): Initial Population (15 items: k0-k14) ---" << std::endl;
    for (int i = 0; i < 15; ++i) {
        do_insert_verbose(make_rflowey_key("k", i), i * 10, "Populate k" + std::to_string(i));
    }

    std::cout << "\n--- Mixed Test (VERBOSE): Step 1: Erase k6 (L1 size 4 -> 3) ---" << std::endl;
    do_erase_verbose(make_rflowey_key("k", 6), 60, "Erase k6");
    // ^^^ This is where your previous failure occurred. The tree print AFTER this (if it passes)
    // or the tree print from within erase (if it fails there) will be key.

    std::cout << "\n--- Mixed Test (VERBOSE): Step 2: Erase k5 (L1 size 3 -> 2, merge with L0) ---" << std::endl;
    do_erase_verbose(make_rflowey_key("k", 5), 50, "Erase k5");

    std::cout << "\n--- Mixed Test (VERBOSE): Step 3: Insert k2a (into L0_merged, size 6 -> 7) ---" << std::endl;
    do_insert_verbose(make_rflowey_key("k02a_", 0), 25, "Insert k02a_0");

    std::cout << "\n--- Mixed Test (VERBOSE): Step 4: Insert k2b (into L0_merged, size 7 -> 8, split L0_merged) ---" << std::endl;
    do_insert_verbose(make_rflowey_key("k02b_", 0), 27, "Insert k02b_0");

    std::cout << "\n--- Mixed Test (VERBOSE): Step 5: Erase k11, k12 (L3 size 4 -> 2, merge with L2) ---" << std::endl;
    do_erase_verbose(make_rflowey_key("k", 11), 110, "Erase k11");
    do_erase_verbose(make_rflowey_key("k", 12), 120, "Erase k12");

    std::cout << "\n--- Mixed Test (VERBOSE): Step 6: Insert duplicate for k7 (into L2_merged, size 6 -> 7) ---" << std::endl;
    do_insert_verbose(make_rflowey_key("k", 7), 777, "Insert duplicate k7->777");

    std::cout << "\n--- Mixed Test (VERBOSE): Step 7: Erase all from L1_new (k02a_0, k02b_0, k3, k4) -> L1_new merges with L0_new ---" << std::endl;
    do_erase_verbose(make_rflowey_key("k02a_", 0), 25, "Erase k02a_0");
    do_erase_verbose(make_rflowey_key("k02b_", 0), 27, "Erase k02b_0");
    do_erase_verbose(make_rflowey_key("k", 3), 30, "Erase k3");
    do_erase_verbose(make_rflowey_key("k", 4), 40, "Erase k4");

    std::cout << "\n--- Mixed Test (VERBOSE): Step 8: Erase all from L2_merged -> L2_merged merges with L0_final ---" << std::endl;
    do_erase_verbose(make_rflowey_key("k", 7), 70, "Erase k7->70");
    do_erase_verbose(make_rflowey_key("k", 7), 777, "Erase k7->777"); // Erase the duplicate
    do_erase_verbose(make_rflowey_key("k", 8), 80, "Erase k8");
    do_erase_verbose(make_rflowey_key("k", 9), 90, "Erase k9");
    do_erase_verbose(make_rflowey_key("k", 10), 100, "Erase k10");
    do_erase_verbose(make_rflowey_key("k", 13), 130, "Erase k13");
    do_erase_verbose(make_rflowey_key("k", 14), 140, "Erase k14");

    std::cout << "\n--- Mixed Test (VERBOSE): Step 9: Erase all from L0_super_final (k0,k1,k2) ---" << std::endl;
    do_erase_verbose(make_rflowey_key("k", 0), 0, "Erase k0");
    do_erase_verbose(make_rflowey_key("k", 1), 10, "Erase k1");
    do_erase_verbose(make_rflowey_key("k", 2), 20, "Erase k2");

    assert(reference_map.empty() && "Reference map should be empty after all mixed erasures");

    std::cout << "\n--- Mixed Test (VERBOSE): Step 10: Final check, insert one item ---" << std::endl;
    do_insert_verbose(make_rflowey_key("final", 0), 9999, "Insert final_0 into empty tree");

    std::cout << "====== BPT Erase/Insert Mixed Test (VERBOSE) Passed ======" << std::endl;
}

void test_bpt_many_values_single_key_large(const std::string& db_filename_prefix) {
    std::map<RFlowey::string<64>, std::vector<int>> reference_map;
    const std::string db_filename = db_filename_prefix + "_many_values_large.dat";
    std::cout << "\n====== Starting BPT Many Values for Single Key (Large >100) Test (Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());
    std::mt19937 rng(std::random_device{}()); // Random number generator
    const int TOTAL_TARGET_VALUES = 150; // More than 100
    const int FILLER_KEYS_PER_BATCH = 10; // Enough to cause splits (SPLIT_T=8)
    const int TARGET_VALUES_INSERT_INTERVAL = 5; // Insert fillers every 5 target value inserts

    RFlowey::string<64> target_key("super_multi_value_key");
    {


        RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);




        reference_map[target_key] = {}; // Initialize for target key

        int filler_key_id_counter = 0;

        std::cout << "--- Phase 1: Population with one key having many values and interleaved fillers ---" << std::endl;
        for (int i = 0; i < TOTAL_TARGET_VALUES; ++i) {
            int current_target_value = 200000 + i; // Unique values for the target key
            if (i % 20 == 0) { // Print progress periodically
                std::cout << "Inserting for target key: " << target_key.c_str() << " -> " << current_target_value << " (value #" << i + 1 << "/" << TOTAL_TARGET_VALUES << ")" << std::endl;
            }
            bpt.insert(target_key, current_target_value);
            reference_map[target_key].push_back(current_target_value);

            // After every few target_key inserts, add a batch of filler keys
            if ((i + 1) % TARGET_VALUES_INSERT_INTERVAL == 0 || i == TOTAL_TARGET_VALUES - 1) {
                std::cout << "--- Adding a batch of " << FILLER_KEYS_PER_BATCH << " filler keys (after target value #" << i+1 << ") ---" << std::endl;
                for (int j = 0; j < FILLER_KEYS_PER_BATCH; ++j) {
                    RFlowey::string<64> filler_key = make_rflowey_key("filler_manyL_", filler_key_id_counter);
                    int filler_value = 300000 + filler_key_id_counter;
                    bpt.insert(filler_key, filler_value);
                    reference_map[filler_key].push_back(filler_value);
                    filler_key_id_counter++;
                }
                verify_bpt_content(bpt, reference_map, "After target val #" + std::to_string(i + 1) + " and filler batch " + std::to_string(filler_key_id_counter/FILLER_KEYS_PER_BATCH));
            }
        }

        std::cout << "--- Final verification after all population ---" << std::endl;
        verify_bpt_content(bpt, reference_map, "After all population in many_values_single_key_large");

        std::cout << "--- Specific check for target_key's content ---" << std::endl;
        sjtu::vector<int> target_values_sjtu = bpt.find(target_key);
        std::vector<int> target_values_std;
        for(size_t k=0; k<target_values_sjtu.size(); ++k) target_values_std.push_back(target_values_sjtu[k]);
        std::sort(target_values_std.begin(), target_values_std.end());

        std::vector<int> expected_target_values = reference_map[target_key]; // Already populated
        std::sort(expected_target_values.begin(), expected_target_values.end());

        assert(target_values_std.size() == TOTAL_TARGET_VALUES && "Target key value count mismatch");
        assert(target_values_std == expected_target_values && "Target key value content mismatch");
        std::cout << "Target key " << target_key.c_str() << " successfully holds " << target_values_std.size() << " values." << std::endl;

        // --- Phase 2: Erase some values from the target key ---
        int num_to_erase_from_target = TOTAL_TARGET_VALUES / 10; // Erase 10%
        std::cout << "--- Phase 2: Erasing " << num_to_erase_from_target << " values from target key ---" << std::endl;
        for (int i = 0; i < num_to_erase_from_target; ++i) {
            if (reference_map[target_key].empty()) {
                std::cout << "Target key's value list is empty, stopping erase from target." << std::endl;
                break;
            }
            int val_idx_to_erase = rng() % reference_map[target_key].size();
            int val_to_erase = reference_map[target_key][val_idx_to_erase];

            // std::cout << "Erasing from target: " << target_key.c_str() << " -> " << val_to_erase << std::endl;
            bool erased = bpt.erase(target_key, val_to_erase);
            assert(erased && "Failed to erase existing value from target key");
            reference_map[target_key].erase(reference_map[target_key].begin() + val_idx_to_erase);
        }
        verify_bpt_content(bpt, reference_map, "After erasing some values from target_key");
        std::cout << "Target key " << target_key.c_str() << " now holds " << reference_map[target_key].size() << " values." << std::endl;


        // --- Phase 3: Erase some filler keys ---
        std::vector<RFlowey::string<64>> current_filler_keys;
        for(const auto& pair_ref : reference_map){ // Renamed pair to pair_ref
            if(pair_ref.first != target_key) {
                current_filler_keys.push_back(pair_ref.first);
            }
        }
        if (!current_filler_keys.empty()) {
            int num_filler_to_erase = std::max(1, (int)current_filler_keys.size() / 5); // Erase at least 1 or 20%
            std::cout << "--- Phase 3: Erasing " << num_filler_to_erase << " filler keys ---" << std::endl;
            std::shuffle(current_filler_keys.begin(), current_filler_keys.end(), rng);

            for (int i = 0; i < num_filler_to_erase; ++i) {
                if (i >= current_filler_keys.size()) break;
                RFlowey::string<64> key_to_erase = current_filler_keys[i];
                if (reference_map.count(key_to_erase) && !reference_map[key_to_erase].empty()) {
                    int val_to_erase = reference_map[key_to_erase][0];
                    // std::cout << "Erasing filler: " << key_to_erase.c_str() << " -> " << val_to_erase << std::endl;
                    bool erased = bpt.erase(key_to_erase, val_to_erase);
                    assert(erased && "Failed to erase existing filler key-value");
                    reference_map[key_to_erase].erase(reference_map[key_to_erase].begin());
                    if (reference_map[key_to_erase].empty()) {
                        reference_map.erase(key_to_erase);
                    }
                }
            }
            verify_bpt_content(bpt, reference_map, "After erasing some filler keys");
        } else {
            std::cout << "--- Phase 3: No filler keys to erase ---" << std::endl;
        }
    }

    // --- Phase 4: Persistence Check ---
    std::cout << "--- Phase 4: Testing Persistence ---" << std::endl;
    std::map<RFlowey::string<64>, std::vector<int>> final_ref_map = reference_map;
    // bpt goes out of scope here, destructor runs, saves data.
    {
        std::cout << "--- Reloading BPT from file: " << db_filename << " ---" << std::endl;
        RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt_reloaded(db_filename);
        verify_bpt_content(bpt_reloaded, final_ref_map, "Reloaded data after many_values_single_key_large test");

        // Quick check on reloaded target key
        sjtu::vector<int> reloaded_target_sjtu = bpt_reloaded.find(target_key);
        assert(reloaded_target_sjtu.size() == final_ref_map[target_key].size() && "Reloaded target key value count mismatch");
    }

    std::cout << "====== BPT Many Values for Single Key (Large >100) Test Passed ======" << std::endl;
}


// NEW COMPREHENSIVE TEST
void test_bpt_comprehensive_small(const std::string& db_filename_prefix) {
    const std::string db_filename = db_filename_prefix + "_comprehensive.dat";
    std::cout << "\n====== Starting BPT Comprehensive Test (Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());


    std::map<RFlowey::string<64>, std::vector<int>> reference_map;
    std::set<RFlowey::string<64>> existing_keys_set; // To quickly pick keys for erase/find

    std::mt19937 rng(25565); // Random number generator

    const int num_initial_inserts = 1500; // Enough to cause some splits
    const int num_operations = 10000;    // Mix of inserts, erases, finds
    const int max_val_per_key = 100;
    {
        RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
        std::cout << "--- Comprehensive Test: Initial Population ---" << std::endl;
        for (int i = 0; i < num_initial_inserts; ++i) {
            RFlowey::string<64> key = make_rflowey_key("comp_", i);
            int value = i * 100 + rng() % 100;
            bpt.insert(key, value);
            reference_map[key].push_back(value);
            existing_keys_set.insert(key);
        }
        verify_bpt_content(bpt, reference_map, "After initial population");

        std::cout << "--- Comprehensive Test: Mixed Operations ---" << std::endl;
        for (int i = 0; i < num_operations; ++i) {
            int op_type = rng() % 3; // 0: insert, 1: erase, 2: find

            if (op_type == 0) { // Insert
                RFlowey::string<64> key;
                if (!existing_keys_set.empty() && (rng() % 2 == 0)) { // Add to existing key
                    int key_idx = rng() % existing_keys_set.size();
                    auto it = existing_keys_set.begin();
                    std::advance(it, key_idx);
                    key = *it;
                } else { // Add new key
                    key = make_rflowey_key("comp_new_", i + num_initial_inserts);
                    existing_keys_set.insert(key);
                }
                int value = i * 10 + rng() % 10;
                if (reference_map[key].size() < max_val_per_key) { // Limit values per key for this test
                    std::cout << "Op " << i << ": INSERT " << key.c_str() << " -> " << value << std::endl;
                    bpt.insert(key, value);
                    reference_map[key].push_back(value);
                }
            } else if (op_type == 1) { // Erase
                if (!existing_keys_set.empty()) {
                    int key_idx = rng() % existing_keys_set.size();
                    auto it = existing_keys_set.begin();
                    std::advance(it, key_idx);
                    RFlowey::string<64> key_to_erase = *it;

                    if (!reference_map[key_to_erase].empty()) {
                        int val_idx = rng() % reference_map[key_to_erase].size();
                        int val_to_erase = reference_map[key_to_erase][val_idx];
                        std::cout << "Op " << i << ": ERASE " << key_to_erase.c_str() << " -> " << val_to_erase << std::endl;
                        bpt.erase(key_to_erase, val_to_erase);
                        reference_map[key_to_erase].erase(reference_map[key_to_erase].begin() + val_idx);
                        if (reference_map[key_to_erase].empty()) {
                            reference_map.erase(key_to_erase);
                            existing_keys_set.erase(key_to_erase); // Remove from set if all values gone
                        }
                    }
                }
            } else { // Find
                RFlowey::string<64> key_to_find;
                if (!existing_keys_set.empty() && (rng() % 2 == 0)) { // Find existing key
                    int key_idx = rng() % existing_keys_set.size();
                    auto it = existing_keys_set.begin();
                    std::advance(it, key_idx);
                    key_to_find = *it;
                } else { // Find potentially non-existing key
                    key_to_find = make_rflowey_key("comp_find_rand_", rng()%100);
                }
                std::cout << "Op " << i << ": FIND " << key_to_find.c_str() << std::endl;
                sjtu::vector<int> found_vals_sjtu = bpt.find(key_to_find);

                std::vector<int> expected_vals;
                if (reference_map.count(key_to_find)) {
                    expected_vals = reference_map[key_to_find];
                }
                std::sort(expected_vals.begin(), expected_vals.end());

                std::vector<int> found_vals_std;
                for(size_t k=0; k<found_vals_sjtu.size(); ++k) found_vals_std.push_back(found_vals_sjtu[k]);
                std::sort(found_vals_std.begin(), found_vals_std.end());

                assert(found_vals_std == expected_vals && "Find operation mismatch");
            }
            //bpt.print_tree_structure();

            if (i % (num_operations / 10) == 0 && i > 0) { // Verify periodically
                verify_bpt_content(bpt, reference_map, "After " + std::to_string(i) + " mixed operations");
            }
        }
        verify_bpt_content(bpt, reference_map, "After all mixed operations");
    }
    std::cout << "--- Comprehensive Test: Final State Persistence ---" << std::endl;
    // BPT destructor will save. Now reload and verify.
    // Need to copy reference_map as it will be used by the loaded BPT.
    std::map<RFlowey::string<64>, std::vector<int>> final_ref_map = reference_map;
    // bpt goes out of scope, destructor runs.
    {
        std::cout << "--- Comprehensive Test: Reloading and Verifying ---" << std::endl;
        RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt_reloaded(db_filename);
        verify_bpt_content(bpt_reloaded, final_ref_map, "Reloaded data after comprehensive test");
    }


    std::cout << "====== BPT Comprehensive Test (Small SIZEMAX) Passed ======" << std::endl;
}
// Add this new test function with the others

void test_bpt_super_duped_and_comprehensive_mixed(const std::string& db_filename_prefix) {
    const std::string db_filename = db_filename_prefix + "_super_duped_comp.dat";
    std::cout << "\n====== Starting BPT Super-Duped Keys & Comprehensive Mixed Test (Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());

    // --- Test Parameters ---
    const int NUM_SUPER_DUPED_KEYS = 3;
    const int INITIAL_VALUES_PER_SUPER_KEY = 50; // Test will generate unique values for these
    const int MAX_VALUES_PER_SUPER_KEY = 150;    // Max unique values test will add to a super key

    const int NUM_INITIAL_NORMAL_KEYS = 100;
    const int INITIAL_VALUES_PER_NORMAL_KEY_AVG = 2; // Normal keys can have duplicate values
    const int MAX_VALUES_PER_NORMAL_KEY = 10;

    const int NUM_MIXED_OPERATIONS = 1000; // Total operations in the mixed phase
    const int VERIFICATION_INTERVAL = NUM_MIXED_OPERATIONS / 20; // Verify every 50 ops if 1000 total

    std::map<RFlowey::string<64>, std::vector<int>> reference_map;
    std::vector<RFlowey::string<64>> list_of_super_duped_keys;
    std::map<RFlowey::string<64>, int> next_value_tracker_for_super_keys; // Tracks next unique value to insert
    std::set<RFlowey::string<64>> set_of_normal_keys;

    std::mt19937 rng(42); // Seed with random_device for variability
    // std::mt19937 rng(12345); // Fixed seed for reproducibility during debugging

    { // Scope for BPT object lifetime
        RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);

        // --- Phase 1: Initial Population ---
        std::cout << "--- Super/Comp Test: Initial Population ---" << std::endl;

        // 1a. Populate Super-Duped Keys
        std::cout << "Populating " << NUM_SUPER_DUPED_KEYS << " super-duped keys..." << std::endl;
        for (int i = 0; i < NUM_SUPER_DUPED_KEYS; ++i) {
            RFlowey::string<64> super_key = make_rflowey_key("super_", i);
            list_of_super_duped_keys.push_back(super_key);
            reference_map[super_key] = {}; // Initialize vector

            int base_value = i * 1000000; // Ensure unique value ranges for different super keys
            for (int j = 0; j < INITIAL_VALUES_PER_SUPER_KEY; ++j) {
                int unique_value = base_value + j;
                bpt.insert(super_key, unique_value);
                reference_map[super_key].push_back(unique_value);
            }
            next_value_tracker_for_super_keys[super_key] = base_value + INITIAL_VALUES_PER_SUPER_KEY;
        }

        // 1b. Populate Normal Keys
        std::cout << "Populating " << NUM_INITIAL_NORMAL_KEYS << " normal keys..." << std::endl;
        for (int i = 0; i < NUM_INITIAL_NORMAL_KEYS; ++i) {
            RFlowey::string<64> normal_key = make_rflowey_key("norm_", i);
            set_of_normal_keys.insert(normal_key);
            reference_map[normal_key] = {}; // Initialize vector

            int num_vals_for_this_key = 1 + (rng() % INITIAL_VALUES_PER_NORMAL_KEY_AVG);
            for (int j = 0; j < num_vals_for_this_key; ++j) {
                int value = rng() % 1000; // Normal keys can have duplicate values
                bpt.insert(normal_key, value);
                reference_map[normal_key].push_back(value);
            }
        }
        verify_bpt_content(bpt, reference_map, "After initial population");

        // --- Phase 2: Mixed Operations ---
        std::cout << "--- Super/Comp Test: Mixed Operations (" << NUM_MIXED_OPERATIONS << " ops) ---" << std::endl;
        for (int op_count = 0; op_count < NUM_MIXED_OPERATIONS; ++op_count) {
            int op_type_roll = rng() % 100;

            if (op_count > 0 && op_count % 100 == 0) {
                 std::cout << "Op " << op_count << "/" << NUM_MIXED_OPERATIONS << "..." << std::endl;
            }

            if (op_type_roll < 40) { // 40% Insert
                RFlowey::string<64> key_to_insert;
                int value_to_insert;
                bool is_super_key_target = false;

                int insert_target_roll = rng() % 100;
                if (insert_target_roll < 20 && !list_of_super_duped_keys.empty()) { // 20% target super key
                    key_to_insert = list_of_super_duped_keys[rng() % list_of_super_duped_keys.size()];
                    if (reference_map[key_to_insert].size() < MAX_VALUES_PER_SUPER_KEY) {
                        value_to_insert = next_value_tracker_for_super_keys[key_to_insert]++;
                        is_super_key_target = true;
                    } else { // Super key full, fallback to new normal key
                        key_to_insert = make_rflowey_key("norm_add_", op_count);
                        value_to_insert = rng() % 1000;
                    }
                } else if (insert_target_roll < 60 && !set_of_normal_keys.empty()) { // 40% target existing normal
                    auto it = set_of_normal_keys.begin();
                    std::advance(it, rng() % set_of_normal_keys.size());
                    key_to_insert = *it;
                    value_to_insert = rng() % 1000; // Can be duplicate
                } else { // 40% target new normal key
                    key_to_insert = make_rflowey_key("norm_add_", op_count);
                    value_to_insert = rng() % 1000;
                }

                // Actual insert if conditions met (e.g. not exceeding max values for normal keys)
                if (!is_super_key_target && reference_map.count(key_to_insert) && reference_map[key_to_insert].size() >= MAX_VALUES_PER_NORMAL_KEY) {
                    // Skip if normal key is full
                } else {
                    // std::cout << "Op " << op_count << ": INSERT " << key_to_insert.c_str() << " -> " << value_to_insert << std::endl;
                    bpt.insert(key_to_insert, value_to_insert);
                    reference_map[key_to_insert].push_back(value_to_insert);
                    if (!is_super_key_target && !reference_map.count(key_to_insert)) { // Check if it was a new normal key
                         set_of_normal_keys.insert(key_to_insert);
                    } else if (!is_super_key_target && set_of_normal_keys.find(key_to_insert) == set_of_normal_keys.end()){
                         set_of_normal_keys.insert(key_to_insert); // If key was new
                    }
                }

            } else if (op_type_roll < 70) { // 30% Erase
                RFlowey::string<64> key_to_erase;
                bool key_chosen_for_erase = false;

                int erase_target_roll = rng() % 100;
                if (erase_target_roll < 40 && !list_of_super_duped_keys.empty()) { // 40% target super key
                    key_to_erase = list_of_super_duped_keys[rng() % list_of_super_duped_keys.size()];
                    if (reference_map.count(key_to_erase) && !reference_map[key_to_erase].empty()) {
                        key_chosen_for_erase = true;
                    }
                }

                if (!key_chosen_for_erase && !set_of_normal_keys.empty()) { // Fallback or 60% target normal key
                    auto it = set_of_normal_keys.begin();
                    std::advance(it, rng() % set_of_normal_keys.size());
                    key_to_erase = *it;
                     if (reference_map.count(key_to_erase) && !reference_map[key_to_erase].empty()) {
                        key_chosen_for_erase = true;
                    }
                }

                if (key_chosen_for_erase) {
                    std::vector<int>& values_in_ref = reference_map[key_to_erase];
                    int val_idx_to_erase = rng() % values_in_ref.size();
                    int value_to_erase = values_in_ref[val_idx_to_erase];

                    // std::cout << "Op " << op_count << ": ERASE " << key_to_erase.c_str() << " -> " << value_to_erase << std::endl;
                    bool erased = bpt.erase(key_to_erase, value_to_erase);
                    assert(erased && "Erase operation failed for an existing key-value pair.");

                    values_in_ref.erase(values_in_ref.begin() + val_idx_to_erase);
                    if (values_in_ref.empty()) {
                        reference_map.erase(key_to_erase);
                        // If it was a normal key, remove from set_of_normal_keys
                        // Super keys remain in list_of_super_duped_keys even if temporarily empty,
                        // as 'next_value_tracker' still holds info for them.
                        if (set_of_normal_keys.count(key_to_erase)) {
                            set_of_normal_keys.erase(key_to_erase);
                        }
                    }
                }
            } else { // 30% Find
                RFlowey::string<64> key_to_find;
                int find_target_roll = rng() % 100;

                if (find_target_roll < 30 && !list_of_super_duped_keys.empty()) { // 30% find super key
                    key_to_find = list_of_super_duped_keys[rng() % list_of_super_duped_keys.size()];
                } else if (find_target_roll < 80 && !set_of_normal_keys.empty()) { // 50% find existing normal key
                    auto it = set_of_normal_keys.begin();
                    std::advance(it, rng() % set_of_normal_keys.size());
                    key_to_find = *it;
                } else { // 20% find non-existent key
                    key_to_find = make_rflowey_key("non_exist_", op_count);
                }

                // std::cout << "Op " << op_count << ": FIND " << key_to_find.c_str() << std::endl;
                sjtu::vector<int> found_vals_sjtu = bpt.find(key_to_find);
                std::vector<int> found_vals_std;
                for(size_t k=0; k<found_vals_sjtu.size(); ++k) found_vals_std.push_back(found_vals_sjtu[k]);
                std::sort(found_vals_std.begin(), found_vals_std.end());

                std::vector<int> expected_vals_std;
                if (reference_map.count(key_to_find)) {
                    expected_vals_std = reference_map[key_to_find];
                }
                std::sort(expected_vals_std.begin(), expected_vals_std.end());

                if (found_vals_std != expected_vals_std) {
                    std::cerr << "FIND MISMATCH for key: " << key_to_find.c_str() << " on op_count " << op_count << std::endl;
                    std::cerr << "  Expected (" << expected_vals_std.size() << " values): [";
                    for (size_t k = 0; k < expected_vals_std.size(); ++k) {
                        std::cerr << expected_vals_std[k] << (k == expected_vals_std.size() - 1 ? "" : ", ");
                    }
                    std::cerr << "]" << std::endl;

                    std::cerr << "  Found    (" << found_vals_std.size() << " values): [";
                    for (size_t k = 0; k < found_vals_std.size(); ++k) {
                        std::cerr << found_vals_std[k] << (k == found_vals_std.size() - 1 ? "" : ", ");
                    }
                    std::cerr << "]" << std::endl;
                }
                assert(found_vals_std == expected_vals_std && "Find operation mismatch during mixed ops.");
            }
            bpt.print_tree_structure();

            if (VERIFICATION_INTERVAL > 0 && op_count > 0 && (op_count % VERIFICATION_INTERVAL == 0)) {
                verify_bpt_content(bpt, reference_map, "After " + std::to_string(op_count) + " mixed operations");
            }
        }
        verify_bpt_content(bpt, reference_map, "After all mixed operations");
        // bpt.print_tree_structure(); // Optional: print final tree structure
    } // BPT destructor runs here, saves data to db_filename

    // --- Phase 3: Persistence Check ---
    std::cout << "--- Super/Comp Test: Final State Persistence Check ---" << std::endl;
    std::map<RFlowey::string<64>, std::vector<int>> final_ref_map = reference_map; // Copy for check
    {
        RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt_reloaded(db_filename);
        verify_bpt_content(bpt_reloaded, final_ref_map, "Reloaded data after Super/Comp test");
    }

    std::cout << "====== BPT Super-Duped Keys & Comprehensive Mixed Test (Small SIZEMAX) Passed ======" << std::endl;
}

int main() {
    //freopen("test.log","w",stdout);

    const std::string base_db_filename = "bpt_small";

    test_bpt_insert_find_split_small(base_db_filename + "_core_persistence.dat");
    test_bpt_persistence_small(base_db_filename + "_core_persistence.dat");

    // Test 2: Multiple values per key
    test_bpt_multiple_values_per_key(base_db_filename);
    // To test persistence of multi-value, one could add a specific persistence test for its file.

    // Test 3: Pressure test
    test_bpt_pressure(base_db_filename); // Uses _pressure_large.dat


    test_bpt_many_values_single_key_large(base_db_filename);
    test_bpt_erase_small(base_db_filename + "_erase_persistence.dat");
    test_bpt_big_erase(base_db_filename);

    test_bpt_erase_insert_mixed_verbose(base_db_filename);

    // Test 5: Comprehensive test (manages its own file and map)
    test_bpt_super_duped_and_comprehensive_mixed(base_db_filename);
    test_bpt_comprehensive_small(base_db_filename);


    std::cout << "\nAll BPT tests completed successfully." << std::endl;


    // Final cleanup after all tests pass
    std::cout << "--- Main: Cleaning up database files ---" << std::endl;
    std::remove((base_db_filename + "_core_persistence.dat").c_str());
    std::remove((base_db_filename + "_super_duped_comp.dat").c_str());
    std::remove((base_db_filename + "_multi_value.dat").c_str());
    std::remove((base_db_filename + "_many_values_large.dat").c_str());
    std::remove((base_db_filename + "_pressure_large.dat").c_str());
    std::remove((base_db_filename + "_erase_persistence.dat").c_str());
    std::remove((base_db_filename + "_comprehensive.dat").c_str());
    std::remove((base_db_filename + "_big_erase.dat").c_str());
    std::remove((base_db_filename + "_mixed_ops.dat").c_str());
    return 0;
}