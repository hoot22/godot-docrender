#include "html_transform_interpolation.h"

#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <cmath>

namespace dockit {
	using namespace godot;

	namespace {
		bool is_2d(const Projection& matrix) {
			return matrix[0].z == 0 && matrix[0].w == 0 && matrix[1].z == 0 && matrix[1].w == 0 &&
				matrix[2] == Vector4(0, 0, 1, 0) && matrix[3].z == 0 && matrix[3].w == 1;
		}

		bool is_finite(const Projection& matrix) {
			return matrix[0].is_finite() && matrix[1].is_finite() && matrix[2].is_finite() && matrix[3].is_finite();
		}

		struct Decomposed2D {
			Vector2 translation;
			Vector2 scale;
			double angle;
			Vector2 residual_x;
			Vector2 residual_y;
		};

		std::optional<Decomposed2D> decompose_2d(const Projection& matrix) {
			const double a = matrix[0].x, b = matrix[0].y, c = matrix[1].x, d = matrix[1].y;
			const double determinant = a * d - b * c;
			if (determinant == 0) return std::nullopt;

			double scale_x = std::hypot(a, b);
			double scale_y = std::hypot(c, d);
			if (determinant < 0) {
				// Preserve reflections by choosing one axis, as required for 2D decomposition.
				if (a < d) scale_x = -scale_x;
				else scale_y = -scale_y;
			}
			const Vector2 x(a / scale_x, b / scale_x);
			const Vector2 y(c / scale_y, d / scale_y);
			const double angle = std::atan2(b / scale_x, a / scale_x);
			return Decomposed2D{Vector2(matrix[3].x, matrix[3].y), Vector2(scale_x, scale_y), angle,
				x * x.x - y * x.y, x * x.y + y * x.x};
		}

		std::optional<Projection> interpolate_2d(const Projection& from, const Projection& to, double progress) {
			auto a = decompose_2d(from);
			auto b = decompose_2d(to);
			if (!a || !b) return std::nullopt;
			constexpr double pi = 3.14159265358979323846;
			if ((a->scale.x < 0 && b->scale.y < 0) || (a->scale.y < 0 && b->scale.x < 0)) {
				a->scale = -a->scale;
				a->angle += a->angle < 0 ? pi : -pi;
			}
			if (a->angle == 0) a->angle = 2 * pi;
			if (b->angle == 0) b->angle = 2 * pi;
			if (a->angle - b->angle > pi) a->angle -= 2 * pi;
			else if (b->angle - a->angle > pi) b->angle -= 2 * pi;

			const auto translation = a->translation.lerp(b->translation, progress);
			const auto scale = a->scale.lerp(b->scale, progress);
			const auto rx = a->residual_x.lerp(b->residual_x, progress);
			const auto ry = a->residual_y.lerp(b->residual_y, progress);
			const double angle = a->angle + (b->angle - a->angle) * progress;
			const double cosine = std::cos(angle), sine = std::sin(angle);
			const auto x = (rx * cosine + ry * sine) * scale.x;
			const auto y = (ry * cosine - rx * sine) * scale.y;
			return Projection(Vector4(x.x, x.y, 0, 0), Vector4(y.x, y.y, 0, 0),
				Vector4(0, 0, 1, 0), Vector4(translation.x, translation.y, 0, 1));
		}

		struct Decomposed3D {
			Vector3 translation;
			Vector3 scale;
			Vector3 shear;
			Quaternion rotation;
			Vector4 perspective;
		};

		Vector3 xyz(const Vector4& column) {
			return Vector3(column.x, column.y, column.z);
		}

		std::optional<Decomposed3D> decompose_3d(const Projection& matrix) {
			const auto w = matrix[3].w;
			if (w == 0) return std::nullopt;

			Vector3 x = xyz(matrix[0]) / w;
			Vector3 y = xyz(matrix[1]) / w;
			Vector3 z = xyz(matrix[2]) / w;
			const auto translation = xyz(matrix[3]) / w;
			const Basis linear(x, y, z);
			const auto determinant = linear.determinant();
			if (determinant == 0 || !std::isfinite(determinant)) return std::nullopt;
			const Vector3 projective_row(matrix[0].w / w, matrix[1].w / w, matrix[2].w / w);
			const auto p = linear.transposed().inverse().xform(projective_row);
			const auto perspective_w = 1 - p.dot(translation);
			if (perspective_w == 0) return std::nullopt;

			// Separate scale and XY, XZ, YZ shear from the orthonormal rotation columns.
			const auto sx = x.length();
			if (sx == 0 || !std::isfinite(sx)) return std::nullopt;
			x /= sx;
			const auto xy = x.dot(y);
			y -= x * xy;
			const auto sy = y.length();
			if (sy == 0 || !std::isfinite(sy)) return std::nullopt;
			y /= sy;
			const auto xz = x.dot(z);
			z -= x * xz;
			const auto yz = y.dot(z);
			z -= y * yz;
			const auto sz = z.length();
			if (sz == 0 || !std::isfinite(sz)) return std::nullopt;
			z /= sz;

			const real_t sign = determinant < 0 ? -1 : 1;
			const auto rotation = Basis(x * sign, y * sign, z * sign).get_quaternion().normalized();
			return Decomposed3D{translation, Vector3(sx, sy, sz) * sign, Vector3(xy / sy, xz / sz, yz / sz),
				rotation, Vector4(p.x, p.y, p.z, perspective_w)};
		}

		std::optional<Projection> interpolate_3d(const Projection& from, const Projection& to, double progress) {
			const auto a = decompose_3d(from);
			const auto b = decompose_3d(to);
			if (!a || !b) return std::nullopt;
			const auto translation = a->translation.lerp(b->translation, progress);
			const auto scale = a->scale.lerp(b->scale, progress);
			const auto shear = a->shear.lerp(b->shear, progress);
			const Basis rotation(a->rotation.slerp(b->rotation, progress).normalized());
			const auto p = a->perspective.lerp(b->perspective, progress);
			const Basis scaled_shear(scale.x, shear.x * scale.y, shear.y * scale.z,
				0, scale.y, shear.z * scale.z, 0, 0, scale.z);
			Projection projective;
			for (int column = 0; column < 4; ++column) projective[column].w = p[column];
			return projective * Projection(Transform3D(rotation * scaled_shear, translation));
		}
	}

	std::optional<Projection> interpolate_transform_matrices(const Projection& p_from, const Projection& p_to,
			double p_progress) {
		if (!is_finite(p_from) || !is_finite(p_to)) return std::nullopt;
		const auto sample = is_2d(p_from) && is_2d(p_to) ?
			interpolate_2d(p_from, p_to, p_progress) : interpolate_3d(p_from, p_to, p_progress);
		if (!sample || !is_finite(*sample)) return std::nullopt;
		if (p_progress == 0) return p_from;
		if (p_progress == 1) return p_to;
		return sample;
	}
}
