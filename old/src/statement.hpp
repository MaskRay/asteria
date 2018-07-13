// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_STATEMENT_HPP_
#define ASTERIA_STATEMENT_HPP_

#include "fwd.hpp"
#include "rocket/variant.hpp"
#include "initializer.hpp"

namespace Asteria {

class Statement {
public:
	enum Target_scope : std::uint8_t {
		target_scope_unspecified  = 0,
		target_scope_switch       = 1,
		target_scope_while        = 2,
		target_scope_for          = 3,
	};

	enum Execution_result : std::uint8_t {
		execution_result_next                  = 0,
		execution_result_break_unspecified     = 1,
		execution_result_break_switch          = 2,
		execution_result_break_while           = 3,
		execution_result_break_for             = 4,
		execution_result_continue_unspecified  = 5,
		execution_result_continue_while        = 6,
		execution_result_continue_for          = 7,
		execution_result_return                = 8,
	};

	struct Switch_clause {
		// `pred_opt` is non-null for `case` clauses and is null for `default` clauses.
		Expression pred;
		Block body;
	};

	enum Index : std::uint8_t {
		index_expression_statement  =  0,
		index_variable_definition   =  1,
		index_function_definition   =  2,
		index_if_statement          =  3,
		index_switch_statement      =  4,
		index_do_while_statement    =  5,
		index_while_statement       =  6,
		index_for_statement         =  7,
		index_for_each_statement    =  8,
		index_try_statement         =  9,
		index_defer_statement       = 10,
		index_break_statement       = 11,
		index_continue_statement    = 12,
		index_throw_statement       = 13,
		index_return_statement      = 14,
	};
	struct S_expression_statement {
		Expression expr;
	};
	struct S_variable_definition {
		Cow_string id;
		bool immutable;
		Initializer init;
	};
	struct S_function_definition {
		Cow_string id;
		Cow_string location;
		Vector<Cow_string> params;
		Block body;
	};
	struct S_if_statement {
		Expression cond;
		Block branch_true;
		Block branch_false;
	};
	struct S_switch_statement {
		Expression ctrl;
		Vector<Switch_clause> clauses;
	};
	struct S_do_while_statement {
		Block body;
		Expression cond;
	};
	struct S_while_statement {
		Expression cond;
		Block body;
	};
	struct S_for_statement {
		Block init;
		Expression cond;
		Expression step;
		Block body;
	};
	struct S_for_each_statement {
		Cow_string key_id;
		Cow_string value_id;
		Initializer range_init;
		Block body;
	};
	struct S_try_statement {
		Block branch_try;
		Cow_string except_id;
		Block branch_catch;
	};
	struct S_defer_statement {
		Cow_string location;
		Block body;
	};
	struct S_break_statement {
		Target_scope target_scope;
	};
	struct S_continue_statement {
		Target_scope target_scope;
	};
	struct S_throw_statement {
		Expression operand;
	};
	struct S_return_statement {
		Expression operand;
	};
	using Variant = rocket::variant<ROCKET_CDR(
		, S_expression_statement  //  0
		, S_variable_definition   //  1
		, S_function_definition   //  2
		, S_if_statement          //  3
		, S_switch_statement      //  4
		, S_do_while_statement    //  5
		, S_while_statement       //  6
		, S_for_statement         //  7
		, S_for_each_statement    //  8
		, S_try_statement         //  9
		, S_defer_statement       // 10
		, S_break_statement       // 11
		, S_continue_statement    // 12
		, S_throw_statement       // 13
		, S_return_statement      // 14
	)>;

private:
	Variant m_variant;

public:
	template<typename CandidateT, typename std::enable_if<std::is_constructible<Variant, CandidateT &&>::value>::type * = nullptr>
	Statement(CandidateT &&cand)
		: m_variant(std::forward<CandidateT>(cand))
	{ }
	Statement(Statement &&) noexcept;
	Statement & operator=(Statement &&) noexcept;
	~Statement();

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

extern Block bind_block_in_place(Sp_cref<Scope> scope_inout, const Block &block);
extern Statement::Execution_result execute_block_in_place(Vp<Reference> &ref_out, Sp_cref<Scope> scope_inout, const Block &block);

extern Block bind_block(const Block &block, Sp_cref<const Scope> scope);
extern Statement::Execution_result execute_block(Vp<Reference> &ref_out, const Block &block, Sp_cref<const Scope> scope);

}

#endif