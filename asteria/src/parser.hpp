// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_PARSER_HPP_
#define ASTERIA_PARSER_HPP_

#include "fwd.hpp"
#include "parser_error.hpp"
#include "block.hpp"
#include "rocket/variant.hpp"

namespace Asteria {

class Parser
  {
  public:
    enum State : Uint8
      {
        state_empty    = 0,
        state_error    = 1,
        state_success  = 2,
      };

  private:
    rocket::variant<Nullptr, Parser_error, Vector<Statement>> m_stor;

  public:
    Parser() noexcept
      : m_stor()
      {
      }
    ~Parser();

    Parser(const Parser &)
      = delete;
    Parser & operator=(const Parser &)
      = delete;

  public:
    State state() const noexcept
      {
        return State(this->m_stor.index());
      }
    explicit operator bool () const noexcept
      {
        return this->state() == state_success;
      }

    Parser_error get_parser_error() const noexcept;
    bool empty() const noexcept;

    bool load(Token_stream &tstrm_io);
    void clear() noexcept;
    Block extract_document();
  };

}

#endif
