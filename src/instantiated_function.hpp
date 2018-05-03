// This file is part of asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_INSTANTIATED_FUNCTION_HPP_
#define ASTERIA_INSTANTIATED_FUNCTION_HPP_

#include "fwd.hpp"
#include "function_base.hpp"

namespace Asteria {

class Instantiated_function : public Function_base {
private:
	Sptr<const std::vector<Parameter>> m_parameters_opt;
	Sptr<const Block> m_body_opt;

public:
	Instantiated_function(Sptr<const std::vector<Parameter>> parameters_opt, Sptr<const Block> body_opt)
		: m_parameters_opt(std::move(parameters_opt)), m_body_opt(std::move(body_opt))
	{ }
	~Instantiated_function();

	Instantiated_function(const Instantiated_function &) = delete;
	Instantiated_function &operator=(const Instantiated_function &) = delete;

public:
	const char *describe() const noexcept override;
	void invoke(Xptr<Reference> &result_out, Spcref<Recycler> recycler, Xptr<Reference> &&this_opt, Xptr_vector<Reference> &&arguments_opt) const override;
};

}

#endif