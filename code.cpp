#include <iostream> // For std::cin, std::cout, std::cerr
#include <string>   // For std::string
#include <cassert>

// Include the B+ Tree header
#include "src/BPT.h"

#define BPT_TEST

struct String64Hasher {
  RFlowey::hash_t operator()(const RFlowey::string<64>& s) const {
    return hash(s);
  }
};

struct IntHasher {
  RFlowey::hash_t operator()(const int& v) const {
    return static_cast<RFlowey::hash_t>(static_cast<long long>(v)-std::numeric_limits<int>::min());
  }
};


bool TEST;

int main() {
  TEST = false;
  if (TEST) {
    // Make sure these files exist if TEST is true
    freopen("test.in", "r", stdin);
    freopen("test.out", "w", stdout);
  }

  std::string bpt_data_file = "No2697.dat";
  //std::remove(bpt_data_file.c_str());
  RFlowey::BPT<RFlowey::string<64>, int,String64Hasher,IntHasher> bpt(bpt_data_file);

  int n;
  if (!(std::cin >> n)) {
      std::cerr << "Error reading number of operations." << std::endl;
      return 1;
  }

  for (int i = 0; i < n; ++i) {
    std::string command;
    if (!(std::cin >> command)) {
        std::cerr << "Error reading command." << std::endl;
        break;
    }

    if (command == "insert") {
      std::string index_str;
      int value;
      if (!(std::cin >> index_str >> value)) {
           std::cerr << "Error reading insert arguments." << std::endl;
           break;
      }
      bpt.insert(index_str, value);
    } else if (command == "delete") {
      std::string index_str;
      int value;
       if (!(std::cin >> index_str >> value)) {
           std::cerr << "Error reading delete arguments." << std::endl;
           break;
       }
       bpt.erase(index_str,value);
    } else if (command == "find") {
      std::string index_str;
       if (!(std::cin >> index_str)) {
           std::cerr << "Error reading find argument." << std::endl;
           break;
       }
      auto out = bpt.find(index_str);

      if (out.empty()) {
        std::cout << "null";
      } else {
        bool first = true;
        for (const auto& val : out) {
          if (!first) {
            std::cout << ' ';
          }
          std::cout << val;
          first = false;
        }
      }
      std::cout << '\n';
    } else {
      std::cerr << "Invalid command: " << command << std::endl;
      std::string dummy;
      std::getline(std::cin, dummy);
    }
    //bpt.print_tree_structure();
  }
  return 0;
}