#ifndef UTILS_H
#define UTILS_H

#include <utility>
#include <cstring>
#include <cassert>


namespace RFlowey {
//from STLite
template<class T1, class T2>
class pair {
public:
  T1 first;   // first    element
  T2 second;  // second   element

  constexpr pair() = default;
  constexpr pair(const pair &other) = default;
  constexpr pair(pair &&other) = default;
  constexpr pair& operator=(const pair &other) = default;
  constexpr pair& operator=(pair &&other) = default;

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

template<class T1, class T2>
constexpr bool operator==(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return lhs.first == rhs.first && lhs.second == rhs.second;
}

template<class T1, class T2>
constexpr bool operator!=(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return !(lhs == rhs);
}

template<class T1, class T2>
constexpr bool operator<(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    if (lhs.first < rhs.first) return true;
    if (rhs.first < lhs.first) return false;
    return lhs.second < rhs.second;
}

template<class T1, class T2>
constexpr bool operator<=(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return !(rhs < lhs);
}

template<class T1, class T2>
constexpr bool operator>(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return rhs < lhs;
}

template<class T1, class T2>
constexpr bool operator>=(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
    return !(lhs < rhs);
}


    template<int N>
    struct string {
        char a[N]{};

        string() noexcept {
            std::memset(a, 0, N);
        }

        string(const std::string& s) noexcept {
            assign(s.data(), s.length());
        }
        string(const char* c_str) noexcept {
            if (c_str == nullptr) {
                std::memset(a, 0, N);
            } else {
                size_t len = strnlen(c_str, N);
                assign(c_str, len);
            }
        }

        // Copy constructor (defaulted is fine)
        string(const string& other) noexcept = default;

        // Move constructor (defaulted is fine and implicitly noexcept)
        string(string&& other) noexcept = default;


        // --- Assignment Operators ---

        // Copy assignment operator (defaulted is fine)
        string& operator=(const string& other) noexcept = default;

        // Move assignment operator (defaulted is fine and implicitly noexcept)
        string& operator=(string&& other) noexcept = default;

        // Assignment from std::string
        string& operator=(const std::string& s) noexcept {
            assign(s.data(), s.length());
            return *this;
        }
        string& operator=(const char* c_str) noexcept {
            if (c_str == nullptr) {
                std::memset(a, 0, N); // Treat nullptr as empty string
            } else {
                size_t len = strnlen(c_str, N);
                assign(c_str, len);
            }
            return *this;
        }


        [[nodiscard]] size_t length() const noexcept {
            return strnlen(a, N);
        }
        [[nodiscard]] bool empty() const noexcept {
            return a[0] == '\0';
        }
        static constexpr size_t capacity() noexcept {
            return N;
        }
        [[nodiscard]] std::string get_str() const{
            return std::string(a, length());
        }
        [[nodiscard]] const char* data() const noexcept {
            return a;
        }
        char* data() noexcept {
            return a;
        }

        bool operator==(const string<N>& other) const noexcept {
            return std::strncmp(a, other.a, N) == 0;
        }

        bool operator!=(const string<N>& other) const noexcept {
            return !(*this == other);
        }

        bool operator<(const string<N>& other) const noexcept {
            return std::strncmp(a, other.a, N) < 0;
        }

        bool operator<=(const string<N>& other) const noexcept {
            return std::strncmp(a, other.a, N) <= 0;
        }

        bool operator>(const string<N>& other) const noexcept {
            return std::strncmp(a, other.a, N) > 0;
        }

        bool operator>=(const string<N>& other) const noexcept {
            return std::strncmp(a, other.a, N) >= 0;
        }

    private:
        void assign(const char* data_ptr, size_t len) noexcept {
            size_t bytes_to_copy = std::min(len, static_cast<size_t>(N));
            std::memcpy(a, data_ptr, bytes_to_copy);

            if (bytes_to_copy < N) {
                // Zero-pad the rest of the buffer
                std::memset(a + bytes_to_copy, 0, N - bytes_to_copy);
            }
        }
    };

    template<int N>
    constexpr unsigned long long hash(const string<N> &s) {
        unsigned long long hash = 0;

        for (int i=0;i<N;++i) {
            hash += s.a[i];
            hash = (hash * 37);
        }
        if (hash == 0) {
            //0被用作空值代表已删除
            hash = 114514;
        }
        return hash;
    }
}
#endif //UTILS_H
