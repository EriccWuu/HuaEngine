#pragma once

namespace HE {
	class System {
	public:
		virtual ~System() = default;
		virtual void Update() = 0;
	};
}