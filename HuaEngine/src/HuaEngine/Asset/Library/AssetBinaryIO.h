#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace HE {
	struct AssetBinaryReaderLimits {
		size_t MaxStringBytes = 16 * 1024 * 1024;
		size_t MaxBlobBytes = 1024 * 1024 * 1024;
	};

	class AssetBinaryWriter {
	public:
		void WriteU8(uint8_t value);
		void WriteU32(uint32_t value);
		void WriteU64(uint64_t value);
		void WriteFloat(float value);
		void WriteString(std::string_view value);
		void WriteBytes(std::span<const uint8_t> bytes);
		void WriteBytes(std::initializer_list<uint8_t> bytes);

		[[nodiscard]] const std::vector<uint8_t>& GetData() const { return m_Data; }
		[[nodiscard]] std::vector<uint8_t> TakeData() { return std::move(m_Data); }

	private:
		std::vector<uint8_t> m_Data;
	};

	class AssetBinaryReader {
	public:
		explicit AssetBinaryReader(
			std::span<const uint8_t> data,
			AssetBinaryReaderLimits limits = {});
		explicit AssetBinaryReader(
			const std::vector<uint8_t>& data,
			AssetBinaryReaderLimits limits = {});

		bool ReadU8(uint8_t& value);
		bool ReadU32(uint32_t& value);
		bool ReadU64(uint64_t& value);
		bool ReadFloat(float& value);
		bool ReadString(std::string& value);
		bool ReadBytes(size_t byteCount, std::vector<uint8_t>& value);

		[[nodiscard]] bool Failed() const { return m_Failed; }
		[[nodiscard]] size_t Remaining() const;

	private:
		[[nodiscard]] bool CanRead(size_t byteCount);

		std::span<const uint8_t> m_Data;
		AssetBinaryReaderLimits m_Limits;
		size_t m_Offset = 0;
		bool m_Failed = false;
	};
}
