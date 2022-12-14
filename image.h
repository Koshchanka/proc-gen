#pragma once

#include <string>
#include <vector>

namespace wfc {

struct RGB {
	int r, g, b;

	bool operator==(const RGB& rhs) const {
		return std::tie(r, g, b) == std::tie(rhs.r, rhs.g, rhs.b);
	}

	bool operator<(const RGB& rhs) const {
		return std::tie(r, g, b) < std::tie(rhs.r, rhs.g, rhs.b);
	}
};

using Image = std::vector<std::vector<RGB>>;

Image ReadPng(const std::string& filename);

void WritePng(const Image&, const std::string& filename);

}  // namespace wfc

