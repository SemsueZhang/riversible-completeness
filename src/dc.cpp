#include "rv/compiler.hpp"
namespace rv {
Compiler::Scan Compiler::cancel_path(const Word &input) {
    require(method == Method::DC, "chain scan is DC-only");
    Builder b(budget, input);
    while (!b.word.empty()) {
        budget.tick(b.word.size());
        int i = int(path_gates.size());
        for (auto &g : b.word)
            i = std::min(i, indices.at(g));
        int k = i - 1;
        std::size_t emitted = 0;
        Word round = b.word;
        for (const auto &g : round) {
            budget.tick(b.word.size());
            int j = indices.at(g), chain = k - i + 1;
            std::size_t at = emitted + std::size_t(chain);
            require(at < b.word.size() && b.word[at] == g, "chain cursor mismatch");
            if (j > k + 1) {
                for (int z = 0; z < chain; ++z)
                    commute(b, at - 1 - z);
                ++emitted;
            } else if (j == k + 1) {
                ++k;
            } else if (j == k) {
                cancel(b, at - 1);
                --k;
            } else {
                require(i <= j && j < k, "invalid chain case");
                for (int z = k; z >= j + 2; --z)
                    commute(b, emitted + std::size_t(z - i));
                std::size_t p = emitted + std::size_t(j - i);
                b.call(braid(b.word[p], b.word[p + 1]), p);
                while (p > emitted) {
                    commute(b, p - 1);
                    --p;
                }
                ++emitted;
            }
        }
        // A nonempty final chain moves s_i; V fixes s_i. This is a rigorous
        // inequivalence witness, so do not silently discard the chain.
        if (k >= i)
            return {b.finish(), false};
        require(emitted == b.word.size(), "chain round endpoint");
    }
    return {b.finish(), true};
}
} // namespace rv
