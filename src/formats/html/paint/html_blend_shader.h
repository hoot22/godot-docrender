#pragma once

namespace dockit {
	// Mode indices match css_blend_mode. The two inputs and output are premultiplied viewport textures.
	inline constexpr const char* html_blend_shader = R"(
shader_type canvas_item;
render_mode blend_disabled, unshaded;
uniform int mode;
uniform bool hdr;
uniform sampler2D backdrop : filter_nearest, repeat_disable;
#ifdef HTML_ELEMENT_BLEND
uniform vec4 backdrop_rect;
varying float group_opacity;
varying vec2 backdrop_uv;

void vertex() {
	group_opacity = COLOR.a;
	backdrop_uv = (VERTEX - backdrop_rect.xy) / backdrop_rect.zw;
}
#endif

vec3 from_linear(vec3 c) {
	return mix(12.92 * c, 1.0 - 1.055 * (1.0 - pow(max(c, vec3(0.0)), vec3(1.0 / 2.4))),
		greaterThan(c, vec3(0.0031308)));
}

vec3 to_linear(vec3 c) {
	return mix(c / 12.92, pow(max((c + 0.055) / 1.055, vec3(0.0)), vec3(2.4)),
		greaterThan(c, vec3(0.04045)));
}

float hard_light(float b, float s) {
	return s <= 0.5 ? 2.0 * b * s : 1.0 - 2.0 * (1.0 - b) * (1.0 - s);
}

float soft_light(float b, float s) {
	if (s <= 0.5) return b - (1.0 - 2.0 * s) * b * (1.0 - b);
	float curve = b <= 0.25 ? ((16.0 * b - 12.0) * b + 4.0) * b : sqrt(b);
	return b + (2.0 * s - 1.0) * (curve - b);
}

float blend_channel(float b, float s) {
	if (mode == 1) return b * s;
	if (mode == 2) return b + s - b * s;
	if (mode == 3) return hard_light(s, b);
	if (mode == 4) return min(b, s);
	if (mode == 5) return max(b, s);
	if (mode == 6) {
		if (b == 0.0) return 0.0;
		return s == 1.0 ? 1.0 : min(1.0, b / (1.0 - s));
	}
	if (mode == 7) {
		if (b == 1.0) return 1.0;
		return s == 0.0 ? 0.0 : 1.0 - min(1.0, (1.0 - b) / s);
	}
	if (mode == 8) return hard_light(b, s);
	if (mode == 9) return soft_light(b, s);
	if (mode == 10) return abs(b - s);
	if (mode == 11) return b + s - 2.0 * b * s;
	return s;
}

float luminosity(vec3 c) {
	return dot(c, vec3(0.3, 0.59, 0.11));
}

float saturation(vec3 c) {
	return max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b);
}

vec3 set_saturation(vec3 c, float target) {
	float spread = saturation(c);
	return spread > 0.0 ? (c - min(min(c.r, c.g), c.b)) * (target / spread) : vec3(0.0);
}

vec3 set_luminosity(vec3 c, float lightness) {
	c += lightness - luminosity(c);
	float low = min(min(c.r, c.g), c.b);
	float high = max(max(c.r, c.g), c.b);
	if (low < 0.0) c = lightness + (c - lightness) * (lightness / (lightness - low));
	if (high > 1.0) c = lightness + (c - lightness) * ((1.0 - lightness) / (high - lightness));
	return c;
}

vec3 blend_color(vec3 b, vec3 s) {
	if (mode == 12) return set_luminosity(set_saturation(s, saturation(b)), luminosity(b));
	if (mode == 13) return set_luminosity(set_saturation(b, saturation(s)), luminosity(b));
	if (mode == 14) return set_luminosity(s, luminosity(b));
	if (mode == 15) return set_luminosity(b, luminosity(s));
	return vec3(blend_channel(b.r, s.r), blend_channel(b.g, s.g), blend_channel(b.b, s.b));
}

vec4 composite(vec4 b, vec4 s) {
	if (b.a <= 0.0) return s;
	if (s.a <= 0.0) return b;
	vec3 cb = b.rgb / b.a;
	vec3 cs = s.rgb / s.a;
	if (hdr) {
		cb = from_linear(cb);
		cs = from_linear(cs);
	}
	cb = clamp(cb, vec3(0.0), vec3(1.0));
	cs = clamp(cs, vec3(0.0), vec3(1.0));
	float alpha;
	vec3 rgb;
	if (mode == 16) {
		alpha = min(1.0, s.a + b.a);
		rgb = min(vec3(1.0), cs * s.a + cb * b.a);
	} else {
		vec3 mixed = mix(cs, blend_color(cb, cs), b.a);
		float retained = b.a * (1.0 - s.a);
		alpha = s.a + retained;
		rgb = mixed * s.a + cb * retained;
	}
	if (hdr) rgb = to_linear(rgb / alpha) * alpha;
	return vec4(rgb, alpha);
}

void fragment() {
#ifdef HTML_ELEMENT_BLEND
	COLOR = composite(texture(backdrop, backdrop_uv), texture(TEXTURE, UV) * group_opacity);
#else
	COLOR = composite(texture(backdrop, UV), texture(TEXTURE, UV));
#endif
}
)";
}
