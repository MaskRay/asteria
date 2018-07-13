// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_REFERENCE_HPP_
#define ASTERIA_REFERENCE_HPP_

#include "fwd.hpp"

namespace Asteria {


class Stored_reference;

class Variable {
private:
	Value m_value_opt;
	bool m_immutable;

public:
	explicit Variable(bool immutable = false)
		: m_value_opt(), m_immutable(immutable)
	{ }
	Variable(Value &&value_opt, bool immutable = false)
		: m_value_opt(std::move(value_opt)), m_immutable(immutable)
	{ }
	~Variable();

	Variable(const Variable &) = delete;
	Variable & operator=(const Variable &) = delete;

private:
	ROCKET_NORETURN void do_throw_immutable() const;

public:
	Vp_cref<const Value> get_value_opt() const noexcept {
		return m_value_opt;
	}
	std::reference_wrapper<Value> mutate_value(){
		const auto immutable = is_immutable();
		if(immutable){
			do_throw_immutable();
		}
		return std::ref(m_value_opt);
	}

	bool is_immutable() const noexcept {
		return m_immutable;
	}
	void set_immutable(bool immutable = true) noexcept {
		m_immutable = immutable;
	}
};

}


// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#include "fwd.hpp"
#include "rocket/variant.hpp"

namespace Asteria {

class Reference {
	friend Stored_reference;

public:
	enum Index : std::uint8_t {
		index_null             = -1,
		index_constant         =  0,
		index_temporary_value  =  1,
		index_variable         =  2,
		index_array_element    =  3,
		index_object_member    =  4,
	};
	struct S_constant {
		Sp<const Value> src_opt;
	};
	struct S_temporary_value {
		Value value_opt;
	};
	struct S_variable {
		Sp<Variable> variable;
	};
	struct S_array_element {
		Vp<Reference> parent_opt;
		Signed_integer index;
	};
	struct S_object_member {
		Vp<Reference> parent_opt;
		Cow_string key;
	};
	using Variant = rocket::variant<ROCKET_CDR(
		, S_constant         //  0
		, S_temporary_value  //  1
		, S_variable         //  2
		, S_array_element    //  3
		, S_object_member    //  4
	)>;

private:
	Variant m_variant;

public:
	template<typename CandidateT, typename std::enable_if<std::is_constructible<Variant, CandidateT &&>::value>::type * = nullptr>
	Reference(CandidateT &&cand)
		: m_variant(std::forward<CandidateT>(cand))
	{ }
	~Reference();

	Reference(const Reference &) = delete;
	Reference & operator=(const Reference &) = delete;

public:
	Index which() const noexcept {
		return static_cast<Index>(m_variant.index());
	}
	template<typename ExpectT>
	const ExpectT * get_opt() const noexcept {
		return m_variant.get<ExpectT>();
	}
	template<typename ExpectT>
	ExpectT * get_opt() noexcept {
		return m_variant.get<ExpectT>();
	}
	template<typename ExpectT>
	const ExpectT & as() const {
		return m_variant.as<ExpectT>();
	}
	template<typename ExpectT>
	ExpectT & as() {
		return m_variant.as<ExpectT>();
	}
	template<typename CandidateT>
	void set(CandidateT &&cand){
		m_variant = std::forward<CandidateT>(cand);
	}
};

extern Reference::Index get_reference_type(Sp_cref<const Reference> reference_opt) noexcept;

extern void dump_reference(std::ostream &os, Sp_cref<const Reference> reference_opt, unsigned indent_next = 0, unsigned indent_increment = 2);
//extern std::ostream & operator<<(std::ostream &os, const Sp_formatter<Reference> &reference_fmt);

extern void copy_reference(Vp<Reference> &ref_out, Sp_cref<const Reference> src_opt);
extern void move_reference(Vp<Reference> &ref_out, Vp<Reference> &&src_opt);

extern Sp<const Value> read_reference_opt(Sp_cref<const Reference> reference_opt);
extern std::reference_wrapper<Value> drill_reference(Sp_cref<const Reference> reference_opt);

// If you do not have an `Vp<Reference>` but an `Sp<const Reference>`, use the following code to copy the value through the reference:
//   `copy_value(value_out, read_reference_opt(reference_opt))`
extern void extract_value_from_reference(Value &value_out, Vp<Reference> &&reference_opt);

// If the reference is a temporary value, convert it to an unnamed variable, allowing further modification to it.
extern void materialize_reference(Vp<Reference> &reference_inout_opt, bool immutable);

}

#endif