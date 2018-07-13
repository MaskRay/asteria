// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_INSTANTIATED_FUNCTION_HPP_
#define ASTERIA_INSTANTIATED_FUNCTION_HPP_

#include "fwd.hpp"
#include "function_base.hpp"

namespace Asteria {

class Function : public Function_base {
private:
	const char * m_category;
	Cow_string m_source;

	Vector<Cow_string> m_params;
	Block m_bound_body;

public:
	Function(const char *category, Cow_string source, Vector<Cow_string> params, Block bound_body)
		: m_category(category), m_source(std::move(source))
		, m_params(std::move(params)), m_bound_body(std::move(bound_body))
	{ }
	~Function() override;

public:
	Cow_string describe() const override;
	void invoke(Vp<Reference> &result_out, Vp<Reference> &&this_opt, Vector<Vp<Reference>> &&args) const override;
};

}

#endif