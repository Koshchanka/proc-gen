#pragma once

#include "wfc.h"

#include <cassert>
#include <iostream>
#include <map>
#include <vector>

namespace wfc {

template<typename Tile>
class MatrixEncoder {
public:
	Pattern Fit(const std::vector<std::vector<Tile>>& pattern, size_t k,
				bool hwrap = false, bool vwrap = false, bool rotate = false) {
		assert(k != 0);
		assert(pattern.size() >= k);
		assert(pattern[0].size() >= k);

		id_to_submat_.clear();
		k_ = k;

		if (pattern.empty()) {
			return {};
		}

		size_t n = pattern.size();
		size_t m = pattern[0].size();

		auto flatten_submat = [&](size_t i, size_t j, size_t rot = 0) {
			std::vector<Tile> res;
			res.reserve(k * k);
			switch (rot) {
			case 0: {
				for (size_t di = 0; di < k; ++di) {
					for (size_t dj = 0; dj < k; ++dj) {
						res.push_back(pattern[(i + di) % n][(j + dj) % m]);
					}
				}
				break;
			}
			case 1: {
				for (size_t dj = 0; dj < k; ++dj) {
					for (size_t di = k - 1; di != -1; --di) {
						res.push_back(pattern[(i + di) % n][(j + dj) % m]);
					}
				}
				break;
			}
			case 2: {
				for (size_t di = k - 1; di != -1; --di) {
					for (size_t dj = k - 1; dj != -1; --dj) {
						res.push_back(pattern[(i + di) % n][(j + dj) % m]);
					}
				}
				break;
			}
			case 3: {
				for (size_t dj = k - 1; dj != -1; --dj) {
					for (size_t di = 0; di < k; ++di) {
						res.push_back(pattern[(i + di) % n][(j + dj) % m]);
					}
				}
				break;
			}
			}
			return res;
		};

		std::map<std::vector<Tile>, size_t> submat_to_id;
		std::vector<size_t> occ_cnt;

		size_t upper_i = (vwrap ? n : n - k + 1);
		size_t upper_j = (hwrap ? m : m - k + 1);

		// count all unique submatrices
		for (size_t i = 0; i < upper_i; ++i) {
			for (size_t j = 0; j < upper_j; ++j) {
				auto process = [&](const std::vector<Tile>& flat) {
					auto iter = submat_to_id.find(flat);
					if (iter == submat_to_id.end()) {
						size_t new_id = id_to_submat_.size();
						submat_to_id.insert({flat, new_id});
						id_to_submat_.push_back(std::move(flat));
						occ_cnt.push_back(1);
					} else {
						++occ_cnt[iter->second];
					}
				};
				process(flatten_submat(i, j, 0));
				if (rotate) {
					for (size_t rot = 1; rot < 4; ++rot) {
						process(flatten_submat(i, j, rot));
					}
				}
			}
		}

		Pattern res;
		res.edges.resize(occ_cnt.size());
		res.probs.resize(occ_cnt.size());

		double prob_scale = upper_i * upper_j;
		for (size_t i = 0; i < res.probs.size(); ++i) {
			res.probs[i] = occ_cnt[i] / prob_scale;
		}

		// find all compatible pairs in 4 directions
		auto check_compatible = [k](const std::vector<Tile>& p1,
									const std::vector<Tile>& p2,
									size_t dir) {
			for (size_t i1 = 0; i1 < k; ++i1) {
				for (size_t j1 = 0; j1 < k; ++j1) {
					size_t i2 = i1 - kDirDn[dir];
					size_t j2 = j1 - kDirDm[dir];
					if (i2 == -1 || i2 == k) {
						break;
					}
					if (j2 == -1 || j2 == k) {
						continue;
					}
					if (p1[k * i1 + j1] != p2[k * i2 + j2]) {
						return false;
					}
				}
			}
			return true;
		};

		for (size_t p1 = 0; p1 < res.edges.size(); ++p1) {
			for (size_t dir = 0; dir < 4; ++dir) {
				for (size_t p2 = 0; p2 < res.edges.size(); ++p2) {
					if (check_compatible(id_to_submat_[p1],
										 id_to_submat_[p2],
										 dir)) {
						res.edges[p1][dir].push_back(p2);
					}
				}
			}
		}

		if (!hwrap || !vwrap) {
			for (const auto& data : res.edges) {
				for (const auto& dir_data : data) {
					std::cerr << "at least one square is incompatible with every other in one of the directions" << std::endl;
					goto warned;
				}
			}
			warned:;
		}

		return res;
	}

	std::vector<std::vector<Tile>>
	Decode(std::vector<std::vector<size_t>>& wave) const {
		assert(!wave.empty());
		assert(!wave[0].empty());
		assert(k_ != 0 && "no Fit call for MatrixEncoder");

		size_t n = wave.size();
		size_t m = wave[0].size();

		std::vector<std::vector<Tile>> res(n + k_ - 1,
										   std::vector<Tile>(m + k_ - 1));

		for (size_t i = 0; i < n; ++i) {
			for (size_t j = 0; j < m; ++j) {
				res[i][j] = id_to_submat_[wave[i][j]][0];
			}
		}

		for (size_t di = 0; di < k_ - 1; ++di) {
			for (size_t j = 0; j < m; ++j) {
				res[n + di][j] = id_to_submat_[wave[n - 1][j]][k_ * (di + 1)];
			}
		}

		for (size_t i = 0; i < n; ++i) {
			for (size_t dj = 0; dj < k_ - 1; ++dj) {
				res[i][m + dj] = id_to_submat_[wave[i][m - 1]][dj + 1];
			}
		}

		const auto& corner_submat = id_to_submat_[wave.back().back()];
		for (size_t di = 0; di < k_ - 1; ++di) {
			for (size_t dj = 0; dj < k_ - 1; ++dj) {
				res[n + di][m + dj] = corner_submat[k_ * (di + 1) + dj + 1];
			}
		}

		return res;
	}

private:
	std::vector<std::vector<Tile>> id_to_submat_;

	size_t k_ = 0;
};

}  // namespace wfc

