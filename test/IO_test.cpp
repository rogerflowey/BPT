#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cstring> // For strcmp
#include <cstdio>  // For remove()


// --- Include your headers ---
// Note: Adjust paths if necessary
#include "src/disk/IO_utils.h"
#include "src/disk/IO_manager.h"
#include "src/disk/serialize.h"
#include "src/common.h"
// --------------------------

// --- Test Data Structure ---
// Must satisfy the PageAble concept
struct TestData {
    int id;
    double value;
    char name[32];
    bool is_active;

    // Default constructor
    TestData() : id(0), value(0.0), is_active(false) {
        name[0] = '\0';
    }

    // Parameterized constructor
    TestData(int i, double v, const char* n, bool active) : id(i), value(v), is_active(active) {
        std::strncpy(name, n, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0'; // Ensure null termination
    }

    // Comparison operator for verification
    bool operator==(const TestData& other) const {
        return id == other.id &&
               value == other.value && // Be careful with float comparison in real apps
               std::strcmp(name, other.name) == 0 &&
               is_active == other.is_active;
    }

     bool operator!=(const TestData& other) const {
        return !(*this == other);
    }
};

// --- Ensure TestData satisfies PageAble ---
// (Compiler will check this, but good for clarity)
static_assert(RFlowey::PageAble<TestData>, "TestData must be PageAble");
// -----------------------------------------


// --- Test Function for a given IOManager ---
void run_manager_tests(RFlowey::IOManager* manager, const std::string& manager_type) {
    std::cout << "--- Testing " << manager_type << " ---" << std::endl;

    // 1. Allocate a new page using the helper
    RFlowey::PagePtr<TestData> page_ptr = RFlowey::allocate<TestData>(manager);
    RFlowey::page_id_t pid = page_ptr.page_id();
    std::cout << "Allocated page ID: " << pid << std::endl;
    assert(pid > 0); // Assuming page IDs start from 1 or similar

    // 2. Create initial data and write it using make_ref
    TestData initial_data(1, 3.14, "Initial Object", true);
    std::cout << "Creating initial data..." << std::endl;
    { // Scope for PageRef to trigger write on destruction
        RFlowey::PageRef<TestData> write_ref = page_ptr.make_ref(
            initial_data.id,
            initial_data.value,
            initial_data.name,
            initial_data.is_active
        );
        // Modify slightly to test dirty flag
        write_ref->id = 10;
        initial_data.id = 10; // Keep initial_data in sync for comparison
        std::cout << "Initial data created (ID modified to " << write_ref->id << "). Ref going out of scope (write expected)." << std::endl;
    } // write_ref destructor runs here, serializes, calls manager->WritePage

    // 3. Read the data back using get_ref
    std::cout << "Reading data back..." << std::endl;
    TestData read_data1;
    { // Scope for PageRef
        RFlowey::PageRef<TestData> read_ref1 = page_ptr.get_ref();
        std::cout << "Read data ID: " << read_ref1->id << std::endl;
        read_data1 = *read_ref1; // Copy the data out
    } // read_ref1 destructor runs (should not write if not modified)

    std::cout << "Verifying initial read..." << std::endl;
    assert(read_data1 == initial_data);
    std::cout << "Initial read verification PASSED." << std::endl;

    // 4. Modify the data using get_ref and let PageRef write it back
    TestData modified_data(20, 2.71, "Modified Object", false);
    std::cout << "Modifying data..." << std::endl;
    { // Scope for PageRef
        RFlowey::PageRef<TestData> modify_ref = page_ptr.get_ref();
        modify_ref->id = modified_data.id;
        modify_ref->value = modified_data.value;
        std::strncpy(modify_ref->name, modified_data.name, sizeof(modify_ref->name) -1);
        modify_ref->name[sizeof(modify_ref->name) - 1] = '\0';
        modify_ref->is_active = modified_data.is_active;
        // modify_ref is now dirty
        std::cout << "Data modified. Ref going out of scope (write expected)." << std::endl;
    } // modify_ref destructor runs here, serializes, calls manager->WritePage

    // 5. Read the modified data back
    std::cout << "Reading modified data back..." << std::endl;
    TestData read_data2;
     { // Scope for PageRef
        RFlowey::PageRef<TestData> read_ref2 = page_ptr.get_ref();
        std::cout << "Read modified data ID: " << read_ref2->id << std::endl;
        read_data2 = *read_ref2; // Copy the data out
    }

    std::cout << "Verifying modified read..." << std::endl;
    assert(read_data2 == modified_data);
    assert(read_data2 != initial_data); // Ensure it actually changed
    std::cout << "Modified read verification PASSED." << std::endl;

    // --- NEW TEST SECTION ---
    // 5.5 Test make_ref without subsequent modification
    std::cout << "Testing make_ref without subsequent modification..." << std::endl;
    RFlowey::PagePtr<TestData> page_ptr_make_ref_only = RFlowey::allocate<TestData>(manager);
    RFlowey::page_id_t pid_make_ref_only = page_ptr_make_ref_only.page_id();
    std::cout << "Allocated page ID for make_ref_only test: " << pid_make_ref_only << std::endl;
    assert(pid_make_ref_only > 0 && pid_make_ref_only != pid); // Ensure new page

    TestData data_for_make_ref(77, 7.77, "MakeRef Only Data", false);
    std::cout << "Creating data with make_ref (no modification)..." << std::endl;
    {
        RFlowey::PageRef<TestData> made_ref = page_ptr_make_ref_only.make_ref(
            data_for_make_ref.id,
            data_for_make_ref.value,
            data_for_make_ref.name,
            data_for_make_ref.is_active
        );
        // NO MODIFICATIONS to made_ref->...
        // The data should be constructed in the page, and the page marked dirty by make_ref.
        std::cout << "Data created via make_ref (ID: " << made_ref->id << ", Name: '" << made_ref->name
                  << "'). Ref going out of scope (write expected if make_ref sets dirty)." << std::endl;
        assert(made_ref->id == data_for_make_ref.id); // Quick check data is there before destruction
        assert(std::strcmp(made_ref->name, data_for_make_ref.name) == 0);
    } // made_ref destructor runs. If make_ref correctly marks page as dirty, data will be written.

    std::cout << "Reading data back (after make_ref_only)..." << std::endl;
    TestData read_data_make_ref_only;
    {
        RFlowey::PageRef<TestData> read_ref_make_ref_only = page_ptr_make_ref_only.get_ref();
        std::cout << "Read data ID (make_ref_only): " << read_ref_make_ref_only->id << std::endl;
        read_data_make_ref_only = *read_ref_make_ref_only;
    }
    std::cout << "Verifying make_ref_only data..." << std::endl;
    assert(read_data_make_ref_only == data_for_make_ref);
    std::cout << "make_ref without modification verification PASSED." << std::endl;
    // --- END OF NEW TEST SECTION ---


    // 6. Test Move Semantics
    std::cout << "Testing move semantics..." << std::endl;
    {
        RFlowey::PageRef<TestData> ref_move1 = page_ptr.get_ref();
        ref_move1->id = 99; // Make it dirty

        RFlowey::PageRef<TestData> ref_move2 = std::move(ref_move1);
        // ref_move1 should be invalid now (though we can't easily check is_valid)
        assert(ref_move2->id == 99); // Check data moved
        std::cout << "Move construction PASSED." << std::endl;

        RFlowey::PageRef<TestData> ref_move3 = page_ptr.make_ref(101, 1.1, "Move Assign Test", true);
        ref_move3 = std::move(ref_move2); // Move assignment
        // ref_move2 should be invalid now
        // ref_move3 should have the data from ref_move1/ref_move2 (id=99)
        // The original object in ref_move3 (id=101) should have been dropped without writing.
        assert(ref_move3->id == 99);
        std::cout << "Move assignment PASSED." << std::endl;

        // Let ref_move3 go out of scope - it should write id=99 back
    }

    // 7. Verify data after move tests
    std::cout << "Verifying data after move tests..." << std::endl;
     { // Scope for PageRef
        RFlowey::PageRef<TestData> read_ref3 = page_ptr.get_ref();
        assert(read_ref3->id == 99); // Should have been written by ref_move3's destructor
        assert(read_ref3->value == modified_data.value); // Other fields shouldn't change unless modified
        assert(std::strcmp(read_ref3->name, modified_data.name) == 0);
        assert(read_ref3->is_active == modified_data.is_active);
    }
    std::cout << "Move test verification PASSED." << std::endl;


    std::cout << "--- " << manager_type << " Tests PASSED ---" << std::endl << std::endl;
}

// --- Main Function ---
int main() {
    std::cout << "Starting IO Utils Tests..." << std::endl;

    // Test Memory Manager
    {
        RFlowey::MemoryManager mem_manager;
        run_manager_tests(&mem_manager, "MemoryManager");
    }

    // Test Disk Manager
    {
        std::string filename = "test_disk_manager.db";
        // Clean up previous test file if it exists
        std::remove(filename.c_str());

        RFlowey::SimpleDiskManager disk_manager(filename);
        run_manager_tests(&disk_manager, "SimpleDiskManager");

        // Optional: Clean up test file after run
        // disk_manager's destructor should close the file
         std::cout << "Disk manager test finished. Deleting " << filename << std::endl;
         std::remove(filename.c_str());
    }


    std::cout << "All IO Utils Tests Completed Successfully!" << std::endl;
    return 0;
}