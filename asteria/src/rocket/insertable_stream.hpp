// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ROCKET_INSERTABLE_IOSTREAM_HPP_
#define ROCKET_INSERTABLE_IOSTREAM_HPP_

#include "insertable_streambuf.hpp"
#include <istream> // std::basic_iostream<>

namespace rocket {

using ::std::basic_iostream;

template<typename charT, typename traitsT = char_traits<charT>, typename allocatorT = allocator<charT>>
  class basic_insertable_stream;

template<typename charT, typename traitsT, typename allocatorT>
  class basic_insertable_stream : public basic_iostream<charT, traitsT>
    {
    public:
      using char_type       = charT;
      using traits_type     = traitsT;
      using allocator_type  = allocatorT;

      using int_type   = typename traits_type::int_type;
      using pos_type   = typename traits_type::pos_type;
      using off_type   = typename traits_type::off_type;

      // N.B. These are non-standard extensions.
      using streambuf_type   = basic_insertable_streambuf<char_type, traits_type, allocator_type>;
      using string_type      = basic_cow_string<char_type, traits_type, allocator_type>;
      using size_type        = typename string_type::size_type;
      using difference_type  = typename string_type::difference_type;

    private:
      streambuf_type m_sb;

    public:
      explicit basic_insertable_stream(string_type str, size_type caret = string_type::npos, ios_base::openmode which = ios_base::in | ios_base::out)
        : basic_iostream<char_type, traits_type>(&(this->m_sb)),
          m_sb(::std::move(str), caret, which)
        {
        }
      basic_insertable_stream()
        : basic_insertable_stream(string_type())
        {
        }
      basic_insertable_stream(ios_base::openmode which)
        : basic_insertable_stream(string_type(), string_type::npos, which)
        {
        }
      ~basic_insertable_stream() override;

    public:
      streambuf_type * rdbuf() const noexcept
        {
          return const_cast<streambuf_type *>(&(this->m_sb));
        }

      const string_type & get_string() const noexcept
        {
          return this->rdbuf()->get_string();
        }
      size_type get_caret() const noexcept
        {
          return this->rdbuf()->get_caret();
        }
      void set_string(string_type str, size_type caret = string_type::npos) noexcept
        {
          return this->rdbuf()->set_string(::std::move(str), caret);
        }
      void set_caret(size_type caret) noexcept
        {
          return this->rdbuf()->set_caret(caret);
        }
      string_type extract_string() noexcept
        {
          return this->rdbuf()->extract_string();
        }
    };

template<typename charT, typename traitsT, typename allocatorT>
  basic_insertable_stream<charT, traitsT, allocatorT>::~basic_insertable_stream()
    = default;

extern template class basic_insertable_stream<char>;
extern template class basic_insertable_stream<wchar_t>;

using insertable_stream  = basic_insertable_stream<char>;
using insertable_wstream = basic_insertable_stream<wchar_t>;

}

#endif