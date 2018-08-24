/// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ROCKET_INTRUSIVE_PTR_HPP_
#define ROCKET_INTRUSIVE_PTR_HPP_

#include <memory> // std::default_delete<>
#include <atomic> // std::atomic<>
#include <type_traits> // so many...
#include <utility> // std::move(), std::forward(), std::declval()
#include <iosfwd> // std::basic_ostream<>
#include <cstddef> // std::nullptr_t
#include <typeinfo>
#include "compatibility.h"
#include "assert.hpp"
#include "throw.hpp"
#include "allocator_utilities.hpp"

namespace rocket {

using ::std::default_delete;
using ::std::atomic;
using ::std::remove_reference;
using ::std::remove_pointer;
using ::std::enable_if;
using ::std::conditional;
using ::std::is_nothrow_constructible;
using ::std::is_convertible;
using ::std::is_array;
using ::std::is_reference;
using ::std::integral_constant;
using ::std::add_lvalue_reference;
using ::std::basic_ostream;
using ::std::nullptr_t;

template<typename elementT, typename deleterT = default_delete<elementT>>
  class intrusive_base;

template<typename elementT, typename deleterT = default_delete<elementT>>
  class intrusive_ptr;

namespace details_intrusive_ptr {

  class refcount_base
    {
    private:
      mutable atomic<long> m_nref;

    public:
      constexpr refcount_base() noexcept
        : m_nref(1)
        {
        }
      constexpr refcount_base(const refcount_base &) noexcept
        : refcount_base()
        {
        }
      refcount_base & operator=(const refcount_base &) noexcept
        {
          return *this;
        }
      virtual ~refcount_base();

    public:
      long reference_count() const noexcept
        {
          return this->m_nref.load(::std::memory_order_relaxed);
        }
      bool try_add_reference() const noexcept
        {
          auto nref_old = this->m_nref.load(::std::memory_order_relaxed);
          do {
            if(nref_old == 0) {
              return false;
            }
            if(this->m_nref.compare_exchange_strong(nref_old, nref_old + 1, ::std::memory_order_relaxed)) {
              return true;
            }
          } while(true);
        }
      void add_reference() const noexcept
        {
          auto nref_old = this->m_nref.fetch_add(1, ::std::memory_order_relaxed);
          ROCKET_ASSERT(nref_old >= 1);
        }
      bool drop_reference() const noexcept
        {
          auto nref_old = this->m_nref.fetch_sub(1, ::std::memory_order_acq_rel);
          if(nref_old > 1) {
            return false;
          }
          ROCKET_ASSERT(nref_old == 1);
          return true;
        }
    };

  template<typename elementT, typename deleterT>
    class stored_pointer;
}

template<typename elementT, typename deleterT>
  class intrusive_base : private details_intrusive_ptr::refcount_base
    {
      template<typename, typename>
        friend class details_intrusive_ptr::stored_pointer;

    private:
      template<typename yelementT, typename ydeleterT, typename cvthisT>
        static intrusive_ptr<yelementT, ydeleterT> do_share_this(cvthisT *cvthis);

    public:
      ~intrusive_base() override;

    public:
      bool unique() const noexcept
        {
          return this->refcount_base::reference_count() == 1;
        }
      long use_count() const noexcept
        {
          return this->refcount_base::reference_count();
        }
      template<typename yelementT = elementT, typename ydeleterT = deleterT>
        intrusive_ptr<const yelementT, ydeleterT> share_this() const
          {
            return this->do_share_this<const yelementT, ydeleterT>(this);
          }
      template<typename yelementT = elementT, typename ydeleterT = deleterT>
        intrusive_ptr<yelementT, ydeleterT> share_this()
          {
            return this->do_share_this<yelementT, ydeleterT>(this);
          }
    };

namespace details_intrusive_ptr {

  template<typename ...unusedT>
    struct make_void
      {
        using type = void;
      };

  template<typename elementT, typename deleterT, typename = void>
    struct pointer_selector
      {
        using type = elementT *;
      };
  template<typename elementT, typename deleterT>
    struct pointer_selector<elementT, deleterT, typename make_void<typename remove_reference<deleterT>::type::pointer>::type>
      {
        using type = typename remove_reference<deleterT>::type::pointer;
      };

  template<typename elementT, typename deleterT>
    class stored_pointer : private allocator_wrapper_base_for<deleterT>::type
      {
      public:
        using element_type  = elementT;
        using deleter_type  = deleterT;
        using pointer       = typename pointer_selector<element_type, deleter_type>::type;

      private:
        using deleter_base  = typename allocator_wrapper_base_for<deleter_type>::type;

      private:
        pointer m_ptr;

      public:
        constexpr stored_pointer() noexcept(is_nothrow_constructible<deleter_type>::value)
          : deleter_base(),
            m_ptr()
          {
          }
        explicit constexpr stored_pointer(const deleter_type &del) noexcept
          : deleter_base(del),
            m_ptr()
          {
          }
        explicit constexpr stored_pointer(deleter_type &&del) noexcept
          : deleter_base(::std::move(del)),
            m_ptr()
          {
          }
        ~stored_pointer()
          {
            this->reset(pointer());
          }

        stored_pointer(const stored_pointer &)
          = delete;
        stored_pointer & operator=(const stored_pointer &)
          = delete;

      public:
        const deleter_type & as_deleter() const noexcept
          {
            return static_cast<const deleter_base &>(*this);
          }
        deleter_type & as_deleter() noexcept
          {
            return static_cast<deleter_base &>(*this);
          }

        long use_count() const noexcept
          {
            const auto ptr = this->m_ptr;
            if(!ptr) {
              return 0;
            }
            return ptr->refcount_base::use_count();
          }
        pointer get() const noexcept
          {
            return this->m_ptr;
          }
        pointer copy_release() const noexcept
          {
            const auto ptr = this->m_ptr;
            if(ptr) {
              ptr->refcount_base::add_reference();
            }
            return ptr;
          }
        pointer release() noexcept
          {
            const auto ptr = this->m_ptr;
            if(ptr) {
              this->m_ptr = pointer();
            }
            return ptr;
          }
        void reset(pointer ptr_new) noexcept
          {
            const auto ptr = noadl::exchange(this->m_ptr, ptr_new);
            if(!ptr) {
              return;
            }
            if(ptr->refcount_base::drop_reference() == false) {
              return;
            }
            this->as_deleter()(ptr);
          }
        void exchange(stored_pointer &other) noexcept
          {
            ::std::swap(this->m_ptr, other.m_ptr);
          }
      };

}

template<typename elementT, typename deleterT>
  class intrusive_ptr
    {
      static_assert(is_array<elementT>::value == false, "`elementT` must not be an array type.");
      static_assert(is_reference<deleterT>::value == false, "`intrusive_ptr` does not accept reference types as deleters.");

      template<typename, typename>
        friend class intrusive_ptr;

    public:
      using element_type  = elementT;
      using deleter_type  = deleterT;
      using pointer       = typename details_intrusive_ptr::pointer_selector<element_type, deleter_type>::type;

    private:
      details_intrusive_ptr::stored_pointer<element_type, deleter_type> m_sth;

    public:
      constexpr intrusive_ptr(nullptr_t = nullptr) noexcept(is_nothrow_constructible<deleter_type>::value)
        : m_sth()
        {
        }
      explicit constexpr intrusive_ptr(const deleter_type &del) noexcept
        : m_sth(del)
        {
        }
      explicit intrusive_ptr(pointer ptr) noexcept(is_nothrow_constructible<deleter_type>::value)
        : intrusive_ptr()
        {
          this->reset(ptr);
        }
      intrusive_ptr(const intrusive_ptr &other) noexcept
        : intrusive_ptr(other.m_sth.as_deleter())
        {
          this->reset(other.m_sth.copy_release());
        }
      intrusive_ptr(const intrusive_ptr &other, const deleter_type &del) noexcept
        : intrusive_ptr(del)
        {
          this->reset(other.m_sth.copy_release());
        }
      intrusive_ptr(intrusive_ptr &&other) noexcept
        : intrusive_ptr(::std::move(other.m_sth.as_deleter()))
        {
          this->reset(other.m_sth.release());
        }
      intrusive_ptr(intrusive_ptr &&other, const deleter_type &del) noexcept
        : intrusive_ptr(del)
        {
          this->reset(other.m_sth.release());
        }
      template<typename yelementT, typename ydeleterT,
               typename enable_if<is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::pointer, pointer>::value &&
                                  is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::deleter_type, deleter_type>::value
                                 >::type * = nullptr>
        intrusive_ptr(const intrusive_ptr<yelementT, ydeleterT> &other) noexcept
          : intrusive_ptr(other.m_sth.as_deleter())
          {
            this->reset(other.m_sth.copy_release());
          }
      template<typename yelementT, typename ydeleterT,
               typename enable_if<is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::pointer, pointer>::value &&
                                  is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::deleter_type, deleter_type>::value
                                 >::type * = nullptr>
        intrusive_ptr(const intrusive_ptr<yelementT, ydeleterT> &other, const deleter_type &del) noexcept
          : intrusive_ptr(del)
          {
            this->reset(other.m_sth.copy_release());
          }
      template<typename yelementT, typename ydeleterT,
               typename enable_if<is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::pointer, pointer>::value &&
                                  is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::deleter_type, deleter_type>::value
                                 >::type * = nullptr>
        intrusive_ptr(intrusive_ptr<yelementT, ydeleterT> &&other) noexcept
          : intrusive_ptr(::std::move(other.m_sth.as_deleter()))
          {
            this->reset(other.m_sth.release());
          }
      template<typename yelementT, typename ydeleterT,
               typename enable_if<is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::pointer, pointer>::value &&
                                  is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::deleter_type, deleter_type>::value
                                 >::type * = nullptr>
        intrusive_ptr(intrusive_ptr<yelementT, ydeleterT> &&other, const deleter_type &del) noexcept
          : intrusive_ptr(del)
          {
            this->reset(other.m_sth.release());
          }
      intrusive_ptr & operator=(nullptr_t) noexcept
        {
          this->reset();
          return *this;
        }
      intrusive_ptr & operator=(const intrusive_ptr &other) noexcept
        {
          this->reset(other.m_sth.copy_release());
          allocator_copy_assigner<deleter_type, true>()(this->m_sth.as_deleter(), other.m_sth.as_deleter());
          return *this;
        }
      intrusive_ptr & operator=(intrusive_ptr &&other) noexcept
        {
          this->reset(other.m_sth.release());
          allocator_move_assigner<deleter_type, true>()(this->m_sth.as_deleter(), ::std::move(other.m_sth.as_deleter()));
          return *this;
        }
      template<typename yelementT, typename ydeleterT,
               typename enable_if<is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::pointer, pointer>::value &&
                                  is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::deleter_type, deleter_type>::value
                                 >::type * = nullptr>
        intrusive_ptr & operator=(const intrusive_ptr<yelementT, ydeleterT> &other) noexcept
          {
            this->reset(other.m_sth.copy_release());
            allocator_copy_assigner<deleter_type, true>()(this->m_sth.as_deleter(), other.m_sth.as_deleter());
            return *this;
          }
      template<typename yelementT, typename ydeleterT,
               typename enable_if<is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::pointer, pointer>::value &&
                                  is_convertible<typename intrusive_ptr<yelementT, ydeleterT>::deleter_type, deleter_type>::value
                                 >::type * = nullptr>
        intrusive_ptr & operator=(intrusive_ptr<yelementT, ydeleterT> &&other) noexcept
          {
            this->reset(other.m_sth.release());
            allocator_move_assigner<deleter_type, true>()(this->m_sth.as_deleter(), ::std::move(other.m_sth.as_deleter()));
            return *this;
          }

    public:
      // 23.11.1.2.4, observers
      pointer get() const noexcept
        {
          return this->m_sth.get();
        }
      typename add_lvalue_reference<element_type>::type operator*() const
        {
          const auto ptr = this->get();
          ROCKET_ASSERT(ptr);
          return *ptr;
        }
      pointer operator->() const noexcept
        {
          const auto ptr = this->get();
          ROCKET_ASSERT(ptr);
          return ptr;
        }
      explicit operator bool () const noexcept
        {
          const auto ptr = this->get();
          return !!ptr;
        }

      // 23.11.1.2.5, modifiers
      pointer release() noexcept
        {
          const auto ptr = this->m_sth.release();
          return ptr;
        }
      void reset(pointer ptr = pointer()) noexcept
        {
          this->m_sth.reset(ptr);
        }

      void swap(intrusive_ptr &other) noexcept
        {
          this->m_sth.exchange(other.m_sth);
          allocator_swapper<deleter_type, true>()(this->m_sth.as_deleter(), other.m_sth.as_deleter());
        }
    };

template<typename xelementT, typename yelementT, typename ...paramsT>
  inline bool operator==(const intrusive_ptr<xelementT, paramsT...> &lhs, const intrusive_ptr<yelementT, paramsT...> &rhs) noexcept
    {
      return lhs.get() == rhs.get();
    }
template<typename xelementT, typename yelementT, typename ...paramsT>
  inline bool operator!=(const intrusive_ptr<xelementT, paramsT...> &lhs, const intrusive_ptr<yelementT, paramsT...> &rhs) noexcept
    {
      return lhs.get() != rhs.get();
    }
template<typename xelementT, typename yelementT, typename ...paramsT>
  inline bool operator<(const intrusive_ptr<xelementT, paramsT...> &lhs, const intrusive_ptr<yelementT, paramsT...> &rhs)
    {
      return lhs.get() < rhs.get();
    }
template<typename xelementT, typename yelementT, typename ...paramsT>
  inline bool operator>(const intrusive_ptr<xelementT, paramsT...> &lhs, const intrusive_ptr<yelementT, paramsT...> &rhs)
    {
      return lhs.get() > rhs.get();
    }
template<typename xelementT, typename yelementT, typename ...paramsT>
  inline bool operator<=(const intrusive_ptr<xelementT, paramsT...> &lhs, const intrusive_ptr<yelementT, paramsT...> &rhs)
    {
      return lhs.get() <= rhs.get();
    }
template<typename xelementT, typename yelementT, typename ...paramsT>
  inline bool operator>=(const intrusive_ptr<xelementT, paramsT...> &lhs, const intrusive_ptr<yelementT, paramsT...> &rhs)
    {
      return lhs.get() >= rhs.get();
    }

template<typename ...paramsT>
  inline bool operator==(const intrusive_ptr<paramsT...> &lhs, nullptr_t) noexcept
    {
      return !(lhs.get());
    }
template<typename ...paramsT>
  inline bool operator!=(const intrusive_ptr<paramsT...> &lhs, nullptr_t) noexcept
    {
      return !!(lhs.get());
    }

template<typename ...paramsT>
  inline bool operator==(nullptr_t, const intrusive_ptr<paramsT...> &rhs) noexcept
    {
      return !(rhs.get());
    }
template<typename ...paramsT>
  inline bool operator!=(nullptr_t, const intrusive_ptr<paramsT...> &rhs) noexcept
    {
      return !!(rhs.get());
    }

template<typename ...paramsT>
  inline void swap(intrusive_ptr<paramsT...> &lhs, intrusive_ptr<paramsT...> &rhs) noexcept
    {
      lhs.swap(rhs);
    }

namespace details_intrusive_ptr {

  template<typename resultT, typename sourceT, typename = void>
    struct static_cast_or_dynamic_cast_helper
      {
        constexpr resultT operator()(sourceT &&src) const
          {
            return dynamic_cast<resultT>(::std::forward<sourceT>(src));
          }
      };
  template<typename resultT, typename sourceT>
    struct static_cast_or_dynamic_cast_helper<resultT, sourceT, typename make_void<decltype(static_cast<resultT>(::std::declval<sourceT>()))>::type>
      {
        constexpr resultT operator()(sourceT &&src) const
          {
            return static_cast<resultT>(::std::forward<sourceT>(src));
          }
      };

}

template<typename elementT, typename deleterT>
  template<typename yelementT, typename ydeleterT, typename cvthisT>
    inline intrusive_ptr<yelementT, ydeleterT> intrusive_base<elementT, deleterT>::do_share_this(cvthisT *cvthis)
      {
        const auto ptr = details_intrusive_ptr::static_cast_or_dynamic_cast_helper<yelementT *, intrusive_base *>()(+cvthis);
        if(!ptr) {
          noadl::throw_domain_error("intrusive_base: The current object cannot be converted to type `%s`, whose most derived type is `%s`.",
                                    typeid(yelementT).name(), typeid(*cvthis).name());
        }
        cvthis->refcount_base::add_reference();
        return intrusive_ptr<yelementT, ydeleterT>(ptr);
      }

template<typename elementT, typename deleterT>
  intrusive_base<elementT, deleterT>::~intrusive_base()
    = default;

template<typename elementT, typename ...paramsT>
  inline intrusive_ptr<elementT> make_intrusive(paramsT &&...params)
    {
      return intrusive_ptr<elementT>(new elementT(::std::forward<paramsT>(params)...));
    }

template<typename charT, typename traitsT, typename ...paramsT>
  inline basic_ostream<charT, traitsT> & operator<<(basic_ostream<charT, traitsT> &os, const intrusive_ptr<paramsT...> &iptr)
    {
      return os << iptr.get();
    }

}

#endif