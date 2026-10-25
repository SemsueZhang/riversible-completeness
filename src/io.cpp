#include "rv/core.hpp"
#include <fstream>
#include <set>
#include <sstream>
namespace rv {
namespace {
constexpr std::size_t input_gate_limit = 1000000;
int integer(const std::string &s) {
    std::size_t used = 0;
    int x = std::stoi(s, &used);
    require(used == s.size(), "invalid integer: " + s);
    return x;
}
std::vector<std::string> tokens(std::string s) {
    auto p = s.find('#');
    if (p != std::string::npos)
        s.resize(p);
    std::istringstream in(s);
    std::vector<std::string> out;
    std::string t;
    while (in >> t)
        out.push_back(t);
    return out;
}
Circuit read_cat(std::istream &in) {
    Circuit c;
    std::string line;
    bool header = false, end = false;
    while (std::getline(in, line)) {
        auto t = tokens(line);
        if (t.empty())
            continue;
        if (!header) {
            require(t.size() == 4 && t[0] == "CAT" && t[1] == "1", "expected CAT 1 d n");
            c.d = integer(t[2]);
            c.n = integer(t[3]);
            require(c.d >= 2 && c.d <= 1024 && c.n >= 1 && c.n <= 64,
                    "unsupported input dimensions");
            header = true;
            continue;
        }
        require(!end, "trailing data after END");
        if (t[0] == "END") {
            require(t.size() == 1, "invalid END");
            end = true;
            continue;
        }
        require(t.size() == std::size_t(c.n) + 3 && t[0] == "G",
                "expected G target lower and n controls");
        auto g = gate(c.n, integer(t[1]), integer(t[2]));
        for (int k = 0; k < c.n; ++k)
            g.controls[k] = t[k + 3] == "*" ? -1 : integer(t[k + 3]);
        validate(g, c.d, c.n);
        require(c.gates.size() < input_gate_limit, "too many input gates");
        c.gates.push_back(std::move(g));
    }
    require(header && end, "CAT input missing header or END");
    return c;
}
Circuit read_real(std::istream &in) {
    Circuit c;
    std::vector<std::string> vars;
    std::map<std::string, int> names;
    std::string line;
    bool body = false, done = false, have_n = false;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        auto t = tokens(line);
        if (t.empty())
            continue;
        require(!done, "trailing REAL content");
        auto error = [&](const std::string &msg) {
            throw std::runtime_error("REAL line " + std::to_string(lineno) + ": " + msg);
        };
        if (t[0] == ".begin") {
            if (body || !have_n || vars.size() != std::size_t(c.n) || t.size() != 1)
                error("invalid .begin/header");
            body = true;
            continue;
        }
        if (t[0] == ".end") {
            if (!body || t.size() != 1)
                error("invalid .end");
            done = true;
            continue;
        }
        if (!body) {
            if (t[0] == ".version") {
                if (t.size() != 2 || (t[1] != "1.0" && t[1] != "2.0"))
                    error("unsupported version");
            } else if (t[0] == ".numvars") {
                if (t.size() != 2 || have_n)
                    error("invalid .numvars");
                c.n = integer(t[1]);
                if (c.n < 1 || c.n > 64)
                    error("unsupported wire count");
                have_n = true;
            } else if (t[0] == ".variables") {
                if (!have_n || !vars.empty() || t.size() != std::size_t(c.n) + 1)
                    error("invalid .variables");
                vars.assign(t.begin() + 1, t.end());
                for (int k = 0; k < c.n; ++k)
                    if (!names.emplace(vars[k], k).second)
                        error("duplicate variable");
            } else if (t[0] == ".constants" || t[0] == ".garbage") {
                if (!have_n || t.size() != 2 || t[1].size() != std::size_t(c.n))
                    error("invalid constants/garbage");
                for (char ch : t[1])
                    if (ch != '-')
                        error("constant inputs/garbage outputs require embedding semantics; use an "
                              "unrestricted circuit");
            } else if (t[0] == ".inputs" || t[0] == ".outputs") {
                if (!have_n || t.size() != std::size_t(c.n) + 1)
                    error("invalid labels");
            } else
                error("unsupported directive: " + t[0]);
            continue;
        }
        if (t[0].size() < 2 || t[0][0] != 't')
            error("only tN (mixed-polarity Toffoli) gates are supported");
        int arity = integer(t[0].substr(1));
        if (arity < 1 || arity > c.n || t.size() != std::size_t(arity) + 1)
            error("gate arity mismatch");
        std::set<int> seen;
        auto g = gate(c.n, 0);
        for (int k = 0; k < arity; ++k) {
            std::string name = t[k + 1];
            bool negative = false;
            if (!name.empty() && name.front() == '-') {
                negative = true;
                name.erase(name.begin());
            }
            if (!name.empty() && name.back() == '\'') {
                if (negative)
                    error("duplicate polarity marker");
                negative = true;
                name.pop_back();
            }
            auto it = names.find(name);
            if (it == names.end() || !seen.insert(it->second).second)
                error("unknown or repeated gate variable");
            if (k == arity - 1) {
                if (negative)
                    error("negative target");
                g.target = it->second;
            } else
                g.controls[it->second] = negative ? 0 : 1;
        }
        validate(g, 2, c.n);
        if (c.gates.size() >= input_gate_limit)
            error("too many gates");
        c.gates.push_back(std::move(g));
    }
    require(done, "REAL input missing .end");
    c.metadata = "RevLib REAL; unrestricted full permutation; variable order retained";
    return c;
}
} // namespace
Circuit read_circuit(const std::string &path) {
    std::ifstream in(path);
    require(bool(in), "cannot open circuit: " + path);
    std::string line;
    while (std::getline(in, line)) {
        auto t = tokens(line);
        if (t.empty())
            continue;
        in.clear();
        in.seekg(0);
        return t[0] == "CAT" ? read_cat(in) : read_real(in);
    }
    throw std::runtime_error("empty circuit file: " + path);
}
void write_circuit(const std::string &path, const Circuit &c) {
    std::ofstream out(path);
    require(bool(out), "cannot write circuit: " + path);
    out << "CAT 1 " << c.d << ' ' << c.n << '\n';
    if (!c.metadata.empty())
        out << "# " << c.metadata << '\n';
    for (auto &g : c.gates) {
        validate(g, c.d, c.n);
        out << "G " << g.target << ' ' << g.lower;
        for (int v : g.controls)
            if (v < 0)
                out << " *";
            else
                out << ' ' << v;
        out << '\n';
    }
    out << "END\n";
    require(bool(out), "write failed: " + path);
}
} // namespace rv
