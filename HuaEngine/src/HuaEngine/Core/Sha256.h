#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace HE {
	using Sha256Digest = std::array<uint8_t, 32>;

	Sha256Digest ComputeSha256(std::span<const uint8_t> bytes);
	std::string Sha256ToHex(const Sha256Digest& digest);
	uint64_t Sha256Prefix64(const Sha256Digest& digest);
}
