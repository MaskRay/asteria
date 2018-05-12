// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_REFERENCE_HPP_
#define ASTERIA_REFERENCE_HPP_

#include "fwd.hpp"
#include "rocket/variant.hpp"

namespace Asteria {

class Reference {
	friend Stored_reference;

public:
	enum Type : unsigned {
		type_null             = -1u,
		type_constant         =  0,
		type_temporary_value  =  1,
		type_local_variable   =  2,
		type_array_element    =  3,
		type_object_member    =  4,
	};
	struct S_constant {
		Sptr<const Variable> source_opt;
	};
	struct S_temporary_value {
		Xptr<Variable> variable_opt;
	};
	struct S_local_variable {
		Sptr<Local_variable> local_variable;
	};
	struct S_array_element {
		Xptr<Reference> parent_opt;
		std::int64_t index;
	};
	struct S_object_member {
		Xptr<Reference> parent_opt;
		std::string key;
	};
	using Variant = rocket::variant<ASTERIA_CDR(void
		, S_constant         //  0
		, S_temporary_value  //  1
		, S_local_variable   //  2
		, S_array_element    //  3
		, S_object_member    //  4
	)>;

private:
	Variant m_variant;

public:
	template<typename ValueT, ASTERIA_UNLESS_IS_BASE_OF(Reference, ValueT)>
	Reference(ValueT &&value)
		: m_variant(std::forward<ValueT>(value))
	{ }
	~Reference();

	Reference(const Reference &) = delete;
	Reference & operator=(const Reference &) = delete;

public:
	Type get_type() const noexcept {
		return static_cast<Type>(m_variant.index());
	}
	template<typename ExpectT>
	const ExpectT * get_opt() const noexcept {
		return m_variant.try_get<ExpectT>();
	}
	template<typename ExpectT>
	ExpectT * get_opt() noexcept {
		return m_variant.try_get<ExpectT>();
	}
	template<typename ExpectT>
	const ExpectT & get() const {
		return m_variant.get<ExpectT>();
	}
	template<typename ExpectT>
	ExpectT & get(){
		return m_variant.get<ExpectT>();
	}
	template<typename ValueT>
	void set(ValueT &&value){
		m_variant = std::forward<ValueT>(value);
	}
};

extern Reference::Type get_reference_type(Spcref<const Reference> reference_opt) noexcept;

extern void dump_reference(std::ostream &os, Spcref<const Reference> reference_opt, unsigned indent_next = 0, unsigned indent_increment = 2);
extern std::ostream & operator<<(std::ostream &os, const Sptr_fmt<Reference> &reference_fmt);

extern void copy_reference(Xptr<Reference> &reference_out, Spcref<const Reference> source_opt);
extern void move_reference(Xptr<Reference> &reference_out, Xptr<Reference> &&source_opt);

extern Sptr<const Variable> read_reference_opt(Spcref<const Reference> reference_opt);
extern std::reference_wrapper<Xptr<Variable>> drill_reference(Spcref<const Reference> reference_opt);

// If you do not have an `Xptr<Reference>` but an `Sptr<const Reference>`, use the following code to copy the variable through the reference:
//   `copy_variable(variable_out, recycler, read_reference_opt(reference_opt))`
extern void extract_variable_from_reference(Xptr<Variable> &variable_out, Spcref<Recycler> recycler, Xptr<Reference> &&reference_opt);
// If the reference is a temporary value, convert it to an unnamed local variable, allowing further modification to it.
extern void materialize_reference(Xptr<Reference> &reference_inout_opt, Spcref<Recycler> recycler, bool constant);

}

#endif