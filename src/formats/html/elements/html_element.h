#pragma once

#include "formats/html/dom/html_node.h"
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <memory>

namespace litehtml {
	class element;
}

namespace dockit { class HTMLDOM; }

class HTMLLayout;
class HTMLInputElement;
class HTMLFormElement;
class HTMLDocumentFragment;
class HTMLAnimation;

class HTMLShadowRoot;

class HTMLElement : public HTMLNode {
	GDCLASS(HTMLElement, HTMLNode)

	friend class HTMLLayout;
	friend class dockit::HTMLDOM;
	friend class HTMLImageElement;
	friend class HTMLInputElement;
	friend class HTMLTextAreaElement;
	friend class HTMLTemplateElement;
	friend class ::HTMLLabelCanvas;
	friend class ::HTMLLabel;
	friend class HTMLDocumentFragment;

protected:
	static void _bind_methods();
	std::shared_ptr<litehtml::element> lock_related_element(const godot::Ref<HTMLElement>& p_element) const;
	godot::Ref<HTMLFormElement> get_form_owner() const;

public:
	godot::Ref<HTMLShadowRoot> attach_shadow(const godot::String& p_mode = "open", const godot::String& p_assignment = "named", bool p_delegates_focus = false);
	godot::Ref<HTMLShadowRoot> get_shadow_root() const;

	godot::Error set_pointer_capture(int64_t p_pointer_id);
	godot::Error release_pointer_capture(int64_t p_pointer_id);
	bool has_pointer_capture(int64_t p_pointer_id) const;
	godot::TypedArray<HTMLAnimation> get_animations(); // 返回以该元素为目标的动画
	godot::Ref<HTMLAnimation> animate(const godot::TypedArray<godot::Dictionary>& p_keyframes,
		const godot::Dictionary& p_options = {}); // 在该元素上创建并播放关键帧动画
	godot::String get_style_property(const godot::String& p_name) const; // 读取内联 CSS 属性值
	godot::String get_style_property_priority(const godot::String& p_name) const; // 读取内联属性的优先级
	godot::Dictionary get_style_properties() const; // 返回内联声明的快照
	godot::Error set_style_property(const godot::String& p_name, const godot::String& p_value,
		const godot::String& p_priority = ""); // 设置内联 CSS 属性及其优先级
	godot::String remove_style_property(const godot::String& p_name); // 删除内联 CSS 属性并返回旧值

	NodeType get_node_type() const override { return ELEMENT_NODE; } // 返回元素节点类型
	bool is_content_editable() const;
	godot::String get_tag_name() const;												// 返回小写 HTML 标签名
	bool has_attribute(const godot::String& p_name) const;							// 检查指定属性是否存在，空属性也视为存在
	godot::String get_attribute(
		const godot::String& p_name,
		const godot::String& p_default_value = {}
	) const;																		// 读取属性值，缺失时返回默认值
	godot::String get_text_content() const;											// 按文档顺序读取后代文本，跳过注释和 CSS 生成内容
	godot::Ref<HTMLElement> get_parent() const;										// 返回父元素，没有父元素时返回 null
	godot::TypedArray<HTMLElement> get_children() const;							// 按文档顺序返回直接子元素
	godot::TypedArray<godot::Rect2> get_client_rects() const;						// 返回各排版片段在文档坐标中的边框盒
	godot::Rect2 get_bounding_rect() const;											// 返回包围所有排版片段的矩形
	godot::Vector2 get_client_size() const; // 返回包括内边距的布局尺寸
	godot::Vector2 get_client_offset() const; // 返回边框左侧和顶部宽度
	godot::Ref<HTMLElement> get_offset_parent() const; // 返回未变换布局偏移的参照元素
	godot::Rect2 get_offset_rect() const; // 返回相对参照元素内边距盒的未变换矩形
	godot::Vector2 get_scroll_size() const; // 返回元素可滚动内容的尺寸
	godot::Vector2 get_scroll_offset() const; // 返回内层滚动容器的当前偏移
	godot::Error set_scroll_offset(const godot::Vector2& p_offset); // 设置并限制内层滚动偏移
	godot::Error scroll_to(const godot::Vector2& p_offset); // 滚动到内层容器的指定偏移
	godot::Error scroll_by(const godot::Vector2& p_delta); // 将内层容器滚动指定距离
	godot::TypedArray<godot::Dictionary> get_css_diagnostics() const;				// 返回本元素的 CSS 声明诊断快照

	// 同一布局内的同一元素复用仍在使用的句柄，重新解析后旧句柄失效
	godot::Ref<HTMLElement> query_selector(const godot::String& p_selector) const;	// 返回第一个匹配的后代元素，:scope 指向当前元素
	godot::TypedArray<HTMLElement> query_selector_all(
		const godot::String& p_selector
	) const;																		// 按文档顺序返回所有匹配后代的数组快照
	bool matches(const godot::String& p_selector) const;							// 检查当前元素是否匹配选择器
	godot::Ref<HTMLElement> closest(const godot::String& p_selector) const;			// 从当前元素起向父级查找，返回第一个匹配项
	godot::Dictionary get_attributes() const;										// 返回全部属性名称和值的字典快照
	godot::Dictionary get_dataset() const;											// 返回 data-* 属性的字典快照，将 data-item-id 映射为 itemId
	godot::PackedStringArray get_class_list() const;								// 返回按 class 属性顺序排列的类名快照
	bool has_class(const godot::String& p_class_name) const;						// 检查当前元素是否包含指定类名

	// 修改作用于当前布局实例，同帧更新合并重排
	godot::Error set_text_content(const godot::String& p_text);						// 将子内容替换为纯文本并使原有子元素的句柄失效
	godot::Error set_attribute(
		const godot::String& p_name,
		const godot::String& p_value
	);																				// 添加或更新指定属性
	godot::Error remove_attribute(const godot::String& p_name);						// 移除指定属性
	godot::Error add_class(const godot::String& p_class_name);						// 添加指定类名，重复添加时保留一个
	godot::Error remove_class(const godot::String& p_class_name);					// 移除指定类名
	godot::Error toggle_class(
		const godot::String& p_class_name,
		const godot::Variant& p_force = {}
	);																				// 切换指定类名，可传入布尔值指定是否保留该类名

	// HTML 片段和元素树编辑
	godot::String get_inner_html() const;											// 将当前元素的子内容序列化为 HTML 片段
	godot::Error set_inner_html(const godot::String& p_html);						// 将 HTML 片段解析为新的子内容并使原有子元素的句柄失效
	godot::Error append_child(const godot::Ref<HTMLElement>& p_child);				// 将同一布局中的新元素或已有元素追加到当前元素的子元素末尾
	godot::Error insert_before(const godot::Ref<HTMLElement>& p_child,
		const godot::Ref<HTMLElement>& p_reference = {});							// 将元素插到指定直接子元素前，省略参照元素时追加到末尾
	godot::Error append_fragment(const godot::Ref<HTMLDocumentFragment>& p_fragment);	// 将片段的全部子元素移到末尾并清空片段
	godot::Error insert_fragment_before(const godot::Ref<HTMLDocumentFragment>& p_fragment,
		const godot::Ref<HTMLElement>& p_reference = {});							// 将片段的全部子元素移到指定直接子元素前，省略时追加到末尾
	godot::Error replace_child(const godot::Ref<HTMLElement>& p_new_child,
		const godot::Ref<HTMLElement>& p_old_child);								// 将新元素移到原子元素的位置并保留移出子树的句柄
	godot::Error remove_child(const godot::Ref<HTMLElement>& p_child);				// 分离指定直接子元素并保留其子树句柄
	godot::Ref<HTMLElement> clone_node(bool p_deep = false) const;					// 创建同一布局中的分离副本，可选复制后代
	godot::Error detach();															// 从父元素分离当前子树并保留已有句柄
	godot::Error remove();															// 从父元素中移除当前元素，使该元素及其后代的句柄失效
	godot::Ref<HTMLElement> get_first_element_child() const; // 返回第一个直接子元素
	godot::Ref<HTMLElement> get_last_element_child() const; // 返回最后一个直接子元素
	godot::Ref<HTMLElement> get_previous_element_sibling() const; // 返回上一个同父元素
	godot::Ref<HTMLElement> get_next_element_sibling() const; // 返回下一个同父元素
	int64_t get_child_element_count() const; // 返回直接子元素数量
	godot::TypedArray<HTMLElement> get_elements_by_tag_name(const godot::String& p_name) const; // 按标签名查找后代元素
	godot::TypedArray<HTMLElement> get_elements_by_class_name(const godot::String& p_names) const; // 按类名查找后代元素
	godot::PackedStringArray get_attribute_names() const; // 返回源属性名称快照
	bool has_attributes() const; // 检查是否存在源属性
	bool toggle_attribute(const godot::String& p_name, const godot::Variant& p_force = {}); // 切换属性并返回切换后的存在状态
	bool replace_class(const godot::String& p_old_name, const godot::String& p_new_name); // 替换类名并保留顺序
	godot::Error set_dataset_value(const godot::String& p_key, const godot::String& p_value); // 设置对应的 data 属性
	godot::Error remove_dataset_value(const godot::String& p_key); // 移除对应的 data 属性
	godot::Error append(const godot::Array& p_items); // 将一批节点、文本或片段插到末尾
	godot::Error prepend(const godot::Array& p_items); // 将一批节点、文本或片段插到开头
	godot::Error before(const godot::Array& p_items); // 将一批节点、文本或片段插到当前元素前
	godot::Error after(const godot::Array& p_items); // 将一批节点、文本或片段插到当前元素后
	godot::Error replace_with(const godot::Array& p_items); // 用一批节点、文本或片段替换当前元素
	godot::Error replace_children(const godot::Array& p_items); // 替换子节点并保留移出的节点句柄
	godot::Error insert_adjacent_element(const godot::String& p_position, const godot::Ref<HTMLElement>& p_element); // 在指定的相邻位置插入元素
	godot::Error insert_adjacent_text(const godot::String& p_position, const godot::String& p_text); // 在指定的相邻位置插入纯文本
	godot::Error insert_adjacent_html(const godot::String& p_position, const godot::String& p_html); // 在指定的相邻位置插入解析后的片段
	godot::String get_outer_html() const; // 序列化自身和子树
	godot::Error set_outer_html(const godot::String& p_html); // 解析标记并替换当前元素
	godot::String get_inner_text() const; // 返回受样式影响的文本和结构换行
	godot::Error set_inner_text(const godot::String& p_text); // 用文本和换行替换子节点
	godot::String get_id() const; // 读取元素 ID
	void set_id(const godot::String& p_value); // 修改元素 ID
	godot::String get_css_class_name() const; // 读取原始类名
	void set_css_class_name(const godot::String& p_value); // 修改原始类名
	godot::String get_lang() const; // 读取语言标签
	void set_lang(const godot::String& p_value); // 修改语言标签
	godot::String get_dir() const; // 读取文字方向
	void set_dir(const godot::String& p_value); // 修改文字方向
	godot::String get_title() const; // 读取提示标题
	void set_title(const godot::String& p_value); // 修改提示标题
	bool get_hidden() const; // 读取隐藏属性
	void set_hidden(bool p_value); // 修改隐藏属性
	bool get_inert() const; // 读取惰性属性
	void set_inert(bool p_value); // 修改惰性属性
	int64_t get_tab_index() const; // 读取Tab 顺序
	void set_tab_index(int64_t p_value); // 修改Tab 顺序
};
