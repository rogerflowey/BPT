#include "IO_manager.h"
#include "IO_utils.h"


namespace RFlowey {

  IOManager::~IOManager() = default;

  //--------Memory version-------
  MemoryManager::MemoryManager(const std::string &file_name) {
    return;
  }

  page_id_t MemoryManager::NewPage() {
    return ++next_page_;
  }
  void MemoryManager::DeletePage(page_id_t page_id) {
    return;
  }
  std::shared_ptr<Page> MemoryManager::ReadPage(page_id_t page_id) {
    auto temp = std::make_shared<Page>(this,page_id);
    std::memcpy(temp->get_data(),memory_+page_id*PAGESIZE,PAGESIZE);
    return temp;
  }
  void MemoryManager::WritePage(Page &page, page_id_t page_id) {
    std::memcpy(memory_+page_id*PAGESIZE,page.get_data(),PAGESIZE);
  };



  //--------Disk version-------
  SimpleDiskManager::SimpleDiskManager(const std::string& file_name) {
    is_new = open(file_,file_name);
  };

  page_id_t SimpleDiskManager::NewPage() {
    return ++next_page_;
  }
  void SimpleDiskManager::DeletePage(page_id_t page_id) {
    return;
  }
  std::shared_ptr<Page> SimpleDiskManager::ReadPage(page_id_t page_id) {
    if (!file_.is_open()) {
        throw std::runtime_error("SimpleDiskManager: File is not open for reading.");
    }
    if (page_id <= 0) { // Page 0 is reserved/invalid
        throw std::out_of_range("SimpleDiskManager: Invalid page_id for ReadPage (must be > 0): " + std::to_string(page_id));
    }

    auto temp = std::make_shared<Page>(this, page_id);
    char* page_data = temp->get_data();
    std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGESIZE;

    file_.seekg(offset);
    if (file_.fail()) {
        file_.clear();
        throw std::runtime_error("SimpleDiskManager: Failed to seek to page " + std::to_string(page_id) +
                                 " (offset " + std::to_string(offset) + ") for reading. File might be too small or I/O error.");
    }

    file_.read(page_data, PAGESIZE);
    if (file_.gcount() != PAGESIZE) {
        bool eof_reached = file_.eof();
        file_.clear();
        throw std::runtime_error("SimpleDiskManager: Failed to read full page " + std::to_string(page_id) +
                                 ". Read " + std::to_string(file_.gcount()) + " bytes instead of " +
                                 std::to_string(PAGESIZE) + (eof_reached ? ". EOF reached." : ". I/O error."));
    }
    // If gcount() == PAGESIZE, fail() should ideally not be true unless stream state is corrupted.
    // Can add `if (file_.fail())` here for extra paranoia.
    return temp;
  }

  void SimpleDiskManager::WritePage(Page& page, page_id_t page_id) {
    if (!file_.is_open()) {
        throw std::runtime_error("SimpleDiskManager: File is not open for writing.");
    }
    if (page_id <= 0) { // Page 0 is reserved/invalid
        throw std::out_of_range("SimpleDiskManager: Invalid page_id for WritePage (must be > 0): " + std::to_string(page_id));
    }

    char* page_data = page.get_data();
    std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGESIZE;

    file_.seekp(offset);
    if (file_.fail()) {
        file_.clear();
        throw std::runtime_error("SimpleDiskManager: Failed to seek to page " + std::to_string(page_id) +
                                 " (offset " + std::to_string(offset) + ") for writing.");
    }

    file_.write(page_data, PAGESIZE);
    if (file_.fail()) {
        file_.clear();
        throw std::runtime_error("SimpleDiskManager: Failed to write page " + std::to_string(page_id));
    }
  }
}