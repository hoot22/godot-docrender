#include "css_grid_values.h"
#include "css_syntax_util.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace litehtml {
	using dockit::css::detail::is_one_of;
	using dockit::css::detail::lowcase;

	namespace {
		bool read_integer(const css_token& token, std::int64_t& integer) {
			if (token.type != NUMBER || token.n.number_type != css_number_integer) return false;
			// Keep the sign and exact digits independently of the tokenizer's float.
			const auto& repr = token.repr;
			if (repr.empty()) return false;
			const bool negative = repr.front() == '-';
			const std::size_t first = negative || repr.front() == '+' ? 1 : 0;
			constexpr auto bound = std::numeric_limits<std::int64_t>::max() / 4;
			std::int64_t value = 0;
			for (std::size_t i = first; i < repr.size(); ++i) {
				const int digit = repr[i] - '0';
				if (digit < 0 || digit > 9) return false;
				value = value > (bound - digit) / 10 ? bound : value * 10 + digit;
			}
			integer = negative ? -value : value;
			return value != 0;
		}

		bool is_grid_name(const css_token& token) {
			return token.type == IDENT && !is_one_of(token.ident(), "auto", "span", "inherit", "initial", "unset", "revert", "revert-layer", "default");
		}

		bool parse_line(const css_token* first, const css_token* end, grid_line& line) {
			const auto count = end - first;
			if (count == 1 && first[0].ident() == "auto") {
				line = {};
				return true;
			}
			if (count < 1 || count > 3) return false;
			std::int64_t value = 0;
			string name;
			bool span = false;
			for (auto token = first; token != end; ++token) {
				if (!span && token->ident() == "span") span = true;
				else if (!value && read_integer(*token, value)) continue;
				else if (name.empty() && is_grid_name(*token)) name = token->name;
				else return false;
			}
			if ((!value && name.empty()) || (span && value < 0)) return false;
			using kind = grid_line::kind;
			line = { span ? kind::span : value ? kind::number : kind::name, value ? value : 1, std::move(name) };
			return true;
		}

		grid_line omitted_grid_line(const grid_line& line) {
			return line.type == grid_line::kind::name ? line : grid_line{};
		}

		std::size_t parse_grid_lines(const css_token_vector& tokens, grid_line* lines, std::size_t capacity) {
			if (tokens.empty()) return 0;
			const auto* next = tokens.data();
			const auto* end = next + tokens.size();
			for (std::size_t index = 0; index < capacity; ++index) {
				const auto* slash = std::find_if(next, end, [](const auto& token) { return token.ch == '/'; });
				if (!parse_line(next, slash, lines[index])) return 0;
				if (slash == end) return index + 1;
				next = slash + 1;
			}
			return 0;
		}

		bool parse_breadth(const css_token& token, bool allow_fraction, grid_breadth& breadth) {
			const auto keyword = token.ident();
			if (keyword == "auto") return true;
			if (keyword == "min-content" || keyword == "max-content") {
				breadth.kind = keyword == "min-content" ? grid_breadth_kind::min_content : grid_breadth_kind::max_content;
				return true;
			}
			if (token.type == DIMENSION && lowcase(token.unit) == "fr") {
				if (!allow_fraction || !std::isfinite(token.n.number) || token.n.number < 0) return false;
				breadth.kind = grid_breadth_kind::fraction;
				breadth.fraction = token.n.number;
				return true;
			}
			if (is_one_of(token.type, NUMBER, DIMENSION, PERCENTAGE) && !std::isfinite(token.n.number)) return false;
			if (!breadth.length.from_token(token, f_length_percentage | f_positive | f_math)) return false;
			breadth.kind = grid_breadth_kind::length;
			return true;
		}

		bool parse_track(const css_token& token, grid_track& track) {
			if (token.type == CV_FUNCTION && lowcase(token.name) == "minmax") {
				const auto& args = token.value;
				return args.size() == 3 && args[1].ch == ',' &&
					parse_breadth(args[0], false, track.minimum) && parse_breadth(args[2], true, track.maximum);
			}
			if (token.type == CV_FUNCTION && lowcase(token.name) == "fit-content") {
				if (token.value.size() != 1 || !track.maximum.length.from_token(token.value.front(), f_length_percentage | f_positive | f_math))
					return false;
				track.maximum.kind = grid_breadth_kind::fit_content;
				return true;
			}
			if (!parse_breadth(token, true, track.maximum)) return false;
			if (track.maximum.kind != grid_breadth_kind::fraction) track.minimum = track.maximum;
			return true;
		}

		bool has_fixed_breadth(const grid_track& track) {
			return track.minimum.kind == grid_breadth_kind::length || track.maximum.kind == grid_breadth_kind::length;
		}

		bool append_grid_line_names(const css_token& block, grid_track_segment& segment) {
			const auto index = static_cast<int>(segment.tracks.size());
			for (const auto& token : block.value) {
				if (token.type == WHITESPACE) continue;
				if (!is_grid_name(token)) return false;
				auto& indices = segment.names[token.name];
				if (indices.empty() || indices.back() != index) indices.push_back(index);
			}
			return true;
		}

		bool is_grid_cell_name_byte(unsigned char ch) {
			return ch >= 0x80 || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
				(ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
		}

		bool is_grid_cell_space(char ch) {
			return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f';
		}

	}

	void append_grid_segment(grid_track_segment& to, const grid_track_segment& from) {
		const auto offset = static_cast<int>(to.tracks.size());
		to.tracks.insert(to.tracks.end(), from.tracks.begin(), from.tracks.end());
		for (const auto& [name, lines] : from.names) {
			auto& indices = to.names[name];
			for (const auto index : lines) {
				const auto line = offset + index;
				if (indices.empty() || indices.back() != line) indices.push_back(line);
			}
		}
	}

	static bool parse_track_template(const css_token_vector& tokens, grid_track_template& tracks, bool allow_repeat) {
		grid_track_template parsed;
		std::size_t available_tracks = grid_track_limit;
		bool previous_was_line_names = false;
		for (const auto& token : tokens) {
			auto& segment = parsed.repeat ? parsed.suffix : parsed.prefix;
			if (token.type == SQUARE_BLOCK) {
				if (previous_was_line_names || !append_grid_line_names(token, segment)) return false;
				previous_was_line_names = true;
				continue;
			}
			previous_was_line_names = false;
			if (token.type == CV_FUNCTION && lowcase(token.name) == "repeat") {
				if (!allow_repeat) return false;
				const auto& args = token.value;
				if (args.size() < 3 || args[1].ch != ',') return false;
				const auto keyword = args[0].ident();
				const bool automatic = keyword == "auto-fill" || keyword == "auto-fit";
				std::int64_t count = 1;
				if (!automatic && (!read_integer(args[0], count) || count <= 0)) return false;
				grid_track_segment pattern;
				const auto max_length = available_tracks / static_cast<std::uint64_t>(count);
				bool pattern_line_names = false;
				for (auto it = args.begin() + 2; it != args.end(); ++it) {
					if (it->type == SQUARE_BLOCK) {
						if (pattern_line_names || !append_grid_line_names(*it, pattern)) return false;
					} else {
						grid_track track;
						if (pattern.tracks.size() == max_length || !parse_track(*it, track)) return false;
						pattern.tracks.push_back(std::move(track));
					}
					pattern_line_names = it->type == SQUARE_BLOCK;
				}
				const auto length = pattern.tracks.size();
				if (length == 0) return false;
				available_tracks -= static_cast<std::size_t>(count) * length;
				if (automatic) {
					if (parsed.repeat || !std::all_of(pattern.tracks.begin(), pattern.tracks.end(), has_fixed_breadth)) return false;
					parsed.repeat = grid_auto_repeat{ std::move(pattern), keyword == "auto-fit" };
				} else {
					for (std::int64_t i = 0; i < count; ++i)
						append_grid_segment(segment, pattern);
				}
			}
			else {
				grid_track track;
				if (available_tracks == 0 || !parse_track(token, track)) return false;
				segment.tracks.push_back(std::move(track));
				--available_tracks;
			}
		}
		if (parsed.repeat) {
			for (const auto* segment : { &parsed.prefix, &parsed.suffix })
				if (!std::all_of(segment->tracks.begin(), segment->tracks.end(), has_fixed_breadth)) return false;
		} else if (parsed.prefix.tracks.empty()) return false;
		tracks = std::move(parsed);
		return true;
	}

	static std::optional<grid_subgrid_lines> parse_subgrid_lines(const css_token_vector& tokens) {
		const auto read_names = [](const css_token& block, grid_subgrid_lines::name_list& lists) {
			if (block.type != SQUARE_BLOCK) return false;
			std::vector<std::string> names;
			for (const auto& token : block.value) {
				if (token.type == WHITESPACE) continue;
				if (!is_grid_name(token)) return false;
				names.push_back(token.name);
			}
			lists.push_back(std::move(names));
			return true;
		};
		grid_subgrid_lines result;
		std::size_t remaining = grid_track_limit + 1;
		for (auto it = tokens.begin() + 1; it != tokens.end(); ++it) {
			auto& lists = result.automatic.empty() ? result.prefix : result.suffix;
			if (it->type != CV_FUNCTION || lowcase(it->name) != "repeat") {
				if (!remaining || !read_names(*it, lists)) return std::nullopt;
				--remaining;
				continue;
			}
			const auto& args = it->value;
			if (args.size() < 3 || args[1].ch != ',' || args.size() - 2 > grid_track_limit + 1) return std::nullopt;
			grid_subgrid_lines::name_list pattern;
			for (auto name = args.begin() + 2; name != args.end(); ++name) {
				if (!read_names(*name, pattern)) return std::nullopt;
			}
			if (args.front().ident() == "auto-fill") {
				if (!result.automatic.empty()) return std::nullopt;
				result.automatic = std::move(pattern);
			} else {
				std::int64_t count = 0;
				if (!read_integer(args.front(), count) || count <= 0 ||
					static_cast<std::uint64_t>(count) > remaining / pattern.size()) return std::nullopt;
				remaining -= static_cast<std::size_t>(count) * pattern.size();
				for (std::int64_t i = 0; i < count; ++i)
					lists.insert(lists.end(), pattern.begin(), pattern.end());
			}
		}
		return result;
	}

	void add_subgrid_line_names(const grid_subgrid_lines& value, std::size_t track_count, grid_line_indices& names) {
		const auto capacity = track_count + 1;
		std::size_t line = 0;
		const auto append = [&](const grid_subgrid_lines::name_list& lists) {
			for (const auto& list : lists) {
				if (line == capacity) break;
				const auto index = static_cast<int>(line++);
				for (const auto& name : list) {
					auto& indices = names[name];
					const auto at = std::lower_bound(indices.begin(), indices.end(), index);
					if (at == indices.end() || *at != index) indices.insert(at, index);
				}
			}
		};
		append(value.prefix);
		const auto fixed = value.prefix.size() + value.suffix.size();
		if (!value.automatic.empty() && capacity > fixed) {
			const auto repetitions = (capacity - fixed) / value.automatic.size();
			for (std::size_t i = 0; i < repetitions; ++i) append(value.automatic);
		}
		append(value.suffix);
	}

	bool parse_grid_tracks(const css_token_vector& tokens, grid_track_template& tracks) {
		if (tokens.size() == 1 && tokens.front().ident() == "none") {
			tracks = {};
			return true;
		}
		if (!tokens.empty() && tokens.front().ident() == "subgrid") {
			auto lines = parse_subgrid_lines(tokens);
			if (!lines) return false;
			tracks = {};
			tracks.subgrid = std::move(lines);
			return true;
		}
		return parse_track_template(tokens, tracks, true);
	}

	bool parse_grid_auto_tracks(const css_token_vector& tokens, grid_track_list& tracks) {
		if (tokens.empty() || tokens.size() > grid_track_limit) return false;
		grid_track_list parsed(tokens.size());
		for (std::size_t i = 0; i < tokens.size(); ++i)
			if (!parse_track(tokens[i], parsed[i])) return false;
		tracks = std::move(parsed);
		return true;
	}

	bool parse_grid_auto_flow(const css_token_vector& tokens, grid_auto_flow& flow) {
		if (tokens.empty() || tokens.size() > 2) return false;
		grid_auto_flow parsed;
		bool has_direction = false;
		for (const auto& token : tokens) {
			const auto keyword = token.ident();
			if (!has_direction && (keyword == "row" || keyword == "column")) {
				parsed.by_column = keyword == "column";
				has_direction = true;
			} else if (!parsed.dense && keyword == "dense") {
				parsed.dense = true;
			} else return false;
		}
		flow = parsed;
		return true;
	}

	bool parse_grid_template_areas(const css_token_vector& tokens, grid_area_template& areas) {
		if (tokens.size() == 1 && tokens.front().ident() == "none") {
			areas = {};
			return true;
		}
		if (tokens.empty() || tokens.size() > grid_track_limit) return false;

		struct area_cells {
			grid_item_placement bounds;
			std::size_t count = 0;
		};
		std::unordered_map<std::string, area_cells> cells;
		grid_area_template parsed;
		for (const auto& token : tokens) {
			if (token.type != STRING) return false;
			const auto& row = token.str;
			std::size_t column = 0;
			for (std::size_t index = 0; index < row.size();) {
				const auto first = index;
				const auto ch = row[index++];
				if (is_grid_cell_space(ch)) continue;
				if (column == grid_track_limit) return false;
				if (ch == '.') {
					while (index < row.size() && row[index] == '.') ++index;
				} else if (is_grid_cell_name_byte(ch)) {
					while (index < row.size() && is_grid_cell_name_byte(row[index])) ++index;
					const grid_item_placement cell{ { parsed.rows, parsed.rows + 1 }, { column, column + 1 } };
					auto& area = cells.try_emplace(row.substr(first, index - first), area_cells{ cell }).first->second;
					area.bounds.rows.end = cell.rows.end;
					area.bounds.columns.start = std::min(area.bounds.columns.start, column);
					area.bounds.columns.end = std::max(area.bounds.columns.end, column + 1);
					++area.count;
				} else return false;
				++column;
			}
			if (column == 0 || (parsed.rows && parsed.columns != column)) return false;
			parsed.columns = column;
			++parsed.rows;
		}
		for (const auto& [name, area] : cells) {
			if (area.count != static_cast<std::size_t>(area.bounds.rows.end - area.bounds.rows.start) *
				(area.bounds.columns.end - area.bounds.columns.start)) return false;
			parsed.areas.emplace(name, area.bounds);
		}
		areas = std::move(parsed);
		return true;
	}

	namespace {
		bool parse_template_with_areas(const css_token_vector& tokens, grid_definition& definition) {
			css_token_vector area_rows;
			auto& rows = definition.rows.prefix;
			const auto end = tokens.end();
			auto it = tokens.begin();
			while (it != end) {
				if (it->type == SQUARE_BLOCK && !append_grid_line_names(*it++, rows)) return false;
				if (it == end || it->type != STRING || area_rows.size() == grid_track_limit) return false;
				area_rows.push_back(*it++);
				grid_track size;
				if (it != end && it->type != STRING && it->type != SQUARE_BLOCK && !parse_track(*it++, size)) return false;
				rows.tracks.push_back(std::move(size));
				// Trailing names and the next row's leading names belong to the same line.
				if (it != end && it->type == SQUARE_BLOCK && !append_grid_line_names(*it++, rows)) return false;
			}
			return parse_grid_template_areas(area_rows, definition.areas);
		}

		bool starts_auto_flow(const css_token_vector& tokens) {
			return !tokens.empty() && is_one_of(tokens.front().ident(), "auto-flow", "dense");
		}

		bool parse_auto_flow_axis(const css_token_vector& tokens, grid_auto_flow& flow, grid_track_list& sizes) {
			auto it = tokens.begin();
			const auto end = tokens.end();
			const bool dense_first = it->ident() == "dense";
			if (dense_first) ++it;
			if (it == end || it->ident() != "auto-flow") return false;
			++it;
			flow.dense = dense_first;
			if (!dense_first && it != end && it->ident() == "dense") {
				flow.dense = true;
				++it;
			}
			return it == end || parse_grid_auto_tracks(css_token_vector(it, end), sizes);
		}
	}

	bool parse_grid_definition(const css_token_vector& tokens, bool allow_auto_flow, grid_definition& definition) {
		grid_definition parsed;
		if (tokens.size() == 1 && tokens.front().ident() == "none") {
			definition = {};
			return true;
		}
		// Slash tokens inside functions (such as calc()) are already grouped.
		const auto slash = std::find_if(tokens.begin(), tokens.end(), [](const auto& token) { return token.ch == '/'; });
		const bool has_columns = slash != tokens.end();
		const css_token_vector rows(tokens.begin(), slash);
		const css_token_vector columns(has_columns ? slash + 1 : slash, tokens.end());
		if (rows.empty() || (has_columns && columns.empty())) return false;

		const bool auto_rows = starts_auto_flow(rows);
		const bool auto_columns = starts_auto_flow(columns);
		if (auto_rows || auto_columns) {
			if (!allow_auto_flow || !has_columns || (auto_rows && auto_columns)) return false;
			parsed.flow.by_column = auto_columns;
			if (auto_rows) {
				if (!parse_auto_flow_axis(rows, parsed.flow, parsed.auto_rows) || !parse_grid_tracks(columns, parsed.columns))
					return false;
			} else if (!parse_grid_tracks(rows, parsed.rows) || !parse_auto_flow_axis(columns, parsed.flow, parsed.auto_columns)) {
				return false;
			}
		} else if (std::any_of(rows.begin(), rows.end(), [](const auto& token) { return token.type == STRING; })) {
			if (!parse_template_with_areas(rows, parsed) || (has_columns && !parse_track_template(columns, parsed.columns, false)))
				return false;
		} else if (!has_columns || !parse_grid_tracks(rows, parsed.rows) || !parse_grid_tracks(columns, parsed.columns)) {
			return false;
		}
		definition = std::move(parsed);
		return true;
	}

	bool parse_grid_line(const css_token_vector& tokens, grid_line& line) {
		return !tokens.empty() && parse_line(tokens.data(), tokens.data() + tokens.size(), line);
	}

	bool parse_grid_axis_placement(const css_token_vector& tokens, grid_axis_placement& placement) {
		std::array<grid_line, 2> lines;
		const auto count = parse_grid_lines(tokens, lines.data(), lines.size());
		if (count == 0) return false;
		if (count == 1) lines[1] = omitted_grid_line(lines[0]);
		placement = { std::move(lines[0]), std::move(lines[1]) };
		return true;
	}

	bool parse_grid_area(const css_token_vector& tokens, std::array<grid_line, 4>& lines) {
		const auto count = parse_grid_lines(tokens, lines.data(), lines.size());
		if (count == 0) return false;
		if (count < 2) lines[1] = omitted_grid_line(lines[0]);
		if (count < 3) lines[2] = omitted_grid_line(lines[0]);
		if (count < 4) lines[3] = omitted_grid_line(lines[1]);
		return true;
	}

}
