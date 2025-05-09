#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio> // For std::remove
#include <cassert>
#include <map>    // For reference model
#include <vector> // For std::vector

// Define BPT_SMALL_SIZE to use smaller SIZEMAX for more frequent splits/merges with less data
#define BPT_SMALL_SIZE
// Define BPT_TEST before including BPT.h to enable internal BPT assertions
#define BPT_TEST

// Assuming BPT.h and utils.h are in a subdirectory "src"
// Adjust path if necessary
#include "src/BPT.h"
#include "src/utils/utils.h" // For RFlowey::string and RFlowey::hash

// --- Custom Hashers (from your provided code) ---
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

// --- Helper Function to verify BPT content (from your provided code, slightly adapted) ---
// This function can be used for a final check if desired.
void verify_bpt_content(
    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher>& bpt,
    const std::map<RFlowey::string<64>, std::vector<int>>& reference_map,
    const std::string& test_stage_msg) {

    std::cout << "--- Verifying BPT content: " << test_stage_msg << " ---" << std::endl;
    bool all_ok = true;

    // Check all keys in the reference map exist in BPT with correct values
    for (const auto& pair : reference_map) {
        const RFlowey::string<64>& key = pair.first;
        std::vector<int> expected_values_std_vec = pair.second; // Already sorted in our main logic

        sjtu::vector<int> found_values_sjtu_vec = bpt.find(key);

        std::vector<int> found_values_std_vec_sorted;
        for(size_t i = 0; i < found_values_sjtu_vec.size(); ++i) {
            found_values_std_vec_sorted.push_back(found_values_sjtu_vec[i]);
        }
        std::sort(found_values_std_vec_sorted.begin(), found_values_std_vec_sorted.end());
        // expected_values_std_vec is already sorted by the main loop's logic

        if (found_values_std_vec_sorted.size() != expected_values_std_vec.size()) {
            std::cerr << "ERROR: Value count mismatch for key: " << key.c_str()
                      << " at stage: " << test_stage_msg
                      << ". Expected: " << expected_values_std_vec.size()
                      << ", Found: " << found_values_std_vec_sorted.size() << std::endl;
            all_ok = false;
        }
        assert(found_values_std_vec_sorted.size() == expected_values_std_vec.size());


        bool values_match = true;
        if (found_values_std_vec_sorted.size() == expected_values_std_vec.size()) {
            for (size_t i = 0; i < expected_values_std_vec.size(); ++i) {
                if (found_values_std_vec_sorted[i] != expected_values_std_vec[i]) {
                    values_match = false;
                    break;
                }
            }
        } else {
            values_match = false;
        }

        if (!values_match) {
             std::cerr << "ERROR: Value mismatch for key: " << key.c_str()
                       << " at stage: " << test_stage_msg << std::endl;
             std::cerr << "  Expected values (" << expected_values_std_vec.size() << "): ";
             for(int v : expected_values_std_vec) std::cerr << v << " ";
             std::cerr << std::endl;
             std::cerr << "  Found values (" << found_values_std_vec_sorted.size() << "): ";
             for(int v : found_values_std_vec_sorted) std::cerr << v << " ";
             std::cerr << std::endl;
             all_ok = false;
        }
        assert(values_match);
    }

    if (all_ok) {
        std::cout << "--- Verification successful: " << test_stage_msg << " ---" << std::endl;
    } else {
        std::cerr << "--- Verification FAILED: " << test_stage_msg << " ---" << std::endl;
        // exit(1); // Optionally terminate on verification failure
    }
}


int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    const std::string db_filename = "interactive_test_bpt.dat";
    // Clean up any previous database file for a fresh start
    std::remove(db_filename.c_str());

    RFlowey::BPT<RFlowey::string<64>, int, String64Hasher, IntHasher> bpt(db_filename);
    std::map<RFlowey::string<64>, std::vector<int>> reference_map;

    int num_operations;
    std::cin >> num_operations;

    for (int op_count = 0; op_count < num_operations; ++op_count) {
        std::string command;
        std::cin >> command;

        std::cout << "\n=== Operation " << op_count + 1 << "/" << num_operations << ": " << command << " ===" << std::endl;

        if (command == "insert") {
            std::string key_str_std;
            int value;
            std::cin >> key_str_std >> value;
            RFlowey::string<64> key(key_str_std.c_str());
            std::cout << "Params: Key='" << key_str_std << "', Value=" << value << std::endl;

            // Perform on BPT
            bpt.insert(key, value);

            // Perform on reference map
            reference_map[key].push_back(value);
            std::sort(reference_map[key].begin(), reference_map[key].end());
            std::cout << "Action: Inserted (" << key.c_str() << ", " << value << ")" << std::endl;


        } else if (command == "find") {
            std::string key_str_std;
            std::cin >> key_str_std;
            RFlowey::string<64> key(key_str_std.c_str());
            std::cout << "Params: Key='" << key_str_std << "'" << std::endl;

            // Perform on BPT
            sjtu::vector<int> bpt_results_sjtu = bpt.find(key);
            std::vector<int> bpt_results_std;
            for (size_t i = 0; i < bpt_results_sjtu.size(); ++i) {
                bpt_results_std.push_back(bpt_results_sjtu[i]);
            }
            std::sort(bpt_results_std.begin(), bpt_results_std.end());

            // Get expected results from reference map
            std::vector<int> ref_results_std;
            if (reference_map.count(key)) {
                ref_results_std = reference_map[key]; // Already sorted
            }

            // Assert correctness
            if (bpt_results_std != ref_results_std) {
                std::cerr << "FIND MISMATCH for key: " << key.c_str() << std::endl;
                std::cerr << "  BPT found: ";
                for(int v : bpt_results_std) std::cerr << v << " ";
                std::cerr << std::endl;
                std::cerr << "  REF expected: ";
                for(int v : ref_results_std) std::cerr << v << " ";
                std::cerr << std::endl;
            }
            assert(bpt_results_std == ref_results_std && "Find operation mismatch with reference map");

            // Print BPT results as per problem requirement
            std::cout << "Output: ";
            if (bpt_results_std.empty()) {
                std::cout << "Not Found" << std::endl;
            } else {
                for (size_t i = 0; i < bpt_results_std.size(); ++i) {
                    std::cout << bpt_results_std[i] << (i == bpt_results_std.size() - 1 ? "" : " ");
                }
                std::cout << std::endl;
            }

        } else if (command == "delete") {
            std::string key_str_std;
            int value;
            std::cin >> key_str_std >> value;
            RFlowey::string<64> key(key_str_std.c_str());
            std::cout << "Params: Key='" << key_str_std << "', Value=" << value << std::endl;

            // Perform on BPT
            bool bpt_erased = bpt.erase(key, value);

            // Perform on reference map
            bool ref_erased = false;
            if (reference_map.count(key)) {
                auto& vec = reference_map[key];
                auto it = std::find(vec.begin(), vec.end(), value);
                if (it != vec.end()) {
                    vec.erase(it);
                    ref_erased = true;
                    if (vec.empty()) {
                        reference_map.erase(key); // Remove key if no values left
                    }
                }
            }

            // Assert correctness
            if (bpt_erased != ref_erased) {
                 std::cerr << "DELETE MISMATCH for key: " << key.c_str() << " value: " << value << std::endl;
                 std::cerr << "  BPT erased: " << (bpt_erased ? "true" : "false") << std::endl;
                 std::cerr << "  REF erased: " << (ref_erased ? "true" : "false") << std::endl;
            }
            assert(bpt_erased == ref_erased && "Delete operation mismatch with reference map");
            std::cout << "Output: Erase " << (bpt_erased ? "succeeded" : "failed (key/value not found)") << std::endl;


        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            // Skip to next line or handle error
            std::string dummy;
            std::getline(std::cin, dummy); // Consume rest of the line
            // Skip printing tree for unknown command
            continue;
        }

        // Print tree structure after every operation
        std::cout << "--- BPT Structure after operation " << op_count + 1 << " (" << command << ") ---" << std::endl;
        bpt.print_tree_structure(); // Assuming this method exists in BPT class
        std::cout << "----------------------------------------" << std::endl;
    }

    // Optional: Perform a full verification at the end of all operations
    // verify_bpt_content(bpt, reference_map, "After all interactive operations");

    // BPT destructor will save data to db_filename.
    // Clean up the database file after the test if it's temporary.
    // std::remove(db_filename.c_str()); // Uncomment if you want to clean up
    std::cout << "\nAll operations processed." << std::endl;

    return 0;
}