#pragma once

namespace HE::Rendering {
	class CommandList;

	class RenderDevice {
	public:
		virtual ~RenderDevice() = default;

		virtual CommandList& GetImmediateCommandList() = 0;
	};
}
