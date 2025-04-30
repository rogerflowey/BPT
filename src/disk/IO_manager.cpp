#include "IO_manager.h"


namespace RFlowey {
  //--------Memory version-------
  page_id_t MemoryManager::NewPage() override {
    return ++next_page_;
  }
  void MemoryManager::DeletePage(page_id_t page_id) override {
    return;
  }
  std::shared_ptr<Page> MemoryManager::ReadPage(page_id_t page_id) override {
    auto temp = std::make_shared<Page>(this,page_id);
    std::memcpy(temp->get_data(),memory_+page_id*PAGESIZE,PAGESIZE);
    return temp;
  }
  void MemoryManager::WritePage(Page &page, page_id_t page_id) override {
    std::memcpy(memory_+page_id*PAGESIZE,page.get_data(),PAGESIZE);
  };



  //--------Disk version-------
  SimpleDiskManager::SimpleDiskManager(std::string& file_name) {
    open(file_,file_name);
  };

  page_id_t SimpleDiskManager::NewPage() override {
    return ++next_page_;
  }
  void DeletePage(page_id_t page_id) override {
  }
  std::shared_ptr<Page> SimpleDiskManager::ReadPage(page_id_t page_id) override {
    auto temp = std::make_shared<Page>(this,page_id);
    file_.seekg(page_id*PAGESIZE);
    file_.read(temp->get_data(),PAGESIZE);
    return temp;
  }
  void SimpleDiskManager::WritePage(Page& page,page_id_t page_id) override {
    file_.seekp(page_id*PAGESIZE);
    file_.write(page.get_data(),PAGESIZE);
  }
}