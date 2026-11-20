#include "html_plane_transform.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace godot;

namespace dockit {
	HTMLPlaneTransform::HTMLPlaneTransform(const Transform2D& p_affine) :
		matrix(p_affine[0].x, p_affine[1].x, p_affine[2].x, p_affine[0].y, p_affine[1].y, p_affine[2].y, 0, 0, 1) {}

	bool HTMLPlaneTransform::is_affine() const {
		return matrix[2].x == 0 && matrix[2].y == 0 && matrix[2].z > 0;
	}

	Transform2D HTMLPlaneTransform::affine_at(const Vector2& p_point) const {
		const Vector2 x(matrix[0].x, matrix[1].x);
		const Vector2 y(matrix[0].y, matrix[1].y);
		if (is_affine()) {
			const real_t w = matrix[2].z;
			return { x / w, y / w, Vector2(matrix[0].z, matrix[1].z) / w };
		}
		const Vector3 projected = matrix.xform(Vector3(p_point.x, p_point.y, 1));
		if (!projected.is_finite() || projected.z <= 0) return {};
		const Vector2 point = Vector2(projected.x, projected.y) / projected.z;
		const Vector2 dx = (x - point * matrix[2].x) / projected.z;
		const Vector2 dy = (y - point * matrix[2].y) / projected.z;
		return { dx, dy, point - dx * p_point.x - dy * p_point.y };
	}

	std::optional<HTMLPlaneTransform> HTMLPlaneTransform::try_inverse() const {
		const real_t det = determinant();
		if (!matrix.is_finite() || !std::isfinite(det) || det == 0) return std::nullopt;
		const auto result = inverse();
		return result.matrix.is_finite() ? std::optional<HTMLPlaneTransform>(result) : std::nullopt;
	}

	HTMLPlaneTransform HTMLPlaneTransform::inverse() const {
		return HTMLPlaneTransform(matrix.inverse());
	}

	HTMLPlaneTransform HTMLPlaneTransform::operator*(const HTMLPlaneTransform& p_other) const {
		return HTMLPlaneTransform(matrix * p_other.matrix);
	}

	Vector2 HTMLPlaneTransform::xform(const Vector2& p_point) const {
		const Vector3 point = matrix.xform(Vector3(p_point.x, p_point.y, 1));
		if (point.z <= 0) {
			const real_t invalid = std::numeric_limits<real_t>::quiet_NaN();
			return { invalid, invalid };
		}
		return Vector2(point.x, point.y) / point.z;
	}

	Rect2 HTMLPlaneTransform::xform(const Rect2& p_rect) const {
		if (is_affine()) return affine_at(Vector2()).xform(p_rect);
		const Vector2 end = p_rect.get_end();
		const std::array<Vector2, 4> corners{ p_rect.position, Vector2(end.x, p_rect.position.y), end,
			Vector2(p_rect.position.x, end.y) };
		const auto polygon = project_polygon(corners);
		if (polygon.empty()) return {};
		Rect2 bounds(polygon.front(), Vector2());
		for (const auto& point : polygon) bounds = bounds.expand(point);
		return bounds;
	}

	std::vector<Vector2> HTMLPlaneTransform::project_polygon(const Vector2* p_points, std::size_t p_count) const {
		std::vector<Vector2> result;
		if (p_count == 0) return result;
		result.reserve(p_count);
		if (is_affine()) {
			const auto affine = affine_at(Vector2());
			for (std::size_t i = 0; i < p_count; ++i) {
				const auto mapped = affine.xform(p_points[i]);
				if (!mapped.is_finite()) return {};
				result.push_back(mapped);
			}
			return result;
		}

		std::vector<Vector3> homogeneous;
		homogeneous.reserve(p_count);
		real_t farthest = 0;
		for (std::size_t i = 0; i < p_count; ++i) {
			const auto& point = p_points[i];
			const auto mapped = matrix.xform(Vector3(point.x, point.y, 1));
			if (!mapped.is_finite()) return {};
			farthest = std::max(farthest, mapped.z);
			homogeneous.push_back(mapped);
		}
		if (farthest <= 0) return result;
		// Horizon intersections project far beyond nearby points while remaining finite.
		const real_t horizon = farthest / 65536;
		Vector3 previous = homogeneous.back();
		for (const auto& point : homogeneous) {
			if ((previous.z > 0) != (point.z > 0)) {
				const auto intersection = previous.lerp(point, previous.z / (previous.z - point.z));
				const Vector2 projected = Vector2(intersection.x, intersection.y) / horizon;
				if (!projected.is_finite()) return {};
				result.push_back(projected);
			}
			if (point.z > 0) {
				const Vector2 projected = Vector2(point.x, point.y) / point.z;
				if (!projected.is_finite()) return {};
				result.push_back(projected);
			}
			previous = point;
		}
		return result;
	}
}
