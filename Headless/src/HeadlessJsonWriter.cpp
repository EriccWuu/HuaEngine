#include "enginepch.h"
#include "HeadlessJsonWriter.h"

#include <algorithm>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	std::string EscapeJson(std::string_view value) {
		std::string output;
		output.reserve(value.size() + 8);

		for (const char character : value) {
			switch (character) {
			case '\\':
				output += "\\\\";
				break;
			case '"':
				output += "\\\"";
				break;
			case '\b':
				output += "\\b";
				break;
			case '\f':
				output += "\\f";
				break;
			case '\n':
				output += "\\n";
				break;
			case '\r':
				output += "\\r";
				break;
			case '\t':
				output += "\\t";
				break;
			default:
				if (static_cast<unsigned char>(character) < 0x20) {
					static constexpr char Hex[] = "0123456789abcdef";
					output += "\\u00";
					output += Hex[(character >> 4) & 0x0f];
					output += Hex[character & 0x0f];
				}
				else {
					output += character;
				}
				break;
			}
		}

		return output;
	}
}

namespace HE::Headless {
	std::string RenderJson(const HeadlessCommandResponse& response) {
		std::ostringstream stream;
		stream << "{";
		stream << "\"host\":\"huaengine-headless\",";
		stream << "\"result\":{";
		stream << "\"operation\":\"" << EscapeJson(response.Result.Operation) << "\",";
		stream << "\"target\":\"" << EscapeJson(response.Result.Target) << "\",";
		stream << "\"status\":\"" << EscapeJson(ToString(response.Result.Status)) << "\",";
		stream << "\"summary\":\"" << EscapeJson(response.Result.Summary) << "\",";
		stream << "\"can_continue_automatically\":" << (response.Result.CanContinueAutomatically() ? "true" : "false") << ",";
		stream << "\"requires_manual_intervention\":" << (response.Result.RequiresManualIntervention() ? "true" : "false") << ",";

		stream << "\"payload\":{";
		std::vector<std::pair<std::string, std::string>> payloadEntries(
			response.Result.Payload.begin(),
			response.Result.Payload.end());
		std::sort(payloadEntries.begin(), payloadEntries.end(), [](const auto& left, const auto& right) {
			return left.first < right.first;
		});
		for (size_t index = 0; index < payloadEntries.size(); ++index) {
			if (index > 0) {
				stream << ",";
			}

			stream << "\"" << EscapeJson(payloadEntries[index].first) << "\":";
			stream << "\"" << EscapeJson(payloadEntries[index].second) << "\"";
		}
		stream << "},";

		stream << "\"details\":[";
		for (size_t index = 0; index < response.Result.Details.size(); ++index) {
			if (index > 0) {
				stream << ",";
			}

			const auto& detail = response.Result.Details[index];
			stream << "{";
			stream << "\"severity\":\"" << EscapeJson(ToString(detail.Severity)) << "\",";
			stream << "\"code\":\"" << EscapeJson(detail.Code) << "\",";
			stream << "\"message\":\"" << EscapeJson(detail.Message) << "\",";
			stream << "\"context\":\"" << EscapeJson(detail.Context) << "\"";
			stream << "}";
		}
		stream << "]";
		stream << "}";

		if (!response.Operations.empty()) {
			stream << ",\"data\":{";
			stream << "\"operations\":[";
			for (size_t index = 0; index < response.Operations.size(); ++index) {
				if (index > 0) {
					stream << ",";
				}

				const auto& descriptor = response.Operations[index];
				stream << "{";
				stream << "\"name\":\"" << EscapeJson(descriptor.Name) << "\",";
				stream << "\"domain\":\"" << EscapeJson(ToString(descriptor.Domain)) << "\",";
				stream << "\"summary\":\"" << EscapeJson(descriptor.Summary) << "\"";
				stream << "}";
			}
			stream << "]";
			stream << "}";
		}

		stream << "}";
		return stream.str();
	}

	int ExitCodeFor(const ResultEnvelope& result) {
		switch (result.Status) {
		case OperationStatus::Success:
			return 0;
		case OperationStatus::Failure:
			return 1;
		case OperationStatus::ManualInterventionRequired:
			return 2;
		}

		return 70;
	}
}
