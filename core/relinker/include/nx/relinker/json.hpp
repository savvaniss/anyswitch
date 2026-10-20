// SPDX-License-Identifier: MIT
// Tiny JSON emitter for machine-readable tool output.
#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace nx::json {

inline std::string quote(std::string_view s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                out += std::format("\\u{:04x}", c);
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    out += '"';
    return out;
}

// Builds one JSON object or array; call key() before each value inside objects.
class Builder {
public:
    Builder& begin_object() { return open('{'); }
    Builder& end_object() { return close('}'); }
    Builder& begin_array() { return open('['); }
    Builder& end_array() { return close(']'); }

    Builder& key(std::string_view k) {
        comma();
        out_ += quote(k);
        out_ += ':';
        pending_key_ = true;
        return *this;
    }
    Builder& value(std::string_view s) { return raw(quote(s)); }
    Builder& value(const char* s) { return raw(quote(s)); }
    Builder& value(std::uint64_t v) { return raw(std::to_string(v)); }
    Builder& value(std::int64_t v) { return raw(std::to_string(v)); }
    Builder& value(int v) { return raw(std::to_string(v)); }
    Builder& value(unsigned v) { return raw(std::to_string(v)); }
    Builder& value(bool v) { return raw(v ? "true" : "false"); }

    const std::string& str() const { return out_; }

private:
    Builder& open(char c) {
        comma();
        out_ += c;
        first_ = true;
        return *this;
    }
    Builder& close(char c) {
        out_ += c;
        first_ = false;
        return *this;
    }
    Builder& raw(const std::string& s) {
        comma();
        out_ += s;
        return *this;
    }
    void comma() {
        if (pending_key_) {
            pending_key_ = false;
            first_ = false;
            return;
        }
        if (!first_ && !out_.empty()) {
            out_ += ',';
        }
        first_ = false;
    }

    std::string out_;
    bool first_ = true;
    bool pending_key_ = false;
};

} // namespace nx::json
