// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_EXPRESSION_NODE_HPP_
#define ASTERIA_EXPRESSION_NODE_HPP_

#include "fwd.hpp"
#include "rocket/variant.hpp"

namespace Asteria {

class Expression_node {
public:
	enum Operator : std::uint8_t {
		// Postfix operators
		operator_postfix_inc    = 10, // ++
		operator_postfix_dec    = 11, // --
		operator_postfix_at     = 12, // []
		// Prefix operators.
		operator_prefix_pos     = 30, // +
		operator_prefix_neg     = 31, // -
		operator_prefix_notb    = 32, // ~
		operator_prefix_notl    = 33, // !
		operator_prefix_inc     = 34, // ++
		operator_prefix_dec     = 35, // --
		// Infix relational operators.
		operator_infix_cmp_eq   = 50, // ==
		operator_infix_cmp_ne   = 51, // !=
		operator_infix_cmp_lt   = 52, // <
		operator_infix_cmp_gt   = 53, // >
		operator_infix_cmp_lte  = 54, // <=
		operator_infix_cmp_gte  = 55, // >=
		// Infix arithmetic operators.
		operator_infix_add      = 60, // +
		operator_infix_sub      = 61, // -
		operator_infix_mul      = 62, // *
		operator_infix_div      = 63, // /
		operator_infix_mod      = 64, // %
		operator_infix_sll      = 65, // <<<
		operator_infix_srl      = 66, // >>>
		operator_infix_sla      = 67, // <<
		operator_infix_sra      = 68, // >>
		operator_infix_andb     = 69, // &
		operator_infix_orb      = 70, // |
		operator_infix_xorb     = 71, // ^
		operator_infix_assign   = 72, // =
	};

	enum Index : std::uint8_t {
		index_literal            = 0, // +1
		index_named_reference    = 1, // +1
		index_bound_reference    = 2, // +1
		index_subexpression      = 3, // +1
		index_lambda_definition  = 4, // +1
		index_branch             = 5, // -1, +1
		index_function_call      = 6, // -X, +1
		index_operator_rpn       = 7, // -X, +1
	};
	struct S_literal {
		Value value;
	};
	struct S_named_reference {
		Cow_string id;
	};
	struct S_bound_reference {
		Sp<const Reference> ref_opt;
	};
	struct S_subexpression {
		Expression subexpr;
	};
	struct S_lambda_definition {
		Cow_string location;
		Vector<Cow_string> params;
		Block body_opt;
	};
	struct S_branch {
		Expression branch_true;
		Expression branch_false;
	};
	struct S_function_call {
		std::size_t argument_count;
	};
	struct S_operator_rpn {
		Operator op;
		bool assign; // This is ignored for `++`, `--`, `[]`, `=` and all rational operators.
	};
	using Variant = rocket::variant<ROCKET_CDR(
		, S_literal            // 0
		, S_named_reference    // 1
		, S_bound_reference    // 2
		, S_subexpression      // 3
		, S_lambda_definition  // 4
		, S_branch             // 5
		, S_function_call      // 6
		, S_operator_rpn       // 7
	)>;

private:
	Variant m_variant;

public:
	template<typename CandidateT, typename std::enable_if<std::is_constructible<Variant, CandidateT &&>::value>::type * = nullptr>
	Expression_node(CandidateT &&variant)
		: m_variant(std::forward<CandidateT>(variant))
	{ }
	Expression_node(Expression_node &&) noexcept;
	Expression_node & operator=(Expression_node &&) noexcept;
	~Expression_node();

public:
	Index which() const noexcept {
		return static_cast<Index>(m_variant.index());
	}
	template<typename ExpectT>
	const ExpectT * get_opt() const noexcept {
		return m_variant.get<ExpectT>();
	}
	template<typename ExpectT>
	const ExpectT & as() const {
		return m_variant.as<ExpectT>();
	}
};

extern const char * get_operator_name(Expression_node::Operator op) noexcept;

extern Expression bind_expression(const Expression &expr, Sp_cref<const Scope> scope);
extern void evaluate_expression(Vp<Reference> &result_out, const Expression &expr, Sp_cref<const Scope> scope);

}

#endif