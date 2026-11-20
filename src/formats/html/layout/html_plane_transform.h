#pragma once

#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>

#include <array>
#include <optional>
#include <vector>

namespace dockit {
	// A mapping between planes. The last row computes the homogeneous divisor.
	class HTMLPlaneTransform {
		godot::Basis matrix;
		std::vector<godot::Vector2> project_polygon(const godot::Vector2* p_points, std::size_t p_count) const;

	public:
		HTMLPlaneTransform() = default;
		HTMLPlaneTransform(const godot::Transform2D& p_affine);
		explicit HTMLPlaneTransform(const godot::Basis& p_matrix) : matrix(p_matrix) {}

		const godot::Vector3& row(int p_index) const { return matrix[p_index]; }
		godot::real_t determinant() const { return matrix.determinant(); }
		bool is_affine() const;
		godot::Transform2D affine_at(const godot::Vector2& p_point) const;
		std::optional<HTMLPlaneTransform> try_inverse() const;
		HTMLPlaneTransform inverse() const;
		HTMLPlaneTransform operator*(const HTMLPlaneTransform& p_other) const;
		bool operator==(const HTMLPlaneTransform& p_other) const { return matrix == p_other.matrix; }
		bool operator!=(const HTMLPlaneTransform& p_other) const { return !(*this == p_other); }

		godot::Vector2 xform(const godot::Vector2& p_point) const;
		godot::Rect2 xform(const godot::Rect2& p_rect) const;
		std::vector<godot::Vector2> project_polygon(const std::vector<godot::Vector2>& p_points) const {
			return project_polygon(p_points.data(), p_points.size());
		}
		template<std::size_t Size>
		std::vector<godot::Vector2> project_polygon(const std::array<godot::Vector2, Size>& p_points) const {
			return project_polygon(p_points.data(), p_points.size());
		}
	};
}
