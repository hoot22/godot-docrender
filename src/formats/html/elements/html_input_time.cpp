#include "html_input_time.h"

#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cmath>
#include <cstdint>

using namespace godot;

namespace dockit::input_time {
namespace {
	// Gregorian civil-date conversion using 400-year eras, with 1970-01-01 as day zero.
	struct Civil { int year; int month; int day; };

	int64_t floor_div(int64_t p_value, int64_t p_unit) {
		return (p_value - (p_value < 0 ? p_unit - 1 : 0)) / p_unit;
	}

	int64_t days_from_civil(int p_year, int p_month, int p_day) {
		p_year -= p_month <= 2;
		const int64_t era = floor_div(p_year, 400);
		const int y = p_year - era * 400;
		const int m = p_month + (p_month > 2 ? -3 : 9);
		return era * 146097 + y * 365 + y / 4 - y / 100 + (153 * m + 2) / 5 + p_day - 719469;
	}

	Civil civil_from_days(int64_t p_days) {
		p_days += 719468;
		const int64_t era = floor_div(p_days, 146097);
		const int doe = p_days - era * 146097;
		const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		const int mp = (5 * doy + 2) / 153;
		const int m = mp + (mp < 10 ? 3 : -9);
		return { int(yoe + era * 400 + (m <= 2)), m, doy - (153 * mp + 2) / 5 + 1 };
	}

	int64_t first_week(int year) {
		const auto jan4 = days_from_civil(year, 1, 4);
		return floor_div(jan4 + 3, 7) * 7 - 3;
	}

	std::optional<int> digits(const String& p_string, int p_min, int p_max) {
		if (p_string.length() < p_min || p_string.length() > p_max) return {};
		int result = 0;
		for (int i = 0; i < p_string.length(); ++i) {
			if (p_string[i] < '0' || p_string[i] > '9') return {};
			result = result * 10 + (p_string[i] - '0');
		}
		return result;
	}

	std::optional<int64_t> parse_clock(const String& p_value) {
		const auto fields = p_value.split(":", true);
		if (fields.size() < 2 || fields.size() > 3) return {};
		const auto hour = digits(fields[0], 2, 2);
		const auto minute = digits(fields[1], 2, 2);
		if (!hour || *hour > 23 || !minute || *minute > 59) return {};
		int64_t result = *hour * 3600000 + *minute * 60000;
		if (fields.size() == 3) {
			const auto seconds = fields[2].split(".", true);
			const auto second = digits(seconds[0], 2, 2);
			if (!second || *second > 59 || seconds.size() > 2) return {};
			result += *second * 1000;
			if (seconds.size() == 2) {
				const auto ms = digits(seconds[1], 1, 3);
				if (!ms) return {};
				result += *ms * (seconds[1].length() == 1 ? 100 : seconds[1].length() == 2 ? 10 : 1);
			}
		}
		return result;
	}

	String clock_string(int64_t ms) {
		String result = vformat("%02d:%02d", ms / 3600000, ms / 60000 % 60);
		if (ms % 60000) result += vformat(":%02d", ms / 1000 % 60);
		if (ms % 1000) {
			String fraction = vformat("%03d", ms % 1000);
			while (fraction.ends_with("0")) fraction = fraction.substr(0, fraction.length() - 1);
			result += "." + fraction;
		}
		return result;
	}
}

std::optional<double> parse(Kind p_kind, const String& p_value) {
	if (p_kind == Kind::none || p_value.is_empty()) return {};
	if (p_kind == Kind::time) {
		const auto clock = parse_clock(p_value);
		return clock ? std::optional<double>(static_cast<double>(*clock)) : std::nullopt;
	}
	if (p_kind == Kind::local) {
		auto separator = p_value.find("T");
		if (separator < 0) separator = p_value.find(" ");
		if (separator < 0) return {};
		const auto date = parse(Kind::date, p_value.substr(0, separator));
		const auto time = parse_clock(p_value.substr(separator + 1));
		return date && time ? std::optional<double>(*date + *time) : std::nullopt;
	}
	const auto fields = p_value.split("-", true);
	if (fields.size() != (p_kind == Kind::date ? 3 : 2)) return {};
	const auto year = digits(fields[0], 4, 6);
	if (!year || *year == 0 || *year > 275760) return {};
	if (p_kind == Kind::week) {
		if (!fields[1].begins_with("W")) return {};
		const auto week = digits(fields[1].substr(1), 2, 2);
		if (!week || *week == 0) return {};
		const auto monday = first_week(*year) + (*week - 1) * 7;
		return monday < first_week(*year + 1) ? std::optional<double>(monday * day) : std::nullopt;
	}
	const auto month = digits(fields[1], 2, 2);
	if (!month || *month == 0 || *month > 12) return {};
	if (p_kind == Kind::month) return (*year - 1970) * 12 + *month - 1;
	const auto dom = digits(fields[2], 2, 2);
	if (!dom || *dom == 0 || *dom > 31) return {};
	const auto candidate = days_from_civil(*year, *month, *dom);
	return civil_from_days(candidate).month == *month ? std::optional<double>(candidate * day) : std::nullopt;
}

String format(Kind p_kind, double p_number) {
	if (!std::isfinite(p_number) || std::abs(p_number) >= 9e15) return {};
	const auto value = static_cast<int64_t>(std::floor(p_number));
	if (p_kind == Kind::time) return value >= 0 && value < day ? clock_string(value) : String();
	if (p_kind == Kind::none) return {};
	if (p_kind == Kind::month) {
		const int64_t year = 1970 + floor_div(value, 12);
		return year > 0 && year <= 275760 ? vformat("%04d-%02d", year, value - (year - 1970) * 12 + 1) : String();
	}
	const auto days = floor_div(value, static_cast<int64_t>(day));
	const auto civil = civil_from_days(days);
	if (p_kind == Kind::week) {
		const auto monday = floor_div(days + 3, 7) * 7 - 3;
		const auto year = civil_from_days(monday + 3).year;
		return year > 0 && year <= 275760 ? vformat("%04d-W%02d", year, (monday - first_week(year)) / 7 + 1) : String();
	}
	if (civil.year <= 0 || civil.year > 275760) return {};
	const auto date = vformat("%04d-%02d-%02d", civil.year, civil.month, civil.day);
	return p_kind == Kind::local ? date + String("T") + clock_string(value - days * static_cast<int64_t>(day)) : date;
}

Dictionary parts(Kind p_kind, double p_number) {
	Dictionary result;
	if (format(p_kind, p_number).is_empty()) return result;
	if (p_kind == Kind::month) p_number = *parse(Kind::date, format(p_kind, p_number) + "-01");
	if (p_kind == Kind::week) p_number = *parse(Kind::week, format(p_kind, p_number));
	const auto value = static_cast<int64_t>(std::floor(p_number));
	const auto days = floor_div(value, static_cast<int64_t>(day));
	const auto civil = civil_from_days(days);
	const auto clock = value - days * static_cast<int64_t>(day);
	result["year"] = civil.year;
	result["month"] = civil.month;
	result["day"] = civil.day;
	result["hour"] = clock / 3600000;
	result["minute"] = clock / 60000 % 60;
	result["second"] = clock / 1000 % 60;
	result["millisecond"] = clock % 1000;
	return result;
}

std::optional<double> from_parts(Kind p_kind, const Dictionary& p) {
	const int64_t year = p.get("year", 1970), month = p.get("month", 1), dom = p.get("day", 1);
	const int64_t hour = p.get("hour", 0), minute = p.get("minute", 0), second = p.get("second", 0), ms = p.get("millisecond", 0);
	if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59 || ms < 0 || ms > 999) return {};
	const auto clock = hour * 3600000 + minute * 60000 + second * 1000 + ms;
	if (p_kind == Kind::time) return static_cast<double>(clock);
	const auto date = parse(Kind::date, vformat("%04d-%02d-%02d", year, month, dom));
	if (!date) return {};
	const auto value = *date + (p_kind == Kind::local ? clock : 0);
	return p_kind == Kind::month || p_kind == Kind::week ?
		parse(p_kind, p_kind == Kind::month ? vformat("%04d-%02d", year, month) : format(p_kind, value)) :
		std::optional<double>(value);
}

double scale(Kind p_kind) {
	switch (p_kind) {
		case Kind::date: return day;
		case Kind::week: return day * 7;
		case Kind::time: case Kind::local: return 1000;
		default: return 1;
	}
}

double step(Kind p_kind) { return p_kind == Kind::time || p_kind == Kind::local ? 60 : 1; }
double base(Kind p_kind) { return p_kind == Kind::week ? -3 * day : 0; }
}
