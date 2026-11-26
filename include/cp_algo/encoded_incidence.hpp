#ifndef CP_ALGO_ENCODED_INCIDENCE_HPP
#define CP_ALGO_ENCODED_INCIDENCE_HPP

#include "cp_algo/ccf.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace cp_algo {

// The paper's L_v lists, stored as one array of fixed-width clique labels.
// offsets[v] and offsets[v+1] delimit L_v in that array.
struct EncodedIncidence {
    std::vector<std::uint32_t> offsets;
    std::vector<std::uint8_t> labels;
    unsigned width = 1;

    void put(std::uint32_t slot, std::uint32_t label) {
        const std::size_t bit = static_cast<std::size_t>(slot) * width;
        const std::size_t byte = bit / 8;
        const unsigned shift = static_cast<unsigned>(bit % 8);
        const std::uint64_t value = static_cast<std::uint64_t>(label) << shift;
        const unsigned count = (width + shift + 7) / 8;
        for (unsigned i = 0; i < count; ++i)
            labels[byte + i] |= static_cast<std::uint8_t>(value >> (8 * i));
    }

    int at(std::uint32_t slot) const {
        const std::size_t bit = static_cast<std::size_t>(slot) * width;
        const std::size_t byte = bit / 8;
        const unsigned shift = static_cast<unsigned>(bit % 8);
        const unsigned count = (width + shift + 7) / 8;
        std::uint64_t value = 0;
        for (unsigned i = 0; i < count; ++i)
            value |= static_cast<std::uint64_t>(labels[byte + i]) << (8 * i);
        return static_cast<int>((value >> shift) & ((1u << width) - 1u));
    }
};

inline EncodedIncidence build_encoded_incidence(const Ccf& ccf) {
    // A single 32-bit offset per vertex is enough for the evaluated covers.
    if (ccf.cover_size > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("encoded incidence exceeds 32-bit offsets");

    EncodedIncidence L;
    L.offsets.resize(static_cast<std::size_t>(ccf.n) + 1);
    while ((1u << L.width) < static_cast<unsigned>(ccf.num_cliques))
        ++L.width;

    // Count memberships, then turn counts into end positions.
    for (int q = 0; q < ccf.num_cliques; ++q) {
        auto clique = ccf.cursor(q);
        int v;
        while (clique.next(v)) ++L.offsets[v];
    }
    std::uint32_t total = 0;
    for (int v = 0; v < ccf.n; ++v) {
        total += L.offsets[v];
        L.offsets[v] = total;
    }
    L.offsets[ccf.n] = total;
    L.labels.resize((static_cast<std::size_t>(total) * L.width + 7) / 8);

    // Fill backward so each L_v ends in increasing clique-label order.
    // Decrementing its end position leaves offsets[v] at L_v's start.
    for (int q = ccf.num_cliques; q-- > 0;) {
        auto clique = ccf.cursor(q);
        int v;
        while (clique.next(v)) L.put(--L.offsets[v], static_cast<std::uint32_t>(q));
    }
    return L;
}

}  // namespace cp_algo

#endif
