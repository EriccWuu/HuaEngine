#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace HE::Refl {
    struct TypeGuid {
        uint64_t High = 0;
        uint64_t Low = 0;

        constexpr explicit operator bool() const noexcept { return High != 0 || Low != 0; }
        constexpr auto operator<=>(const TypeGuid&) const = default;

        static TypeGuid FromString(std::string_view text) noexcept {
            if (text.size() != 32 && text.size() != 36) return {};
            TypeGuid result;
            size_t digit = 0;
            for (size_t index = 0; index < text.size(); ++index) {
                if (text.size() == 36 && (index == 8 || index == 13 || index == 18 || index == 23)) {
                    if (text[index] != '-') return {};
                    continue;
                }
                const char value = text[index];
                const int hex = value >= '0' && value <= '9' ? value - '0' :
                    (value >= 'a' && value <= 'f' ? value - 'a' + 10 :
                     (value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1));
                if (hex < 0) return {};
                auto& half = digit++ < 16 ? result.High : result.Low;
                half = (half << 4) | static_cast<uint64_t>(hex);
            }
            return digit == 32 ? result : TypeGuid{};
        }

        static constexpr TypeGuid FromName(std::string_view name) noexcept {
            uint64_t high = 14695981039346656037ULL;
            uint64_t low = 7809847782465536322ULL;
            for (const unsigned char value : name) {
                high = (high ^ value) * 1099511628211ULL;
                low = (low ^ value) * 14029467366897019727ULL;
            }
            return name.empty() ? TypeGuid{} : TypeGuid{high, low};
        }
    };

    [[nodiscard]] inline std::string ToString(TypeGuid guid) {
        constexpr char digits[] = "0123456789abcdef";
        std::string text(32, '0');
        for (int index = 31; index >= 0; --index) {
            auto& half = index >= 16 ? guid.Low : guid.High;
            text[static_cast<size_t>(index)] = digits[half & 15];
            half >>= 4;
        }
        return text;
    }

    struct TypeGuidHash {
        size_t operator()(TypeGuid guid) const noexcept {
            const uint64_t mixed = guid.High ^ (guid.Low + 0x9e3779b97f4a7c15ULL + (guid.High << 6) + (guid.High >> 2));
            if constexpr (sizeof(size_t) < sizeof(uint64_t)) return static_cast<size_t>(mixed ^ (mixed >> 32));
            return static_cast<size_t>(mixed);
        }
    };
}
