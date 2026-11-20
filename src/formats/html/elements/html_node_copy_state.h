#pragma once

namespace litehtml {
	class element;
}

namespace dockit {
	enum class HTMLNodeCopyMode { COPY, ADOPT };

	class HTMLNodeCopyState {
	public:
		virtual ~HTMLNodeCopyState() = default;
		virtual bool copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode p_mode) const = 0; // 将运行时状态复制到同类型节点
	};
}
