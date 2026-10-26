#include "rv/cli.hpp"
#include <cmath>
namespace rv {
namespace {
struct Sample {
    std::string a, b, expected;
};
std::vector<Sample> samples(const Args &args, bool eq) {
    std::vector<Sample> out;
    if (args.has("--manifest")) {
        require(!args.has("--input") && !args.has("--left") && !args.has("--right"),
                "use either manifest or single input");
        auto file = std::filesystem::absolute(args.get("--manifest"));
        std::ifstream in(file);
        require(bool(in), "cannot open manifest");
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty() || line[0] == '#')
                continue;
            std::vector<std::string> fields;
            std::istringstream row(line);
            std::string f;
            while (std::getline(row, f, '\t'))
                fields.push_back(f);
            require(fields.size() == (eq ? 3u : 1u),
                    "manifest: expected tab-separated " +
                        std::string(eq ? "left, right, expected" : "input"));
            auto resolve = [&](std::string p) {
                return (file.parent_path() / p).lexically_normal().string();
            };
            Sample s{resolve(fields[0]), "", ""};
            if (eq) {
                s.b = resolve(fields[1]);
                s.expected = fields[2];
                require(s.expected == "true" || s.expected == "false" || s.expected == "?",
                        "expected must be true, false, or ?");
            }
            out.push_back(s);
        }
    } else if (eq) {
        require(args.has("--left") && args.has("--right") && !args.has("--input"),
                "provide --left/--right or --manifest");
        out.push_back({args.get("--left"), args.get("--right"), "?"});
    } else {
        require(args.has("--input") && !args.has("--left") && !args.has("--right"),
                "provide --input or --manifest");
        out.push_back({args.get("--input"), "", ""});
    }
    require(!out.empty(), "empty manifest");
    return out;
}
} // namespace
int benchmark_main(int argc, char **argv, bool eq) {
    try {
        Args args(argc, argv);
        args.allowed({"--help", "--algorithm", "--input", "--left", "--right", "--manifest",
                      "--output", "--repeat", "--seconds", "--max-states", "--max-word",
                      "--max-work", "--wire-order"});
        if (args.has("--help")) {
            std::cout << (eq ? "bench_equivalence --left A.cat --right B.cat"
                             : "bench_normalize --input A.cat")
                      << " --algorithm fl2025|fxl2026|nf|dc|all\n  or --manifest FILE.tsv\n  "
                         "[--output results.csv] [--repeat 1] [--seconds 60]\n  [--max-states "
                         "65536] [--max-word 100000000] [--max-work 2000000]\n"
                         "  [--wire-order target-frequency|original] (default: target-frequency)\n";
            return 0;
        }
        Limits limits;
        limits.max_states = natural(args.get("--max-states", "65536"));
        limits.max_word = natural(args.get("--max-word", std::to_string(limits.max_word)));
        limits.max_work = natural(args.get("--max-work", "2000000"));
        std::size_t used = 0;
        auto seconds = args.get("--seconds", "60");
        limits.seconds = std::stod(seconds, &used);
        require(used == seconds.size() && std::isfinite(limits.seconds) && limits.seconds > 0,
                "seconds must be finite and positive");
        require(limits.max_states > 0 && limits.max_word > 0 && limits.max_work > 0,
                "limits must be positive");
        auto repeats = natural(args.get("--repeat", "1"));
        require(repeats > 0 && repeats <= 1000000, "repeat outside 1..1000000");
        const auto wire_order = args.get("--wire-order", "target-frequency");
        require(wire_order == "target-frequency" || wire_order == "original",
                "wire-order must be target-frequency or original");
        const bool reorder = wire_order == "target-frequency";
        std::vector<Method> methods;
        auto choice = args.get("--algorithm", "all");
        if (choice == "all")
            methods = {Method::FL2025, Method::FXL2026, Method::NF, Method::DC};
        else
            methods = {parse_method(choice)};
        auto inputs = samples(args, eq);
        std::ofstream file;
        std::ostream *output = &std::cout;
        if (args.has("--output")) {
            require(!std::filesystem::exists(args.get("--output")),
                    "output already exists; choose a new file");
            file.open(args.get("--output"));
            require(bool(file), "cannot open output file");
            output = &file;
        }
        auto &out = *output;
        out << "algorithm,input_a,input_b,repeat,status,equal,rule_count,time_ms";
        if (reorder)
            out << ",wire_order,wire_map";
        out << '\n';
        int errors = 0;
        for (const auto &s : inputs) {
            Circuit a, b;
            std::string load_error;
            try {
                a = read_circuit(s.a);
                if (eq)
                    b = read_circuit(s.b);
            } catch (const std::exception &e) {
                load_error = e.what();
            }
            for (auto method : methods)
                for (std::uint64_t rep = 0; rep < repeats; ++rep) {
                    std::string status = "ok", equal_text, count, error;
                    std::vector<int> wire_map;
                    double ms = 0;
                    if (!load_error.empty()) {
                        status = "input_error";
                        error = load_error;
                    } else if ((method == Method::FL2025 || method == Method::FXL2026) &&
                               (a.d != 2 || (eq && b.d != 2)))
                        status = "not_applicable";
                    else {
                        auto start = std::chrono::steady_clock::now();
                        try {
                            Budget budget(limits);
                            Result result;
                            if (reorder) {
                                wire_map = target_frequency_order(a, eq ? &b : nullptr, &budget);
                                auto mapped_a = permute_wires(a, wire_map, &budget);
                                if (eq) {
                                    auto mapped_b = permute_wires(b, wire_map, &budget);
                                    result = equivalent(mapped_a, mapped_b, method, budget);
                                } else
                                    result = normalize(mapped_a, method, budget);
                            } else
                                result = eq ? equivalent(a, b, method, budget)
                                            : normalize(a, method, budget);
                            ms = std::chrono::duration<double, std::milli>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();
                            if (eq)
                                equal_text = result.equivalent ? "true" : "false";
                            // For successful equivalence, include all bridge pair
                            // insertions required by Theorem 7. For unequal pairs,
                            // report only work until the sound rejection point.
                            count = std::to_string(result.rule_count);
                            if (eq && s.expected != "?" && s.expected != equal_text) {
                                status = "label_mismatch";
                                error = "decision disagrees with manifest label";
                            }
                        } catch (const std::exception &e) {
                            ms = std::chrono::duration<double, std::milli>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();
                            error = e.what();
                            status =
                                error.rfind("resource_limit:", 0) == 0 ? "resource_limit" : "error";
                        }
                    }
                    out << method_name(method) << ',' << csv(s.a) << ',' << csv(s.b) << ',' << rep
                        << ',' << status << ',' << equal_text << ',' << count << ',' << std::fixed
                        << std::setprecision(6) << ms;
                    if (reorder) {
                        std::string mapping;
                        if (!wire_map.empty()) {
                            mapping = "[";
                            for (std::size_t k = 0; k < wire_map.size(); ++k) {
                                if (k)
                                    mapping += ',';
                                mapping += std::to_string(wire_map[k]);
                            }
                            mapping += ']';
                        }
                        out << ',' << wire_order << ',' << csv(mapping);
                    }
                    out << '\n';
                    out.flush();
                    require(bool(out), "result write failed");
                    if (status != "ok" && status != "not_applicable") {
                        ++errors;
                        std::cerr << method_name(method) << " " << s.a << ": " << error << '\n';
                    }
                }
        }
        return errors ? 2 : 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
} // namespace rv
