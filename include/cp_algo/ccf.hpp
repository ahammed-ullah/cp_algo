#ifndef CP_ALGO_CCF_HPP
#define CP_ALGO_CCF_HPP

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cp_algo {

// The disk format is described by cc_con/docs/output-formats.md. Vertex and
// clique labels remain int, as in cc_con; byte positions follow the CCF format.
class Ccf {
public:
    // The encoded DFS keeps only this progress state for each clique.
    struct CompactCliqueCursor {
        std::uint32_t position;
        int previous = -1;
    };

    class CliqueCursor {
    public:
        CliqueCursor(const Ccf& ccf, int label);
        bool next(int& vertex);

    private:
        const Ccf* ccf_;
        std::uint64_t position_;
        std::uint64_t end_;
        std::uint64_t remaining_;
        int previous_ = 0;
        bool first_ = true;
    };

    static Ccf load(const std::filesystem::path& path);

    CliqueCursor cursor(int label) const { return CliqueCursor(*this, label); }
    CompactCliqueCursor compact_cursor(int label) const;
    bool next_compact(int label, CompactCliqueCursor& cursor, int& vertex) const;
    void decode_clique(int label, std::vector<int>& vertices) const;
    std::vector<std::vector<int>> decode_all() const;
    void release_bytes();

    int n = 0;
    int num_cliques = 0;
    std::uint64_t m = 0;
    std::uint64_t cover_size = 0;
    std::uint64_t file_bytes = 0;
    std::string graph_fingerprint_sha256;
    std::string file_checksum_sha256;
    std::filesystem::path path;

private:
    std::uint64_t offset(int label) const;
    std::uint64_t read_vbyte(std::uint64_t& position,
                             std::uint64_t end) const;
    std::vector<std::uint8_t> bytes_;

    friend class CliqueCursor;
};

}  // namespace cp_algo

#endif
