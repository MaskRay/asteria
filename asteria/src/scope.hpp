// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_SCOPE_HPP_
#define ASTERIA_SCOPE_HPP_

#include "fwd.hpp"

namespace Asteria {

class Scope {
public:
	enum Purpose {
		purpose_lexical   = 0,
		purpose_plain     = 1,
		purpose_function  = 2,
		purpose_file      = 3,
	};

private:
	const Purpose m_purpose;
	const Sptr<const Scope> m_parent_opt;

	Xptr_map<std::string, Reference> m_local_references;
	Sptr_vector<const Function_base> m_deferred_callbacks;

public:
	Scope(Purpose purpose, Sptr<const Scope> parent_opt)
		: m_purpose(std::move(purpose)), m_parent_opt(std::move(parent_opt))
	{ }
	~Scope();

	Scope(const Scope &) = delete;
	Scope & operator=(const Scope &) = delete;

private:
	void do_dispose_deferred_callbacks() noexcept;

public:
	Purpose get_purpose() const noexcept {
		return m_purpose;
	}
	const Sptr<const Scope> & get_parent_opt() const noexcept {
		return m_parent_opt;
	}

	Sptr<const Reference> get_local_reference_opt(const std::string &identifier) const noexcept;
	std::reference_wrapper<Xptr<Reference>> drill_for_local_reference(const std::string &identifier);

	void defer_callback(Sptr<const Function_base> &&callback);
};

using Parameter_vector = std::vector<Parameter>;

extern void prepare_function_scope(Spcref<Scope> scope, Spcref<Recycler> recycler, Spcref<const Parameter_vector> parameters_opt, Xptr<Reference> &&this_opt, Xptr_vector<Reference> &&arguments_opt);
extern void prepare_function_scope_lexical(Spcref<Scope> scope, Spcref<const Parameter_vector> parameters_opt);

}

#endif