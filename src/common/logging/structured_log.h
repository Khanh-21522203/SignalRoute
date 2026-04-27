#pragma once

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

namespace signalroute {

struct LogField {
    std::string key;
    std::string value;
};

struct StructuredLogEvent {
    std::string level = "info";
    std::string component;
    std::string event;
    std::string message;
    std::vector<LogField> fields;
};

[[nodiscard]] StructuredLogEvent make_log_event(
    std::string level,
    std::string component,
    std::string event,
    std::string message,
    std::vector<LogField> fields = {});

[[nodiscard]] std::string format_logfmt(const StructuredLogEvent& event);
void write_logfmt(std::ostream& out, const StructuredLogEvent& event);

} // namespace signalroute
