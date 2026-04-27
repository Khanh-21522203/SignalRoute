#include "common/logging/structured_log.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

void test_logfmt_includes_standard_fields() {
    const auto event = signalroute::make_log_event(
        "info",
        "signalroute",
        "runtime.started",
        "runtime started",
        {signalroute::LogField{"role", "gateway"}});

    const auto line = signalroute::format_logfmt(event);

    assert(line == "level=info component=signalroute event=runtime.started message=\"runtime started\" role=gateway");
}

void test_logfmt_escapes_special_characters() {
    const auto event = signalroute::make_log_event(
        "error",
        "signalroute",
        "runtime.fatal",
        "fatal runtime error",
        {signalroute::LogField{"error", "config \"bad\"\nline"}});

    const auto line = signalroute::format_logfmt(event);

    assert(line.find("level=error") != std::string::npos);
    assert(line.find("error=\"config \\\"bad\\\"\\nline\"") != std::string::npos);
}

void test_write_logfmt_appends_newline() {
    std::ostringstream out;
    signalroute::write_logfmt(out, signalroute::make_log_event("info", "admin", "health", "ok"));

    const auto line = out.str();
    assert(!line.empty());
    assert(line.back() == '\n');
    assert(line.find("component=admin") != std::string::npos);
}

int main() {
    std::cout << "test_structured_log:\n";
    test_logfmt_includes_standard_fields();
    test_logfmt_escapes_special_characters();
    test_write_logfmt_appends_newline();
    std::cout << "All structured log tests passed.\n";
    return 0;
}
