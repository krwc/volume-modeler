#ifndef VM_UTILS_PERSISTENCE_H
#define VM_UTILS_PERSISTENCE_H
#include <array>
#include <cstring>
#include <iostream>

namespace vm {

/**
 * C++ for some reason does not support any kind of binary streams,
 * yet it is kind of nice to use operator<< and operator>> instead
 * of doing clumsy stream.write() each time. So, templates for the
 * rescue and even MORE overloaded operators!
 *
 * NOTE: there is no support for different byte orders.
 */
template <typename T>
inline std::ostream &operator<= (std::ostream &out, const T &data) {
    return out.write(reinterpret_cast<const char *>(&data), sizeof(data));
}

template <typename T>
inline std::istream &operator>= (std::istream &in, T &data) {
    std::array<char, sizeof(T)> read_data;
    in.read(&read_data[0], sizeof(T));
    memcpy(reinterpret_cast<char *>(&data), &read_data[0], sizeof(T));
    return in;
}

} // namespace vm

#endif
