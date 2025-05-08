#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cassert>
#include <map>
#include <set>

// Define BPT_SMALL_SIZE to use smaller SIZEMAX for easier split testing
#define BPT_SMALL_SIZE
// Define BPT_TEST before including BPT.h to enable internal BPT assertions
#define BPT_TEST

#include "src/BPT.h" // Adjust path as needed
#include "src/utils/utils.h" // For RFlowey::string and RFlowey::hash

// --- Custom Hashers (copy from your existing test file) ---
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

// --- Helper Functions (copy from your existing test file) ---
RFlowey::string<64> make_rflowey_key(const char* prefix, int id) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s%d", prefix, id);
    return RFlowey::string<64>(buffer);
}

// Helper to verify BPT content (copy from your existing test file)
void verify_bpt_content(
    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher>& bpt,
    const std::map<RFlowey::string<64>, std::vector<int>>& reference_map,
    const std::string& test_stage_msg) {
    std::cout << "--- Verifying BPT content: " << test_stage_msg << " ---" << std::endl;
    bool verification_passed = true;

    // Check all keys in reference_map exist in BPT with correct values
    for (const auto& pair : reference_map) {
        const RFlowey::string<64>& key = pair.first;
        std::vector<int> expected_values_std_vec = pair.second;
        std::sort(expected_values_std_vec.begin(), expected_values_std_vec.end());

        sjtu::vector<int> found_values_sjtu_vec = bpt.find(key);
        std::vector<int> found_values_std_vec_sorted;
        for(size_t i = 0; i < found_values_sjtu_vec.size(); ++i) {
            found_values_std_vec_sorted.push_back(found_values_sjtu_vec[i]);
        }
        std::sort(found_values_std_vec_sorted.begin(), found_values_std_vec_sorted.end());

        if (found_values_std_vec_sorted.size() != expected_values_std_vec.size()) {
            std::cerr << "ERROR: Value count mismatch for key: " << key.c_str()
                      << " at stage: " << test_stage_msg
                      << ". Expected: " << expected_values_std_vec.size()
                      << ", Found: " << found_values_std_vec_sorted.size() << std::endl;
            verification_passed = false;
        }
        if (found_values_std_vec_sorted != expected_values_std_vec) {
             std::cerr << "ERROR: Value mismatch for key: " << key.c_str()
                      << " at stage: " << test_stage_msg << std::endl;
            std::cerr << "  Expected: "; for(int v : expected_values_std_vec) std::cerr << v << " "; std::cerr << std::endl;
            std::cerr << "  Found:    "; for(int v : found_values_std_vec_sorted) std::cerr << v << " "; std::cerr << std::endl;
            verification_passed = false;
        }
    }

    // Optional: Check if BPT contains keys not in reference_map (more complex without iterator)
    // For now, we rely on the fact that if reference map is empty, BPT should also be empty of user data.
    if (reference_map.empty()) {
        // A simple check: try to find a known previously existing key
        // This is not exhaustive.
        RFlowey::string<64> a_known_key_if_any("key_0"); // Example
        sjtu::vector<int> vals = bpt.find(a_known_key_if_any);
        if (!vals.empty()) {
            // This check is only meaningful if "key_0" was indeed inserted and then erased.
            // And if the reference map is truly empty (meaning all user keys were erased).
            // std::cerr << "ERROR: Reference map is empty, but BPT returned values for a presumed erased key: "
            //           << a_known_key_if_any.c_str() << std::endl;
            // verification_passed = false;
        }
    }


    assert(verification_passed && ("Verification failed at stage: " + test_stage_msg).c_str());
    std::cout << "--- Verification successful: " << test_stage_msg << " ---" << std::endl;
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
int main() {
    const std::string base_db_filename = "bpt_small_non_random";

    // Run Test 1: Big Erase
    //test_bpt_big_erase(base_db_filename);

    // Run Test 2: Erase and Insert Mixed
    test_bpt_erase_insert_mixed_verbose(base_db_filename);

    std::cout << "\nAll non-random BPT tests completed successfully." << std::endl;

    // Final cleanup
    std::cout << "--- Main: Cleaning up database files ---" << std::endl;
    std::remove((base_db_filename + "_big_erase.dat").c_str());
    std::remove((base_db_filename + "_mixed_ops.dat").c_str());
    return 0;
}