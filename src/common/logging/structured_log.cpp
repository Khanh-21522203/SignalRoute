#include "structured_log.h"

#include <ostream>
#include <sstream>
#include <utility>

namespace signalroute {
namespace {

bool needs_quotes(const std::string& value) {
    if (value.empty()) {
        return true;
    }
    for (const char ch : value) {
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '"' || ch == '\\' || ch == '=') {
            return true;
        }
    }
    return false;
}

std::string escape_logfmt_value(const std::string& value) {
    if (!needs_quotes(value)) {
        return value;
    }

    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (const char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    out.push_back('"');
    return out;
}

void append_field(std::ostringstream& out, const std::string& key, const std::string& value) {
    if (key.empty()) {
        return;
    }
    out << ' ' << key << '=' << escape_logfmt_value(value);
}

} // namespace

StructuredLogEvent make_log_event(
    std::string level,
    std::string component,
    std::string event,
    std::string message,
    std::vector<LogField> fields) {
    StructuredLogEvent log_event;
    log_event.level = std::move(level);
    log_event.component = std::move(component);
    log_event.event = std::move(event);
    log_event.message = std::move(message);
    log_event.fields = std::move(fields);
    return log_event;
}

std::string format_logfmt(const StructuredLogEvent& event) {
    std::ostringstream out;
    out << "level=" << escape_logfmt_value(event.level.empty() ? "info" : event.level);
    append_field(out, "component", event.component);
    append_field(out, "event", event.event);
    append_field(out, "message", event.message);
    for (const auto& field : event.fields) {
        append_field(out, field.key, field.value);
    }
    return out.str();
}

void write_logfmt(std::ostream& out, const StructuredLogEvent& event) {
    out << format_logfmt(event) << '\n';
}

} // namespace signalroute
