// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_BLOCK_HPP_
#define ASTERIA_BLOCK_HPP_

#include "fwd.hpp"
#include "statement.hpp"

namespace Asteria {

class Block {
	friend Statement;

public:
	enum Execution_result : unsigned {
		execution_result_end_of_block          = 0,
		execution_result_break_unspecified     = 1,
		execution_result_break_switch          = 2,
		execution_result_break_while           = 3,
		execution_result_break_for             = 4,
		execution_result_continue_unspecified  = 5,
		execution_result_continue_while        = 6,
		execution_result_continue_for          = 7,
		execution_result_return                = 8,
	};

private:
	std::vector<Statement> m_statements;

public:
	Block(std::vector<Statement> statements)
		: m_statements(std::move(statements))
	{ }
	Block(Block &&) noexcept;
	Block & operator=(Block &&) noexcept;
	~Block();

public:
	bool empty() const noexcept {
		return m_statements.empty();
	}
	std::size_t size() const noexcept {
		return m_statements.size();
	}
	decltype(m_statements)::const_iterator begin() const noexcept {
		return m_statements.begin();
	}
	decltype(m_statements)::const_iterator end() const noexcept {
		return m_statements.end();
	}
	decltype(m_statements)::const_reference at(std::size_t n) const {
		return m_statements.at(n);
	}
};

extern void bind_block_in_place(Xptr<Block> &bound_result_out, Spcref<Scope> scope, Spcref<const Block> block_opt);
extern Block::Execution_result execute_block_in_place(Xptr<Reference> &reference_out, Spcref<Scope> scope, Spcref<Recycler> recycler, Spcref<const Block> block_opt);

extern void bind_block(Xptr<Block> &bound_result_out, Spcref<const Block> block_opt, Spcref<const Scope> scope);
extern Block::Execution_result execute_block(Xptr<Reference> &reference_out, Spcref<Recycler> recycler, Spcref<const Block> block_opt, Spcref<const Scope> scope);

}

#endif