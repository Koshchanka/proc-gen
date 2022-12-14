#include <iostream>

#include "encoder.h"
#include "image.h"
#include "wfc.h"

int main() {
	size_t N = 75;
	size_t M = 100;
	size_t k = 3;

	auto img = wfc::ReadPng("example/knot.png");

	wfc::MatrixEncoder<wfc::RGB> enc;
	auto pat = enc.Fit(img, k, true, true, true);

	while (true) {
		auto res = wfc::Collapse(pat, N - k + 1, M - k + 1);
		if (!res) {
			std::cerr << "Retrying\n";
			continue;
		}

		auto decoded = enc.Decode(*res);
		wfc::WritePng(decoded, "example/trick.out.png");

		break;
	}
}

