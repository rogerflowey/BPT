#ifndef UTILS_H
#define UTILS_H

#include <utility>

//from STLite
template<class T1, class T2>
class pair {
public:
  T1 first;   // first    element
  T2 second;  // second   element

  constexpr pair() = default;
  constexpr pair(const pair &other) = default;
  constexpr pair(pair &&other) = default;

  template<class U1 = T1, class U2 = T2>
  constexpr pair(U1 &&x, U2 &&y)
      : first(std::forward<U1>(x)), second(std::forward<U2>(y)) {}

  template<class U1, class U2>
  constexpr pair(const pair<U1, U2> &other)
      : first(other.first), second(other.second) {}

  template<class U1, class U2>
  constexpr pair(pair<U1, U2> &&other)
      : first(std::move(other.first))
      ,second(std::move(other.second)) {}
};

template<class T1, class T2>
pair(T1, T2) -> pair<T1, T2>;

constexpr unsigned long long hash(const std::string &s) {
    unsigned long long hash = 0;
    for (char c: s) {
        hash += c;
        hash = (hash * 37);
    }
    if (hash == 0) {
        //0被用作空值代表已删除
        hash = 114514;
    }
    return hash;
}

#endif //UTILS_H
