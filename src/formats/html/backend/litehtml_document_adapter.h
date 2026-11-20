#pragma once

#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <litehtml/document.h>
#include <litehtml/element.h>

#include <array>

namespace dockit {
	class LiteHTMLContainer;
}

namespace litehtml {
	struct box_shadow;
	// 分阶段的 litehtml 头文件让这个适配器可以访问原生的文档和元素状态
	struct dockit_editor {
		struct ContainerUpdate {
			std::vector<bool> rule_matches;
			std::vector<std::array<pixel_t, 2>> dimensions;
			bool rules_changed = false;
			bool units_changed = false;
		};

		static document::ptr parse(
			const godot::CharString& p_html,
			dockit::LiteHTMLContainer& p_container,
			const string& p_default_styles,
			const string& p_user_styles
		);																			// 解析 UTF-8 文档并应用初始样式
		static document::ptr parse_svg_clip_source(const godot::String& p_svg,
			dockit::LiteHTMLContainer& p_container);								// 解析 XML 裁剪源并计算独立的样式
		static bool media_changed(document& p_document);							// 检查媒体条件的匹配结果是否改变
		static void set_viewport_unit_size(document& p_document, const godot::Vector2i& p_size); // 设置视口百分比单位的参考尺寸
		static void compute_styles(document& p_document);							// 按文档顺序更新计算样式与计数器
		static void rebuild_render_tree(document& p_document);						// 用现有元素和样式重新建立渲染树
		static void push_modal(document& p_document, const element::ptr& p_element); // 将对话框加入文档顶层
		static void prune_modals(document& p_document); // 移除已关闭或离开文档的对话框
		static void remember_content_sizes(document& p_document);					// 记录参加排版的元素的自动固有尺寸
		static void prune_unused_fonts(document& p_document);						// 回收当前文档和渲染树均未引用的字体
		static ContainerUpdate measure_containers(document& p_document);			// 更新容器内容盒尺寸并检测查询和单位变化
		static std::array<overflow, 2> propagate_viewport_overflow(document& p_document); // 应用文档视口的 overflow 并更新来源元素的使用值
		static void restyle(
			const document::ptr& p_document,
			dockit::LiteHTMLContainer& p_container,
			const string& p_default_styles,
			const string& p_user_styles,
			bool p_build_render_tree = true
		);																			// 在原有元素树上重算样式并重建渲染树
		static unsigned selector_state_dependencies(const document& p_document);	// 收集已解析样式表使用的交互状态
		static bool has_transitions(document& p_document); // 检查样式表或元素是否声明了 CSS 过渡
		static void set_hover(element& p_element, bool p_hovered);					// 更新元素的悬停伪类状态
		static bool is_command_button(const element& p_element);					// 判断元素是否有按钮激活行为
		static css_text::vector stylesheets(const document& p_document);
		static void edit_stylesheet(document& p_document, const element::ptr& p_owner, string p_text);
		static void reset_stylesheet_source(element& p_owner);

		static bool set_attribute(
			element& p_element,
			const char* p_name,
			const char* p_value
		);																			// 更新属性，空指针表示移除属性
		static elements_list parse_fragment(
			document& p_document,
			const element& p_parent,
			const godot::CharString& p_html
		);																			// 按父标签的上下文解析 HTML 片段
		static elements_list create_text(
			const document::ptr& p_document,
			const char* p_text,
			bool p_split
		);																			// 创建纯文本节点，可按排版需要拆分文本
		static void replace_children(element& p_parent, elements_list p_children);	// 替换父元素的子节点
		static void replace_child_lists(std::vector<std::pair<element::ptr, elements_list>>& r_lists); // 在同一次操作中更新多个父节点
		static void mark_text_node(elements_list& r_parts, const char* p_text); // 标记属于同一个 DOM 文本节点的排版片段
		enum class child_edit_result { invalid, unchanged, changed };
		static child_edit_result place_child(const element::ptr& p_child, element& p_parent,
			const element::ptr& p_reference, bool p_replace_reference);			// 插入或替换子节点，保留节点身份并报告是否改变顺序
		static bool insert_children(element& p_source, element& p_parent,
			const element::ptr& p_reference);									// 按顺序移动来源的全部子节点

	private:
		static document::ptr parse_tree(const godot::CharString& p_html, document_container& p_container);
		static void apply_author_styles(const document::ptr& p_document, dockit::LiteHTMLContainer& p_container,
			const string& p_additional_styles);
		static void remove_attribute(html_tag& p_element, const char* p_name);
		static void reset_tree(element& p_element);
		static void trim_white_space(render_item& p_root);
	};
}
