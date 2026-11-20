// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#ifndef LH_MEDIA_QUERY_H
#define LH_MEDIA_QUERY_H

#include "css_length.h"
#include "css_token_compat.h"
#include <optional>
#include "css_string_id.h"
#include "css_types.h"

namespace litehtml
{
	class document;
	class html_tag;
	// https://drafts.csswg.org/mediaqueries/#evaluating
	enum trilean
	{
		False,
		True,
		Unknown
	};
	static_assert(False == false && True == true); // for casting bool to trilean

	inline trilean operator!(trilean x)
	{
		if (x == False) return True;
		if (x == True) return False;
		return Unknown;
	}

	inline trilean operator&&(trilean a, trilean b)
	{
		if (a == False || b == False) return False;
		if (a == True && b == True) return True;
		return Unknown;
	}

	inline trilean operator||(trilean a, trilean b)
	{
		if (a == True || b == True) return True;
		if (a == False && b == False) return False;
		return Unknown;
	}


	struct media_condition;

	// <media-query> = <media-condition> | [ not | only ]? <media-type> [ and <media-condition-without-or> ]?
	struct media_query
	{
		bool						m_not         = true;
		media_type					m_media_type  = media_type_all;
		vector<media_condition>		m_conditions;

		trilean check(const media_features& features) const;
	};

	struct media_in_parens;
	struct query_length_environment
	{
		const font_metrics& font;
		css_length_context units;
		const html_tag* container = nullptr;
	};

	// <media-condition> = not <media-in-parens> | <media-in-parens> [ [and <media-in-parens>]* | [or <media-in-parens>]* ]
	struct media_condition
	{
		string_id					op = _and_; // _not_ _and_ _or_
		vector<media_in_parens>		m_conditions;

		trilean check(const media_features& features, const query_length_environment* lengths = nullptr) const;
	};

	// <media-feature> = ( [ <mf-plain> | <mf-boolean> | <mf-range> ] )
	// <mf-plain> = <mf-name> : <mf-value>
	struct media_feature
	{
		using threshold = variant<float, css_length, css_token_vector>;

		string name;
		threshold first = 0.0f;
		threshold second = 0.0f;
		short op = 0;
		short op2 = 0;
		std::weak_ptr<document> length_document;
		font_metrics length_font;

		bool verify_and_convert_units(string_id syntax, css_token val[2], css_token val2[2], shared_ptr<document> doc, bool container);

		std::optional<float> resolve_threshold(const threshold& value, const media_features& features,
			const query_length_environment* lengths) const;
		trilean compare(float x, const media_features& features, const query_length_environment* lengths) const;
		trilean check(const media_features& features, const query_length_environment* lengths) const;
	};

	struct style_query_feature
	{
		string_id name;
		std::optional<css_token_vector> value;

		bool check(const html_tag& container) const;
	};

	// <media-in-parens> = ( <media-condition> ) | <media-feature> | <general-enclosed>
	struct unknown {}; // <general-enclosed>
	struct media_in_parens : variant<media_condition, media_feature, style_query_feature, unknown>
	{
		using base::base; // inherit ctors

		trilean check(const media_features& features, const query_length_environment* lengths) const;
	};

	enum query_axes : unsigned { query_inline = 1, query_block = 2, query_unknown = 4 };

	struct container_condition
	{
		string name;
		std::optional<media_condition> condition;
		unsigned axes = 0;
	};

	struct container_query
	{
		vector<container_condition> conditions;
		shared_ptr<const container_query> parent;
	};

	bool is_container_name(const css_token& token);
	std::optional<vector<container_condition>> parse_container_conditions(const css_token_vector& tokens, const shared_ptr<document>& doc);

	struct media_query_list
	{
		vector<media_query> m_queries;
		bool empty() const { return m_queries.empty(); }
		bool check(const media_features& features) const;
	};
	media_query_list parse_media_query_list(const css_token_vector& tokens, shared_ptr<document> doc);
	media_query_list parse_media_query_list(const string& str, shared_ptr<document> doc);
	bool matches_media_condition(const css_token_vector& tokens, const shared_ptr<document>& doc, const media_features& features);


	class media_query_list_list
	{
	public:
		using ptr    = shared_ptr<media_query_list_list>;
		using vector = std::vector<ptr>;
	private:
		std::vector<media_query_list>	m_media_query_lists;
		bool							m_is_used = false;
	public:
		void add(const media_query_list& mq_list)
		{
			m_media_query_lists.push_back(mq_list);
		}
		bool is_used() const { return m_is_used; }

		bool apply_media_features(const media_features& features); // returns true if the m_is_used changed
	};

}

#endif  // LH_MEDIA_QUERY_H
