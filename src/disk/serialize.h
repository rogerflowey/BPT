#pragma once

#include <cstring>
#include <memory>
#include "IO_utils.h"
#include "src/common.h"

namespace RFlowey {
  template <typename T>
  concept PageAble = std::is_trivially_copyable_v<T> && requires
  {
    sizeof(T)<PAGESIZE;
  };
  template<typename T>
    requires PageAble<T>
  void Serialize(void* _dest,const T& value) {
    std::memcpy(_dest,&value,sizeof(T));
  }
  template<typename T>
    requires PageAble<T>
  std::unique_ptr<T> Deserialize(void* _dest) {
    return std::make_shared<T>(*reinterpret_cast<T*>(_dest));
  }

}
