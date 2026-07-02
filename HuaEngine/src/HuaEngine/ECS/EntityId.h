#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace HE {
	struct EntityId {
		uint32_t Index = 0;
		uint32_t Generation = 0;

		constexpr explicit operator bool() const {
			return Generation != 0;
		}
	};

	constexpr bool operator==(EntityId lhs, EntityId rhs) {
		return lhs.Index == rhs.Index && lhs.Generation == rhs.Generation;
	}

	constexpr bool operator!=(EntityId lhs, EntityId rhs) {
		return !(lhs == rhs);
	}

	struct EntityUuid {
		uint64_t High = 0;
		uint64_t Low = 0;

		static EntityUuid FromString(std::string_view text);
	};

	constexpr bool operator==(EntityUuid lhs, EntityUuid rhs) {
		return lhs.High == rhs.High && lhs.Low == rhs.Low;
	}

	constexpr bool operator!=(EntityUuid lhs, EntityUuid rhs) {
		return !(lhs == rhs);
	}

	inline int HexValue(char c) {
		if (c >= '0' && c <= '9') {
			return c - '0';
		}
		if (c >= 'a' && c <= 'f') {
			return c - 'a' + 10;
		}
		if (c >= 'A' && c <= 'F') {
			return c - 'A' + 10;
		}
		return -1;
	}

	inline EntityUuid EntityUuid::FromString(std::string_view text) {
		if (text.size() != 32) {
			return {};
		}

		EntityUuid uuid;
		for (size_t index = 0; index < text.size(); ++index) {
			const int value = HexValue(text[index]);
			if (value < 0) {
				return {};
			}

			uint64_t& half = index < 16 ? uuid.High : uuid.Low;
			half = (half << 4) | static_cast<uint64_t>(value);
		}

		return uuid;
	}

	inline std::string ToString(EntityUuid uuid) {
		constexpr char HexDigits[] = "0123456789abcdef";

		std::string text(32, '0');
		for (int index = 15; index >= 0; --index) {
			text[static_cast<size_t>(index)] = HexDigits[uuid.High & 0x0f];
			uuid.High >>= 4;
		}
		for (int index = 31; index >= 16; --index) {
			text[static_cast<size_t>(index)] = HexDigits[uuid.Low & 0x0f];
			uuid.Low >>= 4;
		}

		return text;
	}
}

namespace std {
	template<>
	struct hash<HE::EntityId> {
		size_t operator()(HE::EntityId id) const noexcept {
			return (static_cast<size_t>(id.Index) << 32) ^ static_cast<size_t>(id.Generation);
		}
	};

	template<>
	struct hash<HE::EntityUuid> {
		size_t operator()(HE::EntityUuid uuid) const noexcept {
			const auto highHash = hash<uint64_t>{}(uuid.High);
			const auto lowHash = hash<uint64_t>{}(uuid.Low);
			return highHash ^ (lowHash + 0x9e3779b97f4a7c15ULL + (highHash << 6) + (highHash >> 2));
		}
	};
}
