#include "enginepch.h"
#include "AssetSourceHash.h"

#include <array>
#include <cstdint>
#include <fstream>

namespace {
	constexpr std::array<uint32_t, 64> RoundConstants = {
		0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
		0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
		0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
		0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
		0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
		0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
		0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
		0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
	};

	constexpr uint32_t RotateRight(uint32_t value, uint32_t bits) {
		return (value >> bits) | (value << (32u - bits));
	}

	class Sha256 final {
	public:
		void Update(const uint8_t* data, size_t size) {
			m_TotalBytes += size;
			for (size_t index = 0; index < size; ++index) {
				m_Buffer[m_BufferSize++] = data[index];
				if (m_BufferSize == m_Buffer.size()) {
					Transform();
					m_BufferSize = 0;
				}
			}
		}

		std::array<uint8_t, 32> Finalize() {
			m_Buffer[m_BufferSize++] = 0x80;
			if (m_BufferSize > 56) {
				while (m_BufferSize < m_Buffer.size()) {
					m_Buffer[m_BufferSize++] = 0;
				}
				Transform();
				m_BufferSize = 0;
			}
			while (m_BufferSize < 56) {
				m_Buffer[m_BufferSize++] = 0;
			}

			const uint64_t bitLength = m_TotalBytes * 8;
			for (size_t index = 0; index < 8; ++index) {
				m_Buffer[63 - index] = static_cast<uint8_t>(bitLength >> (index * 8));
			}
			Transform();

			std::array<uint8_t, 32> digest{};
			for (size_t stateIndex = 0; stateIndex < m_State.size(); ++stateIndex) {
				for (size_t byteIndex = 0; byteIndex < 4; ++byteIndex) {
					digest[stateIndex * 4 + byteIndex] = static_cast<uint8_t>(m_State[stateIndex] >> ((3 - byteIndex) * 8));
				}
			}
			return digest;
		}

	private:
		void Transform() {
			std::array<uint32_t, 64> words{};
			for (size_t index = 0; index < 16; ++index) {
				const size_t offset = index * 4;
				words[index] =
					(static_cast<uint32_t>(m_Buffer[offset]) << 24) |
					(static_cast<uint32_t>(m_Buffer[offset + 1]) << 16) |
					(static_cast<uint32_t>(m_Buffer[offset + 2]) << 8) |
					static_cast<uint32_t>(m_Buffer[offset + 3]);
			}
			for (size_t index = 16; index < words.size(); ++index) {
				const uint32_t s0 = RotateRight(words[index - 15], 7) ^ RotateRight(words[index - 15], 18) ^ (words[index - 15] >> 3);
				const uint32_t s1 = RotateRight(words[index - 2], 17) ^ RotateRight(words[index - 2], 19) ^ (words[index - 2] >> 10);
				words[index] = words[index - 16] + s0 + words[index - 7] + s1;
			}

			uint32_t a = m_State[0];
			uint32_t b = m_State[1];
			uint32_t c = m_State[2];
			uint32_t d = m_State[3];
			uint32_t e = m_State[4];
			uint32_t f = m_State[5];
			uint32_t g = m_State[6];
			uint32_t h = m_State[7];

			for (size_t index = 0; index < words.size(); ++index) {
				const uint32_t sum1 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
				const uint32_t choose = (e & f) ^ (~e & g);
				const uint32_t temporary1 = h + sum1 + choose + RoundConstants[index] + words[index];
				const uint32_t sum0 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
				const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
				const uint32_t temporary2 = sum0 + majority;

				h = g;
				g = f;
				f = e;
				e = d + temporary1;
				d = c;
				c = b;
				b = a;
				a = temporary1 + temporary2;
			}

			m_State[0] += a;
			m_State[1] += b;
			m_State[2] += c;
			m_State[3] += d;
			m_State[4] += e;
			m_State[5] += f;
			m_State[6] += g;
			m_State[7] += h;
		}

		std::array<uint32_t, 8> m_State = {
			0x6a09e667u,
			0xbb67ae85u,
			0x3c6ef372u,
			0xa54ff53au,
			0x510e527fu,
			0x9b05688cu,
			0x1f83d9abu,
			0x5be0cd19u
		};
		std::array<uint8_t, 64> m_Buffer{};
		size_t m_BufferSize = 0;
		uint64_t m_TotalBytes = 0;
	};

	HE::ResultEnvelope MakeHashFailure(
		const std::filesystem::path& sourcePath,
		std::string code,
		std::string message) {
		auto result = HE::ResultEnvelope::Failure("asset.source_hash", sourcePath.generic_string(), message);
		result.AddDetail({ HE::DiagnosticSeverity::Error, std::move(code), std::move(message), sourcePath.generic_string() });
		return result;
	}
}

namespace HE {
	ResultEnvelope ComputeAssetSourceHash(
		const std::filesystem::path& sourcePath,
		std::string& outHash) {
		outHash.clear();
		std::ifstream stream(sourcePath, std::ios::in | std::ios::binary);
		if (!stream.good()) {
			return MakeHashFailure(sourcePath, "asset.source_hash.open_failed", "Failed to open asset source for hashing");
		}

		Sha256 sha256;
		std::array<uint8_t, 64 * 1024> buffer{};
		while (stream) {
			stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
			const auto bytesRead = stream.gcount();
			if (bytesRead > 0) {
				sha256.Update(buffer.data(), static_cast<size_t>(bytesRead));
			}
		}
		if (!stream.eof()) {
			return MakeHashFailure(sourcePath, "asset.source_hash.read_failed", "Failed to read asset source for hashing");
		}

		constexpr char HexDigits[] = "0123456789abcdef";
		const auto digest = sha256.Finalize();
		outHash.resize(digest.size() * 2);
		for (size_t index = 0; index < digest.size(); ++index) {
			outHash[index * 2] = HexDigits[digest[index] >> 4];
			outHash[index * 2 + 1] = HexDigits[digest[index] & 0x0f];
		}

		auto result = ResultEnvelope::Success("asset.source_hash", sourcePath.generic_string(), "Asset source hash computed");
		result.SetPayloadValue("sha256", outHash);
		return result;
	}
}
