#include "log.h"
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace std;
namespace vm {
namespace log {

namespace {
static void write_severity_color(ostream &out, Severity severity) {
    switch (severity) {
    case Severity::trace:
        out << "\033[90m";
        break;
    case Severity::debug:
        out << "\033[37m";
        break;
    case Severity::info:
        out << "\033[92m";
        break;
    case Severity::warning:
        out << "\033[93m";
        break;
    case Severity::error:
        out << "\033[91m";
        break;
    default:
        out << "\033[0m";
        break;
    }
}

static void write_severity(ostream &out, Severity severity) {
    static const string severity_names[] = {
        "[Trace]",
        "[Debug]",
        "[Info]",
        "[Warning]",
        "[Error]"
    };
    out << severity_names[static_cast<int>(severity)];
}

static void write_short_source_name(ostream &out, const string &name, int line) {
    auto idx = name.rfind("src/");
    auto name_to_write = name;
    if (idx != string::npos) {
        name_to_write = name.substr(idx);
    }
    out << '(' << name_to_write << ':' << line << ')';
}

static void write_timestamp(ostream &out) {
    auto current_time = chrono::system_clock::now();
    time_t now_ctime = chrono::system_clock::to_time_t(current_time);
    out << put_time(localtime(&now_ctime), "\033[90m%H:%M:%S ");
}
} // namespace

Logger::Logger(Severity severity, const char *filename, int line)
        : m_stream() {
    write_timestamp(m_stream);
    write_severity_color(m_stream, severity);
    write_severity(m_stream, severity);
    write_short_source_name(m_stream, filename, line);
    m_stream << ' ';
}

Logger::~Logger() {
    cerr << m_stream.str();
    write_severity_color(cerr, Severity::nothing);
    cerr << endl;
}

} // namespace log
} // namespace vm
