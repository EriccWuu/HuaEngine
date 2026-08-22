#include "enginepch.h"
#include "AssetBinaryIO.h"

#include <bit>
#include <limits>

namespace HE {
	void AssetBinaryWriter::WriteU8(uint8_t value) {
		m_Data.push_back(value);
	}

	void AssetBinaryWriter::WriteU32(uint32_t value) {
		for (uint32_t shift = 0; shift < 32; shift += 8) {
			m_Data.push_back(static_cast<uint8_t>((value >> shift) & 0xffu));
		}
	}

	void AssetBinaryWriter::WriteU64(uint64_t value) {
		for (uint32_t shift = 0; shift < 64; shift += 8) {
			m_Data.push_back(static_cast<uint8_t>((value >> shift) & 0xffull));
		}
	}

	void AssetBinaryWriter::WriteFloat(float value) {
		WriteU32(std::bit_cast<uint32_t>(value));
	}

	void AssetBinaryWriter::WriteString(std::string_view value) {
		HE_CORE_ASSERT(value.size() <= std::numeric_limits<uint32_t>::max(), "Asset binary string is too large");
		WriteU32(static_cast<uint32_t>(value.size()));
		const auto* begin = reinterpret_cast<const uint8_t*>(value.data());
		WriteBytes(std::span<const uint8_t>(begin, value.size()));
	}

	void AssetBinaryWriter::WriteBytes(std::span<const uint8_t> bytes) {
		m_Data.insert(m_Data.end(), bytes.begin(), bytes.end());
	}

	void AssetBinaryWriter::WriteBytes(std::initializer_list<uint8_t> bytes) {
		WriteBytes(std::span<const uint8_t>(bytes.begin(), bytes.size()));
	}

	AssetBinaryReader::AssetBinaryReader(
		std::span<const uint8_t> data,
		AssetBinaryReaderLimits limits)
		: m_Data(data), m_Limits(limits) {}

	AssetBinaryReader::AssetBinaryReader(
		const std::vector<uint8_t>& data,
		AssetBinaryReaderLimits limits)
		: AssetBinaryReader(std::span<const uint8_t>(data.data(), data.size()), limits) {}

	bool AssetBinaryReader::ReadU8(uint8_t& value) {
		if (!CanRead(1)) {
			return false;
		}

		value = m_Data[m_Offset++];
		return true;
	}

	bool AssetBinaryReader::ReadU32(uint32_t& value) {
		if (!CanRead(sizeof(uint32_t))) {
			return false;
		}

		value = 0;
		for (uint32_t shift = 0; shift < 32; shift += 8) {
			value |= static_cast<uint32_t>(m_Data[m_Offset++]) << shift;
		}
		return true;
	}

	bool AssetBinaryReader::ReadU64(uint64_t& value) {
		if (!CanRead(sizeof(uint64_t))) {
			return false;
		}

		value = 0;
		for (uint32_t shift = 0; shift < 64; shift += 8) {
			value |= static_cast<uint64_t>(m_Data[m_Offset++]) << shift;
		}
		return true;
	}

	bool AssetBinaryReader::ReadFloat(float& value) {
		uint32_t bits = 0;
		if (!ReadU32(bits)) {
			return false;
		}

		value = std::bit_cast<float>(bits);
		return true;
	}

	bool AssetBinaryReader::ReadString(std::string& value) {
		uint32_t byteCount = 0;
		if (!ReadU32(byteCount)) {
			return false;
		}
		if (byteCount > m_Limits.MaxStringBytes || !CanRead(byteCount)) {
			m_Failed = true;
			return false;
		}

		const auto* begin = reinterpret_cast<const char*>(m_Data.data() + m_Offset);
		value.assign(begin, byteCount);
		m_Offset += byteCount;
		return true;
	}

	bool AssetBinaryReader::ReadBytes(size_t byteCount, std::vector<uint8_t>& value) {
		if (byteCount > m_Limits.MaxBlobBytes || !CanRead(byteCount)) {
			m_Failed = true;
			return false;
		}

		const auto begin = m_Data.begin() + static_cast<std::ptrdiff_t>(m_Offset);
		value.assign(begin, begin + static_cast<std::ptrdiff_t>(byteCount));
		m_Offset += byteCount;
		return true;
	}

	size_t AssetBinaryReader::Remaining() const {
		return m_Offset <= m_Data.size() ? m_Data.size() - m_Offset : 0;
	}

	bool AssetBinaryReader::CanRead(size_t byteCount) {
		if (m_Failed || byteCount > Remaining()) {
			m_Failed = true;
			return false;
		}
		return true;
	}
}
