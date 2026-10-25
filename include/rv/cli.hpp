#pragma once
#include "rv/compiler.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
namespace rv {
struct Args {
    std::map<std::string, std::string> values;
    Args(int argc, char **argv) {
        for (int i = 1; i < argc; ++i) {
            std::string k = argv[i];
            require(k.rfind("--", 0) == 0, "expected --option");
            if (k == "--help") {
                values[k] = "1";
                continue;
            }
            require(i + 1 < argc, "missing value: " + k);
            require(values.emplace(k, argv[++i]).second, "duplicate option: " + k);
        }
    }
    std::string get(const std::string &k, const std::string &fallback = "") const {
        auto i = values.find(k);
        return i == values.end() ? fallback : i->second;
    }
    bool has(const std::string &k) const {
        return values.count(k);
    }
    void allowed(std::initializer_list<std::string> keys) const {
        for (const auto &kv : values)
            require(std::find(keys.begin(), keys.end(), kv.first) != keys.end(),
                    "unknown option: " + kv.first);
    }
};
inline std::uint64_t natural(const std::string &s) {
    require(!s.empty() && s[0] != '-', "nonnegative integer required");
    std::size_t used = 0;
    auto v = std::stoull(s, &used);
    require(used == s.size(), "invalid integer: " + s);
    return v;
}
inline std::string csv(const std::string &s) {
    std::string r = "\"";
    for (char c : s) {
        if (c == '\"')
            r += '\"';
        r += c;
    }
    return r + '\"';
}
int benchmark_main(int argc, char **argv, bool equivalence_mode);
} // namespace rv
