#pragma once

#include <fstream>
#include <memory>
#include <cstring>
#include "src/common.h"
#include "IO_utils.h"



namespace RFlowey {
  class IOManager {
  public:
    virtual ~IOManager() = default;

    virtual page_id_t NewPage();
    virtual void DeletePage(page_id_t page_id);
    virtual std::shared_ptr<Page> ReadPage(page_id_t page_id);
    virtual void WritePage(Page& page,page_id_t page_id);
    
  };

  class MemoryManager:public IOManager {
    char memory_[4*1024*1024]={};
    page_id_t next_page_=0;

  public:

    page_id_t NewPage() override;
    void DeletePage(page_id_t page_id) override;
    std::shared_ptr<Page> ReadPage(page_id_t page_id) override;
    void WritePage(Page &page, page_id_t page_id) override;

  };

  class SimpleDiskManager:public IOManager {
    std::fstream file_;
    page_id_t next_page_=0;//0 reserved for IOM

  public:
    explicit SimpleDiskManager(std::string& file_name);

    page_id_t NewPage() override;
    void DeletePage(page_id_t page_id) override;
    std::shared_ptr<Page> ReadPage(page_id_t page_id) override;
    void WritePage(Page& page,page_id_t page_id) override;
  };
}