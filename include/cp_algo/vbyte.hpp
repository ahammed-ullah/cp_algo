#ifndef CP_ALGO_VBYTE_HPP
#define CP_ALGO_VBYTE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cp_algo {

// Small in-memory lists (L_v and Gamma_v) use the same low-seven-bits-first
// vbyte convention as CCF. These lists are built after the algorithm timer
// starts. Their values are labels, so uint32_t is sufficient under cc_con's
// int-label convention.
inline void append_vbyte(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    while (value >= 0x80u) {
        bytes.push_back(static_cast<std::uint8_t>((value & 0x7fu) | 0x80u));
        value >>= 7;
    }
    bytes.push_back(static_cast<std::uint8_t>(value));
}

// The lists are produced solely by append_vbyte, so their readers need no
// repeated format validation inside an algorithm's traversal loop.
inline std::uint32_t read_vbyte(const std::vector<std::uint8_t>& bytes,
                                std::size_t& position) {
    std::uint32_t value = 0;
    unsigned shift = 0;
    for (;;) {
        const std::uint8_t byte = bytes[position++];
        value |= static_cast<std::uint32_t>(byte & 0x7fu) << shift;
        if ((byte & 0x80u) == 0) return value;
        shift += 7;
    }
}

}  // namespace cp_algo

#endif
