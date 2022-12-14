#pragma once

#include <array>
#include <vector>
#include <optional>

namespace wfc {

enum Dir : uint32_t {
	kDown,
	kLeft,
	kUp,
	kRight,
};
constexpr size_t kDirDn[4] = {1, 0, -1ull, 0};
constexpr size_t kDirDm[4] = {0, -1ull, 0, 1};

struct Pattern {
	// edges[i][dir] is the list of tiles compatible with the ith tile
	// in direction dir.
	// IMPORTANT: all elements in edges[i][dir] MUST be distinct.
	std::vector<std::array<std::vector<size_t>, 4>> edges;

	// Probability of each pattern occuring.
	std::vector<double> probs;
};

using Wave = std::vector<std::vector<size_t>>;

std::optional<Wave> Collapse(const Pattern& pat, size_t n, size_t m);

}  // namespace wfc

