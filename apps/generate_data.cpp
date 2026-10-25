#include "rv/cli.hpp"
#include <random>
namespace {
struct Generator {
    std::mt19937_64 rng;
    int d, n;
    explicit Generator(std::uint64_t seed, int radix, int lines) : rng(seed), d(radix), n(lines) {}
    std::uint64_t pick(std::uint64_t bound) {
        rv::require(bound > 0, "empty random range");
        auto threshold = (std::uint64_t(0) - bound) % bound;
        for (;;) {
            auto x = rng();
            if (x >= threshold)
                return x % bound;
        }
    }
    rv::Gate random_gate(bool full = false) {
        auto g = rv::gate(n, int(pick(n)), int(pick(d - 1)));
        for (int k = 0; k < n; ++k)
            if (k != g.target && (full || pick(2)))
                g.controls[k] = int(pick(d));
        return g;
    }
    rv::Word random_word(std::size_t length) {
        rv::Word w;
        for (std::size_t k = 0; k < length; ++k)
            w.push_back(random_gate());
        return w;
    }
    void equivalent_edit(rv::Word &w) {
        int mode = int(pick(4));
        if (w.empty() || mode == 0) {
            auto g = random_gate();
            auto at = pick(w.size() + 1);
            w.insert(w.begin() + at, {g, g});
            return;
        }
        std::size_t at = pick(w.size());
        auto g = w[at];
        rv::Word replacement;
        if (mode == 1) {
            std::vector<int> free;
            for (int k = 0; k < n; ++k)
                if (k != g.target && g.controls[k] < 0)
                    free.push_back(k);
            if (!free.empty()) {
                int p = free[pick(free.size())];
                for (int a = 0; a < d; ++a) {
                    auto h = g;
                    h.controls[p] = a;
                    replacement.push_back(h);
                }
            }
        } else if (mode == 2) {
            rv::Word p;
            auto h = g;
            for (int k = 0; k < n; ++k)
                if (g.controls[k] >= 0) {
                    for (int j = g.controls[k]; j < d - 1; ++j)
                        p.push_back(rv::gate(n, k, j));
                    h.controls[k] = d - 1;
                }
            replacement = rv::joined({p, {h}, rv::reversed(p)});
        } else if (at + 1 < w.size()) {
            auto h = w[at + 1];
            bool conflict = false;
            for (int k = 0; k < n; ++k)
                if (g.controls[k] >= 0 && h.controls[k] >= 0 && g.controls[k] != h.controls[k])
                    conflict = true;
            if (conflict) {
                std::swap(w[at], w[at + 1]);
                return;
            }
        }
        if (replacement.empty()) {
            auto h = random_gate();
            w.insert(w.begin() + at, {h, h});
        } else {
            w.erase(w.begin() + at);
            w.insert(w.begin() + at, replacement.begin(), replacement.end());
        }
    }
};
} // namespace
int main(int argc, char **argv) {
    try {
        rv::Args args(argc, argv);
        args.allowed({"--help", "--out", "--d", "--n", "--gates", "--pairs", "--edits", "--seed"});
        if (args.has("--help")) {
            std::cout << "generate_data --out NEW_DIRECTORY [--d 2] [--n 3] [--gates 8]\n  "
                         "[--pairs 10] [--edits 4] [--seed 1]\nEach base circuit produces one "
                         "guaranteed equal and one guaranteed unequal pair.\n";
            return 0;
        }
        rv::require(args.has("--out"), "--out is required");
        auto radix = rv::natural(args.get("--d", "2")), lines = rv::natural(args.get("--n", "3")),
             gates = rv::natural(args.get("--gates", "8")),
             pairs = rv::natural(args.get("--pairs", "10")),
             edits = rv::natural(args.get("--edits", "4")),
             seed = rv::natural(args.get("--seed", "1"));
        rv::require(radix >= 2 && radix <= 32 && lines >= 1 && lines <= 64,
                    "generator dimensions out of range");
        rv::require(gates <= 100000 && pairs > 0 && pairs <= 100000 && edits <= 10000,
                    "generator sizes out of range");
        std::filesystem::path dir = args.get("--out");
        rv::require(!std::filesystem::exists(dir), "output directory already exists");
        std::filesystem::create_directories(dir);
        Generator gen(seed, int(radix), int(lines));
        std::ofstream nf(dir / "normalization.tsv"), eq(dir / "equivalence.tsv"),
            meta(dir / "generation.txt");
        rv::require(bool(nf) && bool(eq) && bool(meta), "cannot write manifests");
        nf << "# circuit\n";
        eq << "# left\tright\texpected\n";
        meta << "generator_version=1\nseed=" << seed << "\nd=" << radix << "\nn=" << lines
             << "\ngates=" << gates << "\nbase_cases=" << pairs << "\nedits=" << edits << "\n";
        for (std::uint64_t i = 0; i < pairs; ++i) {
            rv::Circuit a{int(radix), int(lines), gen.random_word(gates),
                          "seed=" + std::to_string(seed) + " case=" + std::to_string(i)},
                b = a;
            for (std::uint64_t j = 0; j < edits; ++j) {
                gen.equivalent_edit(b.gates);
                rv::require(b.gates.size() <= 1000000, "generated circuit too long");
            }
            if (b.gates == a.gates) {
                auto g = gen.random_gate();
                b.gates.push_back(g);
                b.gates.push_back(g);
            }
            rv::Circuit unequal = b;
            unequal.gates.push_back(gen.random_gate(true));
            auto prefix = "case_" + std::to_string(i);
            std::string an = prefix + "_a.cat", bn = prefix + "_equal.cat",
                        cn = prefix + "_unequal.cat";
            rv::write_circuit((dir / an).string(), a);
            rv::write_circuit((dir / bn).string(), b);
            rv::write_circuit((dir / cn).string(), unequal);
            nf << an << '\n';
            eq << an << '\t' << bn << "\ttrue\n" << an << '\t' << cn << "\tfalse\n";
        }
        rv::require(bool(nf) && bool(eq) && bool(meta), "manifest write failed");
        std::cout << "Created input circuits and manifests in " << dir
                  << "; no benchmark was run.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
