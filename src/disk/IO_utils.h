#ifndef IO_UTILS_H
#define IO_UTILS_H

#include <fstream>
#include <memory>
#include <src/disk/serialize.h>

#include "IO_manager.h"
#include "src/common.h"


namespace RFlowey {
  /**
   * A wrapper of a Byte Page(Temporary solution for no Buffer Pool)
   * Ensure the life span covers the value of it
   */
  class Page {
    char data_[4096]={};
    page_id_t page_id_;
    IOManager* manager_;
  public:
    Page() = delete;
    Page(IOManager* manager,page_id_t page_id);
    ~Page();
    char* get_data();
    void flush();
  };
  bool open(std::fstream& file,std::string& filename);


  //不要问为啥不分到.cpp,问就是模板类
  template<typename T>
  //代表“解引用”后的内存中的对象，析构时会同时析构并重新序列化管理的对象
  class PageRef {
    std::shared_ptr<Page> page_;
    std::unique_ptr<T> t_ptr_;
    bool is_dirty = false;
    bool is_valid = true;
  public:
    PageRef(std::shared_ptr<Page> page,std::unique_ptr<T> t_ptr):page_(std::move(page)),t_ptr_(std::move(t_ptr)){};
    PageRef(PageRef&& ref) noexcept :page_(std::move(ref.page_)),t_ptr_(std::move(ref.t_ptr_)) {
      ref.is_valid = false;
    };

    PageRef& operator=(PageRef&& ref)  noexcept {
      if(this==&ref) {
        return *this;
      }
      Drop();
      ref.is_valid = false;
      page_ = std::move(ref.page_);
      t_ptr_ = std::move(ref.t_ptr_);
      return *this;
    }
    ~PageRef() {
      Drop();
    }
    void Drop() {
      if(!is_valid) {
        return;
      }
      if(is_dirty) {
        Serialize(page_->get_data(),*t_ptr_);
      }
      is_valid = false;

    }
    T* operator->() {
      is_dirty = true;
      return t_ptr_.get();
    }
    const T* operator->() const{
      return t_ptr_.get();
    }
    T& operator*() {
      is_dirty = true;
      return *t_ptr_;
    }
    const T& operator*() const{
      return *t_ptr_;
    }
  };
  template<typename T>
  class PagePtr {
    IOManager* manager_;
    page_id_t page_id_;
  public:


    explicit PagePtr(page_id_t page_id,IOManager* manager):page_id_(page_id),manager_(manager){}

    [[nodiscard]] page_id_t page_id() const {
      return page_id_;
    }

    [[nodiscard]] PageRef<T> get_ref() const {
      std::shared_ptr<Page> page = manager_->ReadPage(page_id_);
      return PageRef{page,Deserialize<T>(page->get_data())};
    }

    template<typename... Args>
    PageRef<T> make_ref(Args args) {
      return {Page(manager_,page_id_),std::make_unique<T>(args)};
    }
    PageRef<T> make_ref(std::unique_ptr<T> ptr) {
      return {Page(manager_,page_id_),ptr};
    }
  };

  template<typename T>
  PagePtr<T> allocate(IOManager* manager) {
    return {manager,manager->NewPage()};
  }

  /**
   * @return true if new
   */
  inline bool open(std::fstream& file,std::string& filename) {
    file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    if(!file){
      file.clear();
      file.open(filename, std::ios::out | std::ios::binary);
      file.close();
      file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
      return true;
    }
    return false;
  }
}

#endif //IO_UTILS_H
