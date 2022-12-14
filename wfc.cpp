#include "wfc.h"

#include <iostream>
#include <cmath>
#include <optional>
#include <queue>
#include <random>
#include <vector>

namespace wfc {

namespace {

Dir InverseDir(Dir dir) {
	switch (dir) {
		case kUp: return kDown;
		case kDown: return kUp;
		case kLeft: return kRight;
		case kRight: return kLeft;
	}
}

class Cell {
public:
	Cell(const Pattern& pat)
		: possible_(pat.edges.size(), true)
		, n_possible_(possible_.size())
		, pat_(pat)
	{
		for (size_t dir = 0; dir < 4; ++dir) {
			auto& arr = n_compatible_[dir];
			arr.resize(pat.edges.size());
			for (size_t id = 0; id < pat.edges.size(); ++id) {
				arr[id] =
					pat.edges[id][InverseDir(static_cast<Dir>(dir))].size();
			}
		}

		for (double prob : pat.probs) {
			entropy_.sum_p += prob;

			double log = std::log(prob);
			entropy_.sum_plogp += prob * log;
		}

		double log_sum = std::log(entropy_.sum_p);
		entropy_.val = -entropy_.sum_plogp / entropy_.sum_p + log_sum;
	}

	size_t RandomState(double rnd) const {
		assert(n_possible_ >= 1);
		rnd *= entropy_.sum_p;

		size_t poss_idx = -1;
		for (size_t i = 0; i < possible_.size() && rnd > 0; ++i) {
			if (possible_[i]) {
				poss_idx = i;
				rnd -= pat_.probs[i];
			}
		}
		assert(poss_idx != -1 && "sanity check");

		return poss_idx;
	}

	bool Observe(size_t id) {
		assert(!observed_);
		observed_ = true;

		for (auto& row : n_compatible_) {
			for (auto& elem : row) {
				elem = 0;
			}
		}

		std::vector<size_t> eliminated;
		eliminated.reserve(n_possible_ - 1);

		for (size_t i = 0; i < possible_.size(); ++i) {
			if (possible_[i] && (i != id)) {
				eliminated.push_back(i);
				possible_[i] = false;
			}
		}
		n_possible_ = 1;
		entropy_.sum_p = pat_.probs[id];
		entropy_.val = 0;

		for (auto id : eliminated) {
			if (!PropagateToNeighbors(id)) {
				return false;
			}
		}

		return true;
	}

	double GetEntropy() const {
		return entropy_.val;
	}

	bool IsObserved() const {
		return observed_;
	}

	void SetNeighbor(Dir dir, Cell* nb) {
		neighbors_[dir] = nb;
	}

	bool PropagateInitial() {
		for (size_t dir = 0; dir < 4; ++dir) {
			if (!neighbors_[InverseDir(static_cast<Dir>(dir))]) {
				continue;
			}
			for (size_t id = 0; id < n_compatible_[dir].size(); ++id) {
				if (possible_[id] && n_compatible_[dir][id] <= 0) {
					if (!RemoveFromState(id)) {
						return false;
					}
				}
			}
		}
		return true;
	}

private:
	bool PropagateToNeighbors(size_t tile_id) {
		for (size_t dir = 0; dir < 4; ++dir) {
			if (neighbors_[dir]) {
				if (!neighbors_[dir]->PropagateToNeighbor(
						static_cast<Dir>(dir), tile_id)) {
					return false;
				}
			}
		}
		return true;
	}

	bool PropagateToNeighbor(Dir dir, size_t tile_id) {
		const auto& compat_list = pat_.edges[tile_id][dir];

		for (size_t id : compat_list) {
			if (!--n_compatible_[dir][id]) {
				if (!possible_[id]) {
					continue;
				}

				// The ID id has just become impossible for the cell.
				if (!RemoveFromState(id)) {
					return false;
				}
			}
		}

		return true;
	}

	bool RemoveFromState(size_t id) {
		assert(possible_[id]);
		possible_[id] = false;
		--n_possible_;

		if (n_possible_ == 0) {
			return false;
		}

		if (n_possible_ != 1) {
			double id_p = pat_.probs[id];
			double id_logp = std::log(id_p);
			entropy_.sum_p -= id_p;
			entropy_.sum_plogp -= id_p * id_logp;

			double log_sum = std::log(entropy_.sum_p);
			entropy_.val = -entropy_.sum_plogp / entropy_.sum_p + log_sum;
		} else {
			entropy_.val = 0;
		}

		if (!PropagateToNeighbors(id)) {
			return false;
		}
		return true;
	}

	std::array<std::vector<int32_t>, 4> n_compatible_;
	std::vector<bool> possible_;
	uint32_t n_possible_;

	std::array<Cell*, 4> neighbors_ = {};

	const Pattern& pat_;

	struct {
		double val = 0.0;

		// Most run-time of the algorithm is entropy recalculation.
		// It's O(1) per pattern elimination if these values are kept.
		double sum_plogp = 0.0;
		double sum_p = 0.0;
	} entropy_;

	bool observed_ = false;
};

class Wfc {
public:
	Wfc(const Pattern& pat)
		: pat_(pat)
	{
	}

	std::optional<Wave> Observe(size_t n, size_t m) {
		assert(n > 0 && m > 0);
		Wave res(n, std::vector<size_t>(m));
		std::vector<std::vector<Cell>> cells(n, std::vector<Cell>(m, pat_));
		for (size_t i = 0; i < n; ++i) {
			for (size_t j = 0; j < m; ++j) {
				for (size_t dir = 0; dir < 4; ++dir) {
					size_t i2 = i + kDirDn[dir];
					size_t j2 = j + kDirDm[dir];
					if (i2 == -1 || i2 == n) {
						continue;
					}
					if (j2 == -1 || j2 == m) {
						continue;
					}
					cells[i][j].SetNeighbor(static_cast<Dir>(dir),
							&cells[i2][j2]);
				}
			}
		}
		for (size_t i = 0; i < n; ++i) {
			for (size_t j = 0; j < m; ++j) {
				if (!cells[i][j].PropagateInitial()) {
					return std::nullopt;
				}
			}
		}

		size_t unobserved = n * m;
		std::uniform_real_distribution<double> dist(0, 1);
		std::mt19937 gen((std::random_device())());

		int cnt = 0;
		while (unobserved--) {
			double min_entropy = std::numeric_limits<double>::max();

			size_t argmini;
			size_t argminj;

			for (size_t i = 0; i < n; ++i) {
				for (size_t j = 0; j < m; ++j) {
					if (!cells[i][j].IsObserved() &&
							cells[i][j].GetEntropy() <= min_entropy) {
						double noize = 1e-12 * dist(gen);
						min_entropy = cells[i][j].GetEntropy() + noize;
						// min_entropy = cells[i][j].GetEntropy();
						argmini = i;
						argminj = j;
					}
				}
			}

			Cell& argmin = cells[argmini][argminj];

			size_t state = argmin.RandomState(dist(gen));
			if (!argmin.Observe(state)) {
				return std::nullopt;
			}

			res[argmini][argminj] = state;
		}
		
		return res;
	}

private:
	const Pattern& pat_;
};

}  // namespace

std::optional<Wave> Collapse(const Pattern& pat, size_t n, size_t m) {
	Wfc wfc(pat);
	return wfc.Observe(n, m);
}

}  // namespace wfc

