#include "formats/html/css/css_math.h"

#include "css_syntax_util.h"
#include "css_token_compat.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace litehtml {
	using dockit::css::detail::index_value;
	using dockit::css::detail::lowcase;
	namespace {
		enum class MathOp {
			LITERAL, INPUT, ADD, SUBTRACT, MULTIPLY, DIVIDE,
			MIN, MAX, CLAMP, ROUND, MOD, REM, SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2,
			POW, SQRT, HYPOT, LOG, EXP, ABS, SIGN,
		};
		enum MathType { NUMBER_TYPE = 0, LENGTH_TYPE = 1, PERCENT_TYPE = 2, ANGLE_TYPE = 4, RESOLUTION_TYPE = 8 };
		enum class RoundStrategy { NEAREST, UP, DOWN, TO_ZERO };
		constexpr std::string_view ROUND_STRATEGIES[]{ "nearest", "up", "down", "to-zero" };

		struct MathFunction {
			std::string_view name;
			MathOp operation;
			size_t minimum;
			size_t maximum;
		};

		constexpr MathFunction MATH_FUNCTIONS[]{
			{ "min", MathOp::MIN, 1, SIZE_MAX }, { "max", MathOp::MAX, 1, SIZE_MAX }, { "clamp", MathOp::CLAMP, 3, 3 },
			{ "round", MathOp::ROUND, 1, 2 }, { "mod", MathOp::MOD, 2, 2 }, { "rem", MathOp::REM, 2, 2 },
			{ "sin", MathOp::SIN, 1, 1 }, { "cos", MathOp::COS, 1, 1 }, { "tan", MathOp::TAN, 1, 1 },
			{ "asin", MathOp::ASIN, 1, 1 }, { "acos", MathOp::ACOS, 1, 1 }, { "atan", MathOp::ATAN, 1, 1 },
			{ "atan2", MathOp::ATAN2, 2, 2 }, { "pow", MathOp::POW, 2, 2 }, { "sqrt", MathOp::SQRT, 1, 1 },
			{ "hypot", MathOp::HYPOT, 1, SIZE_MAX }, { "log", MathOp::LOG, 1, 2 }, { "exp", MathOp::EXP, 1, 1 },
			{ "abs", MathOp::ABS, 1, 1 }, { "sign", MathOp::SIGN, 1, 1 },
		};
		constexpr double PI = 3.14159265358979323846;
		constexpr double E = 2.71828182845904523536;
		constexpr int MAX_DEPTH = 64;
		constexpr int MAX_COMPONENTS = 256;
		constexpr double CSS_LENGTH_LIMIT = 2147483520.0; // Largest float below INT32_MAX.
	}

	struct css_math_node {
		MathOp op;
		int type;
		double number = 0.0;
		css_units unit = css_units_none;
		std::vector<css_math_node_ptr> args;
		RoundStrategy strategy = RoundStrategy::NEAREST;
	};

	css_length interpolate_css_length(const css_length& from, const css_length& to, double progress, bool non_negative) {
		const auto operation = [](MathOp op, int type, css_math_node_ptr left, css_math_node_ptr right) {
			return std::make_shared<css_math_node>(css_math_node{op, type, 0, css_units_none, {std::move(left), std::move(right)}});
		};
		const auto scaled = [&](const css_length& length, double weight) {
			const auto node = length.expression() ? length.expression()->root :
				std::make_shared<css_math_node>(css_math_node{MathOp::LITERAL,
					length.units() == css_units_percentage ? PERCENT_TYPE : LENGTH_TYPE,
					length.val(), length.units(), {}});
			const auto factor = std::make_shared<css_math_node>(css_math_node{MathOp::LITERAL, NUMBER_TYPE, weight, css_units_none, {}});
			return operation(MathOp::MULTIPLY, node->type, node, factor);
		};
		if (!from.expression() && !to.expression() && from.units() == to.units()) {
			const auto result = std::clamp(from.val() + (to.val() - from.val()) * progress,
				non_negative ? 0.0 : -CSS_LENGTH_LIMIT, CSS_LENGTH_LIMIT);
			return css_length(static_cast<float>(result), from.units());
		}
		const auto left = scaled(from, 1.0 - progress);
		const auto right = scaled(to, progress);
		const int type = left->type | right->type;
		css_length result;
		result.set_expression(std::make_shared<css_math_expression>(css_math_expression{
			operation(MathOp::ADD, type, left, right), (type & PERCENT_TYPE) != 0, non_negative, false
		}));
		return result;
	}

	namespace {
		css_math_node_ptr literal(double p_number, css_units p_unit = css_units_none, int p_dimension = NUMBER_TYPE) {
			const int type = p_unit == css_units_none ? p_dimension : (p_unit == css_units_percentage ? PERCENT_TYPE : LENGTH_TYPE);
			return std::make_shared<css_math_node>(css_math_node{ MathOp::LITERAL, type, p_number, p_unit, {} });
		}

		std::optional<double> resolution_dimension(const css_token& p_token) {
			if (p_token.type != DIMENSION || !std::isfinite(p_token.n.number)) return std::nullopt;
			const auto unit = lowcase(p_token.unit);
			const double value = p_token.n.number;
			if (unit == "x" || unit == "dppx") return value;
			if (unit == "dpi") return value / CSS_DPI_PER_DPPX;
			if (unit == "dpcm") return value * 2.54 / CSS_DPI_PER_DPPX;
			return std::nullopt;
		}

		// A percentage combines with a length only in a length-percentage context.
		int merge_types(int p_a, int p_b, int p_options) {
			if (p_a == p_b) return p_a;
			if (p_a == NUMBER_TYPE || p_b == NUMBER_TYPE || ((p_a | p_b) & (ANGLE_TYPE | RESOLUTION_TYPE)) ||
				(p_options & f_length_percentage) != f_length_percentage) return -1;
			return p_a | p_b;
		}

		int function_result(MathOp p_operation, int p_input) {
			switch (p_operation) {
				case MathOp::SIN:
				case MathOp::COS:
				case MathOp::TAN: return p_input == NUMBER_TYPE || p_input == ANGLE_TYPE ? NUMBER_TYPE : -1;
				case MathOp::ASIN:
				case MathOp::ACOS:
				case MathOp::ATAN: return p_input == NUMBER_TYPE ? ANGLE_TYPE : -1;
				case MathOp::ATAN2: return ANGLE_TYPE;
				case MathOp::SIGN: return NUMBER_TYPE;
				case MathOp::POW:
				case MathOp::SQRT:
				case MathOp::LOG:
				case MathOp::EXP: return p_input == NUMBER_TYPE ? NUMBER_TYPE : -1;
				default: return p_input;
			}
		}

		struct MathParser {
			const css_token_vector& tokens;
			int options;
			int depth;
			int& budget;
			size_t cursor = 0;
			const css_math_input_names* names = nullptr;

			css_math_node_ptr sum() { return parse_binary(false); }

			css_math_node_ptr parse_binary(bool p_product) {
				auto left = p_product ? term() : parse_binary(true);
				while (left && cursor < tokens.size()) {
					const auto& token = tokens[cursor];
					const int op = token.ch;
					if (!(p_product ? op == '*' || op == '/' : op == '+' || op == '-')) break;
					if ((!p_product && !token.spaced_operator) || --budget < 0) return {};
					++cursor;
					auto right = p_product ? term() : parse_binary(true);
					if (!right) return {};
					int type = -1;
					MathOp operation;
					if (op == '+' || op == '-') {
						type = merge_types(left->type, right->type, options);
						operation = op == '+' ? MathOp::ADD : MathOp::SUBTRACT;
					}
					else if (op == '*') {
						if (left->type == NUMBER_TYPE || right->type == NUMBER_TYPE) type = left->type | right->type;
						operation = MathOp::MULTIPLY;
					}
					else {
						if (right->type == NUMBER_TYPE) type = left->type;
						else if (left->type == right->type &&
							(left->type != PERCENT_TYPE || (options & f_percentage))) type = NUMBER_TYPE;
						operation = MathOp::DIVIDE;
					}
					if (type < 0) return {};
					left = std::make_shared<css_math_node>(css_math_node{ operation, type, 0.0, css_units_none, { left, right } });
				}
				return left;
			}

			css_math_node_ptr term() {
				if (cursor >= tokens.size() || --budget < 0) return {};
				const auto& token = tokens[cursor++];
				if (token.type == CV_FUNCTION || token.type == ROUND_BLOCK) return function(token, options, depth + 1, budget, names);
				if (token.type == NUMBER) return std::isfinite(token.n.number) ? literal(token.n.number) : nullptr;
				if (token.type == PERCENTAGE && std::isfinite(token.n.number)) return literal(token.n.number, css_units_percentage);
				if (token.type == IDENT) {
					const auto name = token.ident();
					if (names) {
						const auto input = std::find(names->begin(), names->end(), name);
						if (input != names->end()) return std::make_shared<css_math_node>(css_math_node{
							MathOp::INPUT, NUMBER_TYPE, double(input - names->begin()), css_units_none, {}});
					}
					if (name == "pi") return literal(PI);
					if (name == "e") return literal(E);
					if (name == "infinity") return literal(std::numeric_limits<double>::infinity());
					if (name == "-infinity") return literal(-std::numeric_limits<double>::infinity());
					if (name == "nan") return literal(std::numeric_limits<double>::quiet_NaN());
					return {};
				}
				if (token.type == DIMENSION && std::isfinite(token.n.number)) {
					const auto unit = lowcase(token.unit);
					const double value = token.n.number;
					if (unit == "rad") return literal(value, css_units_none, ANGLE_TYPE);
					if (unit == "deg") return literal(value * (PI / 180.0), css_units_none, ANGLE_TYPE);
					if (unit == "grad") return literal(value * (PI / 200.0), css_units_none, ANGLE_TYPE);
					if (unit == "turn") return literal(value * (2.0 * PI), css_units_none, ANGLE_TYPE);
					if (const auto resolution = resolution_dimension(token)) return literal(*resolution, css_units_none, RESOLUTION_TYPE);
				}
				css_length length;
				if (token.type == DIMENSION && length.from_token(token, f_length) && std::isfinite(length.val())) {
					return literal(length.val(), length.units());
				}
				return {};
			}

			static css_math_node_ptr function(const css_token& p_token, int p_options, int p_depth, int& r_budget,
				const css_math_input_names* p_names = nullptr) {
				if (p_depth > MAX_DEPTH || p_token.value.empty()) return {};
				MathParser parser{ p_token.value, p_options, p_depth, r_budget, 0, p_names };
				const auto name = p_token.type == ROUND_BLOCK ? "calc" : lowcase(p_token.name);
				if (name == "calc") {
					auto result = parser.sum();
					return parser.cursor == parser.tokens.size() ? result : nullptr;
				}
				const auto descriptor = std::find_if(std::begin(MATH_FUNCTIONS), std::end(MATH_FUNCTIONS),
					[&](const MathFunction& entry) { return entry.name == name; });
				if (descriptor == std::end(MATH_FUNCTIONS)) return {};
				const auto operation = descriptor->operation;
				RoundStrategy strategy = RoundStrategy::NEAREST;
				if (operation == MathOp::ROUND && parser.tokens.size() > 1 && parser.tokens[1].type == ',') {
					const auto keyword = parser.tokens.front().ident();
					for (size_t i = 0; i < std::size(ROUND_STRATEGIES); ++i) {
						if (keyword == ROUND_STRATEGIES[i]) {
							strategy = static_cast<RoundStrategy>(i);
							parser.cursor = 2;
							break;
						}
					}
				}

				std::vector<css_math_node_ptr> args;
				int input_type = -1;
				for (;;) {
					if (args.size() == descriptor->maximum) return {};
					const bool unbounded = operation == MathOp::CLAMP && args.size() != 1 && parser.cursor < parser.tokens.size() &&
						parser.tokens[parser.cursor].ident() == "none";
					if (unbounded) {
						++parser.cursor;
						args.emplace_back();
					}
					else {
						auto arg = parser.sum();
						if (!arg) return {};
						input_type = input_type < 0 ? arg->type : merge_types(input_type, arg->type, p_options);
						if (input_type < 0) return {};
						args.push_back(std::move(arg));
					}
					if (parser.cursor == parser.tokens.size()) break;
					if (parser.tokens[parser.cursor++].type != ',') return {};
				}
				if (args.size() < descriptor->minimum) return {};
				if (operation == MathOp::ROUND && args.size() == 1) {
					if (input_type != NUMBER_TYPE) return {};
					args.push_back(literal(1.0));
				}
				const int type = function_result(operation, input_type);
				if (type < 0) return {};
				return std::make_shared<css_math_node>(css_math_node{
					operation, type, 0.0, css_units_none, std::move(args), strategy,
				});
			}
		};

		double round_to_step(double p_value, double p_step, RoundStrategy p_strategy) {
			const double step = std::abs(p_step);
			if (step == 0.0 || std::isnan(p_value) || std::isnan(step) ||
				(std::isinf(p_value) && std::isinf(step))) return std::numeric_limits<double>::quiet_NaN();
			if (std::isinf(p_value) || p_value == 0.0) return p_value;
			if (std::isinf(step)) {
				const bool round_away = (p_strategy == RoundStrategy::UP && p_value > 0.0) ||
					(p_strategy == RoundStrategy::DOWN && p_value < 0.0);
				return std::copysign(round_away ? step : 0.0, p_value);
			}

			const double remainder = std::fmod(p_value, step);
			if (remainder == 0.0) return p_value;
			const double magnitude = std::abs(remainder);
			const double towards_zero = std::abs(p_value) - magnitude;
			const double away = step - magnitude;
			const bool positive = p_value > 0.0;
			const bool round_away = p_strategy == RoundStrategy::NEAREST ? magnitude > away || (magnitude == away && positive) :
				(p_strategy == RoundStrategy::UP && positive) || (p_strategy == RoundStrategy::DOWN && !positive);
			return std::copysign(round_away ? towards_zero + step : towards_zero, p_value);
		}

		double modulus(double p_value, double p_step) {
			if (std::isinf(p_step) && std::signbit(p_value) != std::signbit(p_step))
				return std::numeric_limits<double>::quiet_NaN();
			const double remainder = std::fmod(p_value, p_step);
			if (remainder == 0.0) return std::copysign(0.0, p_step);
			return std::signbit(remainder) == std::signbit(p_step) ? remainder : remainder + p_step;
		}

		double evaluate_node(const css_math_node& p_node, double p_basis, const css_math_input_values* p_inputs = nullptr) {
			const auto at = [&](size_t p_index) { return evaluate_node(*p_node.args[p_index], p_basis, p_inputs); };
			switch (p_node.op) {
				case MathOp::LITERAL: return p_node.unit == css_units_percentage ? p_node.number * p_basis / 100.0 : p_node.number;
				case MathOp::INPUT: return p_inputs ? (*p_inputs)[static_cast<size_t>(p_node.number)] : 0.0;
				case MathOp::ADD: return at(0) + at(1);
				case MathOp::SUBTRACT: return at(0) - at(1);
				case MathOp::MULTIPLY: return at(0) * at(1);
				case MathOp::DIVIDE: return at(0) / at(1);
				case MathOp::ROUND: return round_to_step(at(0), at(1), p_node.strategy);
				case MathOp::MOD: return modulus(at(0), at(1));
				case MathOp::REM: return std::fmod(at(0), at(1));
				case MathOp::SIN: return std::sin(at(0));
				case MathOp::COS: return std::cos(at(0));
				case MathOp::TAN: return std::tan(at(0));
				case MathOp::ASIN: return std::asin(at(0));
				case MathOp::ACOS: return std::acos(at(0));
				case MathOp::ATAN: return std::atan(at(0));
				case MathOp::ATAN2: return std::atan2(at(0), at(1));
				case MathOp::POW: {
					const double base = at(0);
					const double exponent = at(1);
					if (std::isnan(base) || std::isnan(exponent) || (std::abs(base) == 1.0 && std::isinf(exponent)))
						return std::numeric_limits<double>::quiet_NaN();
					return std::pow(base, exponent);
				}
				case MathOp::SQRT: return std::sqrt(at(0));
				case MathOp::LOG: {
					const double value = at(0);
					if (p_node.args.size() == 1) return std::log(value);
					const double base = at(1);
					if (std::isnan(value) || std::isnan(base) || value < 0.0 || base <= 0.0 || base == 1.0)
						return std::numeric_limits<double>::quiet_NaN();
					if (value == 0.0 || std::isinf(value)) return std::log(value);
					return std::log(value) / std::log(base);
				}
				case MathOp::EXP: return std::exp(at(0));
				case MathOp::ABS: return std::abs(at(0));
				case MathOp::SIGN: {
					const double value = at(0);
					return value == 0.0 || std::isnan(value) ? value : std::copysign(1.0, value);
				}
				case MathOp::HYPOT: {
					double value = 0.0;
					for (size_t i = 0; i < p_node.args.size(); ++i) {
						const double next = at(i);
						if (std::isnan(next)) return next;
						value = std::hypot(value, next);
					}
					return value;
				}
				case MathOp::MIN:
				case MathOp::MAX: {
					double result = at(0);
					if (std::isnan(result)) return result;
					for (size_t index = 1; index < p_node.args.size(); ++index) {
						const double next = at(index);
						if (std::isnan(next)) return next;
						result = p_node.op == MathOp::MIN ? std::min(result, next) : std::max(result, next);
					}
					return result;
				}
				case MathOp::CLAMP: {
					const double lower = p_node.args[0] ? at(0) : -std::numeric_limits<double>::infinity();
					const double middle = at(1);
					const double upper = p_node.args[2] ? at(2) : std::numeric_limits<double>::infinity();
					if (std::isnan(lower) || std::isnan(middle) || std::isnan(upper)) return std::numeric_limits<double>::quiet_NaN();
					return std::max(lower, std::min(middle, upper));
				}
			}
			return 0.0;
		}

		css_math_node_ptr compute_node(const css_math_node_ptr& p_node, const std::function<pixel_t(const css_length&)>& p_resolve_length) {
			if (p_node->op == MathOp::LITERAL) {
				if (p_node->type != LENGTH_TYPE || p_node->unit == css_units_px) return p_node;
				const css_length length(static_cast<float>(p_node->number), p_node->unit);
				return literal(p_resolve_length(length), css_units_px);
			}
			css_math_node computed = *p_node;
			for (auto& arg : computed.args) {
				if (arg) arg = compute_node(arg, p_resolve_length);
			}
			return std::make_shared<css_math_node>(std::move(computed));
		}

		string serialize_node(const css_math_node& p_node) {
			char binary_operator = '\0';
			switch (p_node.op) {
				case MathOp::INPUT: return "input(" + std::to_string(static_cast<size_t>(p_node.number)) + ')';
				case MathOp::LITERAL: {
					if (std::isnan(p_node.number)) return "NaN";
					if (std::isinf(p_node.number)) return p_node.number < 0 ? "-infinity" : "infinity";
					const auto suffix = p_node.type == ANGLE_TYPE ? "rad" :
						p_node.type == RESOLUTION_TYPE ? "dppx" : (p_node.unit == css_units_none ? "" : index_value(p_node.unit, css_units_strings));
					return std::to_string(p_node.number) + suffix;
				}
				case MathOp::ADD: binary_operator = '+'; break;
				case MathOp::SUBTRACT: binary_operator = '-'; break;
				case MathOp::MULTIPLY: binary_operator = '*'; break;
				case MathOp::DIVIDE: binary_operator = '/'; break;
				default: break;
			}
			if (binary_operator) {
				return "(" + serialize_node(*p_node.args[0]) + " " + binary_operator + " " + serialize_node(*p_node.args[1]) + ")";
			}
			const auto descriptor = std::find_if(std::begin(MATH_FUNCTIONS), std::end(MATH_FUNCTIONS),
				[&](const MathFunction& entry) { return entry.operation == p_node.op; });
			string result(descriptor->name);
			result += '(';
			if (p_node.op == MathOp::ROUND && p_node.strategy != RoundStrategy::NEAREST) {
				result += ROUND_STRATEGIES[static_cast<size_t>(p_node.strategy)];
				result += ", ";
			}
			for (size_t index = 0; index < p_node.args.size(); ++index) {
				if (index) result += ", ";
				result += p_node.args[index] ? serialize_node(*p_node.args[index]) : "none";
			}
			return result + ')';
		}

		bool uses_basis(const css_math_node_ptr& p_node) {
			if (!p_node) return false;
			if (p_node->type & PERCENT_TYPE) return true;
			return std::any_of(p_node->args.begin(), p_node->args.end(), uses_basis);
		}

		bool requires_lengths(const css_math_node_ptr& p_node) {
			if (!p_node) return false;
			if (p_node->type & LENGTH_TYPE) return true;
			return std::any_of(p_node->args.begin(), p_node->args.end(), requires_lengths);
		}
	}

	std::shared_ptr<const css_math_expression> parse_css_math(const css_token& p_token, int p_options) {
		if (p_token.type != CV_FUNCTION) return {};
		int budget = MAX_COMPONENTS;
		const auto root = MathParser::function(p_token, p_options, 0, budget);
		if (!root || (root->type == NUMBER_TYPE && !(p_options & f_number)) || (root->type & (ANGLE_TYPE | RESOLUTION_TYPE)) ||
			((root->type & LENGTH_TYPE) && !(p_options & f_length)) ||
			((root->type & PERCENT_TYPE) && !(p_options & f_percentage))) return {};
		return std::make_shared<css_math_expression>(css_math_expression{
			root, uses_basis(root), bool(p_options & f_positive), root->type == NUMBER_TYPE,
		});
	}

	css_math_node_ptr parse_css_scalar_math(const css_token& p_token, css_math_scalar p_type,
		const css_math_input_names* p_names) {
		if (p_token.type != CV_FUNCTION) return {};
		int budget = MAX_COMPONENTS;
		auto root = MathParser::function(p_token, f_percentage, 0, budget, p_names);
		if (!root || requires_lengths(root)) return {};
		const bool allow_number = p_type == css_math_scalar::number || p_type == css_math_scalar::number_or_percentage ||
			p_type == css_math_scalar::number_or_angle;
		const bool allow_percent = p_type == css_math_scalar::percentage || p_type == css_math_scalar::number_or_percentage;
		const bool allow_angle = p_type == css_math_scalar::angle || p_type == css_math_scalar::number_or_angle;
		if ((root->type == NUMBER_TYPE && !allow_number) || (root->type == PERCENT_TYPE && !allow_percent) ||
			(root->type == ANGLE_TYPE && !allow_angle) || root->type == RESOLUTION_TYPE) return {};
		return root;
	}

	double evaluate_css_scalar_math(const css_math_node& p_node, css_math_scalar p_type,
		double p_percentage_basis, const css_math_input_values* p_inputs) {
		double value = evaluate_node(p_node, p_percentage_basis, p_inputs);
		if (std::isnan(value)) value = 0.0;
		if (p_type == css_math_scalar::number_or_angle && p_node.type == ANGLE_TYPE)
			value *= 180.0 / PI;
		return value;
	}

	std::optional<double> evaluate_css_scalar_math(const css_token& p_token, css_math_scalar p_type,
		double p_percentage_basis) {
		const auto root = parse_css_scalar_math(p_token, p_type);
		if (!root) return std::nullopt;
		return evaluate_css_scalar_math(*root, p_type, p_percentage_basis);
	}

	std::optional<double> parse_css_angle(const css_token& p_token, bool p_allow_unitless_zero, bool p_normalize) {
		constexpr double full_turn = 2.0 * PI;
		const auto normalize = [p_normalize](double value, double period) {
			return p_normalize ? std::remainder(value, period) : value;
		};
		if (p_token.type == CV_FUNCTION) {
			const auto angle = evaluate_css_scalar_math(p_token, css_math_scalar::angle);
			if (!angle) return std::nullopt;
			return std::isfinite(*angle) ? normalize(*angle, full_turn) : 0.0;
		}
		if (p_token.type == NUMBER && p_allow_unitless_zero && p_token.n.number == 0) return 0.0;
		if (p_token.type != DIMENSION || !std::isfinite(p_token.n.number)) return std::nullopt;
		const auto unit = lowcase(p_token.unit);
		const double value = p_token.n.number;
		if (unit == "deg") return normalize(value, 360.0) * PI / 180.0;
		if (unit == "grad") return normalize(value, 400.0) * PI / 200.0;
		if (unit == "rad") return normalize(value, full_turn);
		if (unit == "turn") return normalize(value, 1.0) * full_turn;
		return std::nullopt;
	}

	std::optional<double> parse_css_resolution(const css_token& p_token) {
		if (p_token.type == DIMENSION) {
			const auto value = resolution_dimension(p_token);
			return value && *value >= 0.0 ? value : std::nullopt;
		}
		if (p_token.type != CV_FUNCTION) return std::nullopt;
		int budget = MAX_COMPONENTS;
		const auto root = MathParser::function(p_token, 0, 0, budget);
		if (!root || root->type != RESOLUTION_TYPE || requires_lengths(root) || uses_basis(root)) return std::nullopt;
		const double value = evaluate_node(*root, 0);
		return std::isnan(value) ? 0.0 : std::max(value, 0.0);
	}

	std::shared_ptr<const css_math_expression> compute_css_math(const css_math_expression& p_value,
		const std::function<pixel_t(const css_length&)>& p_resolve_length) {
		return std::make_shared<css_math_expression>(css_math_expression{
			compute_node(p_value.root, p_resolve_length), p_value.has_percentage, p_value.non_negative, p_value.is_number,
		});
	}

	double css_math_expression::evaluate(double p_basis) const {
		const double value = evaluate_node(*root, p_basis);
		// Keep non-finite values and unrepresentable geometry out of the layout engine.
		return std::isnan(value) ? 0.0 : std::clamp(value, non_negative ? 0.0 : -CSS_LENGTH_LIMIT, CSS_LENGTH_LIMIT);
	}

	string css_math_expression::to_string() const {
		return serialize_node(*root);
	}
}
