#include "html_gauge_elements.h"

#include "html_gauge_node.h"
#include "html_form_state.h"
#include "formats/html/dom/html_dom.h"
#include "formats/html/markup/html_number.h"

#include <algorithm>
#include <cmath>

using namespace godot;
using namespace litehtml;

namespace {
	struct ProgressValue {
		double value;
		double max;
		bool indeterminate;
	};

	ProgressValue progress_value(const element* p_node) {
		const auto raw_max = dockit::html_number(p_node ? p_node->get_attr("max") : nullptr).value_or(1);
		const double max = raw_max > 0 ? raw_max : 1;
		const auto* raw_value = p_node ? p_node->get_attr("value") : nullptr;
		return { std::clamp(dockit::html_number(raw_value).value_or(0), 0.0, max), max, !raw_value };
	}

	struct MeterValue {
		double min;
		double max;
		double value;
		double low;
		double high;
		double optimum;
	};

	MeterValue meter_value(const element* p_node) {
		const auto number = [&](const char* p_name, double p_default) {
			return dockit::html_number(p_node ? p_node->get_attr(p_name) : nullptr).value_or(p_default);
		};
		const double min = number("min", 0);
		const double max = std::max(min, number("max", 1));
		const double low = std::clamp(number("low", min), min, max);
		return { min, max, std::clamp(number("value", 0), min, max), low,
			std::clamp(number("high", max), low, max),
			std::clamp(number("optimum", min / 2 + max / 2), min, max) };
	}

	double meter_ratio(const MeterValue& p_meter) {
		if (p_meter.max == p_meter.min) return 0;
		const double span = p_meter.max - p_meter.min;
		if (std::isfinite(span)) return (p_meter.value - p_meter.min) / span;
		return (p_meter.value / 2 - p_meter.min / 2) / (p_meter.max / 2 - p_meter.min / 2);
	}
}

dockit::HTMLGaugeReading dockit::read_html_gauge(const element& p_node) {
	if (p_node.tag() == _progress_) {
		const auto progress = progress_value(&p_node);
		return { progress.value / progress.max, progress.indeterminate, 0 };
	}
	const auto meter = meter_value(&p_node);
	int zone = 0;
	if (meter.optimum < meter.low) zone = meter.value <= meter.low ? 0 : meter.value <= meter.high ? 1 : 2;
	else if (meter.optimum > meter.high) zone = meter.value >= meter.high ? 0 : meter.value >= meter.low ? 1 : 2;
	else zone = meter.value >= meter.low && meter.value <= meter.high ? 0 : 1;
	return { meter_ratio(meter), false, zone };
}

void HTMLProgressElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_value"), &HTMLProgressElement::get_value);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &HTMLProgressElement::set_value);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "value"), "set_value", "get_value");
	ClassDB::bind_method(D_METHOD("get_max"), &HTMLProgressElement::get_max);
	ClassDB::bind_method(D_METHOD("set_max", "value"), &HTMLProgressElement::set_max);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max"), "set_max", "get_max");
	ClassDB::bind_method(D_METHOD("get_position"), &HTMLProgressElement::get_position);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "position"), "", "get_position");
	ClassDB::bind_method(D_METHOD("get_labels"), &HTMLProgressElement::get_labels);
}

double HTMLProgressElement::get_value() const {
	const auto node = lock_element();
	return progress_value(node.get()).value;
}
void HTMLProgressElement::set_value(double p_value) {
	if (std::isfinite(p_value)) set_attribute("value", String::num_real(p_value));
}

double HTMLProgressElement::get_max() const {
	const auto node = lock_element();
	return progress_value(node.get()).max;
}
void HTMLProgressElement::set_max(double p_value) {
	if (std::isfinite(p_value) && p_value > 0) set_attribute("max", String::num_real(p_value));
}

double HTMLProgressElement::get_position() const {
	const auto node = lock_element();
	const auto progress = progress_value(node.get());
	return progress.indeterminate ? -1 : progress.value / progress.max;
}

TypedArray<HTMLElement> HTMLProgressElement::get_labels() const {
	const auto node = lock_element();
	TypedArray<HTMLElement> result;
	if (node) for (const auto& label : dockit::forms::labels(*node)) result.append(get_dom().make_element(label));
	return result;
}

void HTMLMeterElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_min"), &HTMLMeterElement::get_min);
	ClassDB::bind_method(D_METHOD("set_min", "value"), &HTMLMeterElement::set_min);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min"), "set_min", "get_min");
	ClassDB::bind_method(D_METHOD("get_max"), &HTMLMeterElement::get_max);
	ClassDB::bind_method(D_METHOD("set_max", "value"), &HTMLMeterElement::set_max);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max"), "set_max", "get_max");
	ClassDB::bind_method(D_METHOD("get_value"), &HTMLMeterElement::get_value);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &HTMLMeterElement::set_value);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "value"), "set_value", "get_value");
	ClassDB::bind_method(D_METHOD("get_low"), &HTMLMeterElement::get_low);
	ClassDB::bind_method(D_METHOD("set_low", "value"), &HTMLMeterElement::set_low);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "low"), "set_low", "get_low");
	ClassDB::bind_method(D_METHOD("get_high"), &HTMLMeterElement::get_high);
	ClassDB::bind_method(D_METHOD("set_high", "value"), &HTMLMeterElement::set_high);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "high"), "set_high", "get_high");
	ClassDB::bind_method(D_METHOD("get_optimum"), &HTMLMeterElement::get_optimum);
	ClassDB::bind_method(D_METHOD("set_optimum", "value"), &HTMLMeterElement::set_optimum);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "optimum"), "set_optimum", "get_optimum");
	ClassDB::bind_method(D_METHOD("get_labels"), &HTMLMeterElement::get_labels);
}

double HTMLMeterElement::get_min() const {
	const auto node = lock_element();
	return meter_value(node.get()).min;
}
void HTMLMeterElement::set_min(double p_value) {
	if (std::isfinite(p_value)) set_attribute("min", String::num_real(p_value));
}

double HTMLMeterElement::get_max() const {
	const auto node = lock_element();
	return meter_value(node.get()).max;
}
void HTMLMeterElement::set_max(double p_value) {
	if (std::isfinite(p_value)) set_attribute("max", String::num_real(p_value));
}

double HTMLMeterElement::get_value() const {
	const auto node = lock_element();
	return meter_value(node.get()).value;
}
void HTMLMeterElement::set_value(double p_value) {
	if (std::isfinite(p_value)) set_attribute("value", String::num_real(p_value));
}

double HTMLMeterElement::get_low() const {
	const auto node = lock_element();
	return meter_value(node.get()).low;
}
void HTMLMeterElement::set_low(double p_value) {
	if (std::isfinite(p_value)) set_attribute("low", String::num_real(p_value));
}

double HTMLMeterElement::get_high() const {
	const auto node = lock_element();
	return meter_value(node.get()).high;
}
void HTMLMeterElement::set_high(double p_value) {
	if (std::isfinite(p_value)) set_attribute("high", String::num_real(p_value));
}

double HTMLMeterElement::get_optimum() const {
	const auto node = lock_element();
	return meter_value(node.get()).optimum;
}
void HTMLMeterElement::set_optimum(double p_value) {
	if (std::isfinite(p_value)) set_attribute("optimum", String::num_real(p_value));
}

TypedArray<HTMLElement> HTMLMeterElement::get_labels() const {
	const auto node = lock_element();
	TypedArray<HTMLElement> result;
	if (node) for (const auto& label : dockit::forms::labels(*node)) result.append(get_dom().make_element(label));
	return result;
}
