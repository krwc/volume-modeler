#ifndef VM_UTILS_LOG_H
#define VM_UTILS_LOG_H
#include <sstream>
#include <iostream>

namespace vm {
namespace log {

enum class Severity {
    trace,
    debug,
    info,
    warning,
    error,
    nothing
};

class Logger {
    std::ostringstream m_stream;

public:
    Logger(Severity severity, const char *file, int line);
    ~Logger();

    template <typename T>
    inline Logger &operator<<(const T &element) {
        m_stream << element;
        return *this;
    }

    // Holy shit... somebody made it super hard to use custom "stream" like
    // objects with predefined manipulators
    Logger &operator<<(std::ostream &(*manipulator)(std::ostream &)) {
        manipulator(m_stream);
        return *this;
    }

    Logger &operator<<(std::ios_base &(*manipulator)(std::ios_base &)) {
        manipulator(m_stream);
        return *this;
    }

    typedef std::basic_ios<std::ostream::char_type, std::ostream::traits_type>
            basic_ios_type;
    Logger &operator<<(basic_ios_type &(*manipulator)(basic_ios_type &) ) {
        manipulator(m_stream);
        return *this;
    }
};

} // namespace log
} // namespace vm

#define LOG(severity) vm::log::Logger(vm::log::Severity::severity, __FILE__, __LINE__)

#endif /* VM_UTILS_LOG_H */
