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
    const std::string& test_stage_msg) {

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

// NEW COMPREHENSIVE TEST
void test_bpt_comprehensive_small(const std::string& db_filename_prefix) {
    const std::string db_filename = db_filename_prefix + "_comprehensive.dat";
    std::cout << "\n====== Starting BPT Comprehensive Test (Small SIZEMAX) ======" << std::endl;
    std::remove(db_filename.c_str());

    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
    std::map<RFlowey::string<64>, std::vector<int>> reference_map;
    std::set<RFlowey::string<64>> existing_keys_set; // To quickly pick keys for erase/find

    std::mt19937 rng(114); // Random number generator

    const int num_initial_inserts = 150; // Enough to cause some splits
    const int num_operations = 1000;    // Mix of inserts, erases, finds
    const int max_val_per_key = 10;

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
        bpt.print_tree_structure();

        if (i % (num_operations / 10) == 0 && i > 0) { // Verify periodically
            verify_bpt_content(bpt, reference_map, "After " + std::to_string(i) + " mixed operations");
        }
    }
    verify_bpt_content(bpt, reference_map, "After all mixed operations");

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


int main() {
    freopen("test.log","w",stdout);

    const std::string base_db_filename = "bpt_small";

    test_bpt_insert_find_split_small(base_db_filename + "_core_persistence.dat");
    test_bpt_persistence_small(base_db_filename + "_core_persistence.dat");

    // Test 2: Multiple values per key
    test_bpt_multiple_values_per_key(base_db_filename);
    // To test persistence of multi-value, one could add a specific persistence test for its file.

    // Test 3: Pressure test
    //test_bpt_pressure(base_db_filename); // Uses _pressure_large.dat

    // Test 4: Erase test (uses global_reference_map_for_persistence)
    // This will clear global_reference_map_for_persistence.
    test_bpt_erase_small(base_db_filename + "_erase_persistence.dat");
    // Optionally, add a persistence check for the erase test's output file:
    // test_bpt_persistence_small(base_db_filename + "_erase_persistence.dat"); // Reuses persistence test logic
    // For this to work cleanly, test_bpt_persistence_small needs to be robust to an empty map if erase clears everything.
    // Or, a dedicated persistence for erase test. For now, erase test saves, manual inspection or next run might use it.

    // Test 5: Comprehensive test (manages its own file and map)
    test_bpt_comprehensive_small(base_db_filename);


    std::cout << "\nAll BPT tests completed successfully." << std::endl;


    // Final cleanup after all tests pass
    std::cout << "--- Main: Cleaning up database files ---" << std::endl;
    std::remove((base_db_filename + "_core_persistence.dat").c_str());
    std::remove((base_db_filename + "_multi_value.dat").c_str());
    std::remove((base_db_filename + "_pressure_large.dat").c_str()); // Corrected filename
    std::remove((base_db_filename + "_erase_persistence.dat").c_str());
    std::remove((base_db_filename + "_comprehensive.dat").c_str());
    return 0;
}