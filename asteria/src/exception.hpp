// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_EXCEPTION_HPP_
#define ASTERIA_EXCEPTION_HPP_

#include "fwd.hpp"
#include "source_location.hpp"
#include "value.hpp"
#include <exception>

namespace Asteria {

class Exception : public virtual std::exception
  {
  private:
    Source_location m_loc;
    Value m_value;

    Vector<Source_location> m_backtrace;

  public:
    template<typename XvalueT, typename std::enable_if<std::is_constructible<Value, XvalueT &&>::value>::type * = nullptr>
      Exception(const Source_location &loc, XvalueT &&value)
      : m_loc(loc), m_value(std::forward<XvalueT>(value))
      {
      }
    explicit Exception(const std::exception &stdex)
      : m_loc(String::shallow("<native code>"), 0), m_value(D_string(stdex.what()))
      {
      }
    ~Exception();

  public:
    const Source_location & get_location() const noexcept
      {
        return this->m_loc;
      }
    const Value & get_value() const noexcept
      {
        return this->m_value;
      }

    const Vector<Source_location> & get_backtrace() const noexcept
      {
        return this->m_backtrace;
      }
    void append_backtrace(const Source_location &loc)
      {
        this->m_backtrace.emplace_back(loc);
      }

    const char * what() const noexcept override;
  };

}

#endif
