// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ROCKET_COW_VECTOR_HPP_
#define ROCKET_COW_VECTOR_HPP_

#include <memory> // std::allocator<>, std::allocator_traits<>
#include <atomic> // std::atomic<>
#include <type_traits> // so many...
#include <iterator> // std::iterator_traits<>, std::reverse_iterator<>, std::random_access_iterator_tag
#include <initializer_list> // std::initializer_list<>
#include <utility> // std::move(), std::forward(), std::declval()
#include <typeinfo> // typeid()
#include <cstddef> // std::size_t, std::ptrdiff_t
#include <cstdint> // std::uintptr_t
#include <cstring> // std::memset()
#include "compatibility.h"
#include "assert.hpp"
#include "throw.hpp"
#include "utilities.hpp"
#include "allocator_utilities.hpp"

/* Differences from `std::vector`:
 * 1. All functions guarantee only basic exception safety rather than strong exception safety, hence are more efficient.
 * 2. `begin()` and `end()` always return `const_iterator`s. `at()`, `front()` and `back()` always return `const_reference`s.
 * 3. The copy constructor and copy assignment operator will not throw exceptions.
 * 4. The specialization for `bool` is not provided.
 * 5. `emplace()` is not provided.
 * 6. Comparison operators are not provided.
 * 7. The value type may be incomplete. It need be neither copy-assignable nor move-assignable, but must be swappable.
 */

namespace rocket {

using ::std::allocator;
using ::std::allocator_traits;
using ::std::atomic;
using ::std::is_same;
using ::std::decay;
using ::std::is_array;
using ::std::is_trivial;
using ::std::enable_if;
using ::std::is_convertible;
using ::std::is_copy_constructible;
using ::std::is_nothrow_constructible;
using ::std::is_nothrow_copy_constructible;
using ::std::is_nothrow_move_constructible;
using ::std::conditional;
using ::std::integral_constant;
using ::std::iterator_traits;
using ::std::initializer_list;
using ::std::size_t;
using ::std::ptrdiff_t;
using ::std::uintptr_t;

template<typename valueT, typename allocatorT = allocator<valueT>>
  class cow_vector;

  namespace details_cow_vector {

  template<typename allocatorT>
    struct basic_storage
    {
      using allocator_type   = allocatorT;
      using value_type       = typename allocator_type::value_type;
      using size_type        = typename allocator_traits<allocator_type>::size_type;

      static constexpr size_type min_nblk_for_nelem(size_type nelem) noexcept
        {
          return (nelem * sizeof(value_type) + sizeof(basic_storage) - 1) / sizeof(basic_storage) + 1;
        }
      static constexpr size_type max_nelem_for_nblk(size_type nblk) noexcept
        {
          return (nblk - 1) * sizeof(basic_storage) / sizeof(value_type);
        }

      atomic<long> nref;
      allocator_type alloc;
      size_type nblk;
      size_type nelem;
      union { value_type data[0]; };

      basic_storage(const allocator_type &xalloc, size_type xnblk) noexcept
        : alloc(xalloc), nblk(xnblk)
        {
          this->nelem = 0;
          this->nref.store(1, ::std::memory_order_release);
        }
      ~basic_storage()
        {
          auto nrem = this->nelem;
          while(nrem != 0) {
            --nrem;
            allocator_traits<allocator_type>::destroy(this->alloc, this->data + nrem);
          }
#ifdef ROCKET_DEBUG
          this->nelem = 0xCCAA;
#endif
        }

      basic_storage(const basic_storage &)
        = delete;
      basic_storage & operator=(const basic_storage &)
        = delete;
    };

  template<typename allocatorT>
    struct copy_trivially : integral_constant<bool, is_trivial<typename allocatorT::value_type>::value && is_std_allocator<allocatorT>::value>
    {
    };

  template<typename allocatorT, bool copyableT = is_copy_constructible<typename allocatorT::value_type>::value, bool memcpyT = copy_trivially<allocatorT>::value>
    struct copy_storage_helper
    {
      // This is the generic version.
      template<typename xpointerT, typename ypointerT>
        void operator()(xpointerT ptr, ypointerT ptr_old, size_t off, size_t cnt) const
        {
          static_assert(is_same<typename decay<decltype(*ptr)>::type, basic_storage<allocatorT>>::value, "???");
          static_assert(is_same<typename decay<decltype(*ptr_old)>::type, basic_storage<allocatorT>>::value, "???");
          // Copy elements one by one.
          auto nelem = ptr->nelem;
          const auto cap = basic_storage<allocatorT>::max_nelem_for_nblk(ptr->nblk);
          ROCKET_ASSERT(cnt <= cap - nelem);
          for(auto i = off; i != off + cnt; ++i) {
            allocator_traits<allocatorT>::construct(ptr->alloc, ptr->data + nelem, ptr_old->data[i]);
            ptr->nelem = (nelem += 1);
          }
        }
    };
  template<typename allocatorT, bool memcpyT>
    struct copy_storage_helper<allocatorT, false, memcpyT>
    {
      // This specialization is used when `allocatorT::value_type` is not copy-constructible.
      template<typename xpointerT, typename ypointerT>
        [[noreturn]] void operator()(xpointerT /*ptr*/, ypointerT /*ptr_old*/, size_t /*off*/, size_t /*cnt*/) const
        {
          // Throw an exception unconditionally, even when there is nothing to copy.
          noadl::throw_domain_error("cow_vector: `%s` is not copy-constructible.", typeid(typename allocatorT::value_type).name());
        }
    };
  template<typename allocatorT>
    struct copy_storage_helper<allocatorT, true, true>
    {
      // This specialization is used when `std::allocator` is to be used to copy a trivial type.
      template<typename xpointerT, typename ypointerT>
        void operator()(xpointerT ptr, ypointerT ptr_old, size_t off, size_t cnt) const
        {
          static_assert(is_same<typename decay<decltype(*ptr)>::type, basic_storage<allocatorT>>::value, "???");
          static_assert(is_same<typename decay<decltype(*ptr_old)>::type, basic_storage<allocatorT>>::value, "???");
          // Optimize it using `std::memcpy()`, as the source and destination locations can't overlap.
          auto nelem = ptr->nelem;
          const auto cap = basic_storage<allocatorT>::max_nelem_for_nblk(ptr->nblk);
          ROCKET_ASSERT(cnt <= cap - nelem);
          ::std::memcpy(ptr->data + nelem, ptr_old->data + off, sizeof(ptr->data[0]) * cnt);
          ptr->nelem = (nelem += cnt);
        }
    };

  template<typename allocatorT, bool memcpyT = copy_trivially<allocatorT>::value>
    struct move_storage_helper
    {
      // This is the generic version.
      template<typename xpointerT, typename ypointerT>
        void operator()(xpointerT ptr, ypointerT ptr_old, size_t off, size_t cnt) const
        {
          static_assert(is_same<typename decay<decltype(*ptr)>::type, basic_storage<allocatorT>>::value, "???");
          static_assert(is_same<typename decay<decltype(*ptr_old)>::type, basic_storage<allocatorT>>::value, "???");
          // Move elements one by one.
          auto nelem = ptr->nelem;
          const auto cap = basic_storage<allocatorT>::max_nelem_for_nblk(ptr->nblk);
          ROCKET_ASSERT(cnt <= cap - nelem);
          for(auto i = off; i != off + cnt; ++i) {
            allocator_traits<allocatorT>::construct(ptr->alloc, ptr->data + nelem, ::std::move(ptr_old->data[i]));
            ptr->nelem = (nelem += 1);
          }
        }
    };
  template<typename allocatorT>
    struct move_storage_helper<allocatorT, true>
    {
      // This specialization is used when `std::allocator` is to be used to move a trivial type.
      template<typename xpointerT, typename ypointerT>
        void operator()(xpointerT ptr, ypointerT ptr_old, size_t off, size_t cnt) const
        {
          static_assert(is_same<typename decay<decltype(*ptr)>::type, basic_storage<allocatorT>>::value, "???");
          static_assert(is_same<typename decay<decltype(*ptr_old)>::type, basic_storage<allocatorT>>::value, "???");
          // Optimize it using `std::memcpy()`, as the source and destination locations can't overlap.
          auto nelem = ptr->nelem;
          const auto cap = basic_storage<allocatorT>::max_nelem_for_nblk(ptr->nblk);
          ROCKET_ASSERT(cnt <= cap - nelem);
          ::std::memcpy(ptr->data + nelem, ptr_old->data + off, sizeof(ptr->data[0]) * cnt);
#ifdef ROCKET_DEBUG
          ::std::memset(ptr_old->data + off, '/', sizeof(ptr->data[0]) * cnt);
#endif
          ptr->nelem = (nelem += cnt);
        }
    };

  template<typename allocatorT>
    class storage_handle : private allocator_wrapper_base_for<allocatorT>::type
    {
    public:
      using allocator_type   = allocatorT;
      using value_type       = typename allocator_type::value_type;
      using size_type        = typename allocator_traits<allocator_type>::size_type;

    private:
      using allocator_base    = typename allocator_wrapper_base_for<allocator_type>::type;
      using storage           = basic_storage<allocator_type>;
      using storage_allocator = typename allocator_traits<allocator_type>::template rebind_alloc<storage>;
      using storage_pointer   = typename allocator_traits<storage_allocator>::pointer;

    private:
      storage_pointer m_ptr;
      void (*m_smf)(storage_pointer, bool);

    public:
      explicit constexpr storage_handle(const allocator_type &alloc) noexcept
        : allocator_base(alloc),
          m_ptr(), m_smf()
        {
        }
      explicit constexpr storage_handle(allocator_type &&alloc) noexcept
        : allocator_base(::std::move(alloc)),
          m_ptr(), m_smf()
        {
        }
      ~storage_handle()
        {
          this->deallocate();
        }

      storage_handle(const storage_handle &)
        = delete;
      storage_handle & operator=(const storage_handle &)
        = delete;

    private:
      void do_reset(storage_pointer ptr_new, void (*smf_new)(storage_pointer, bool)) noexcept
        {
          const auto ptr = noadl::exchange(this->m_ptr, ptr_new);
          const auto smf = noadl::exchange(this->m_smf, smf_new);
          if(!ptr) {
            return;
          }
          (*smf)(ptr, false);
        }

      static void do_manipulate_storage(storage_pointer ptr, bool to_add_ref)
        {
          if(to_add_ref) {
            // Increment the reference count.
            const auto nref_old = ptr->nref.fetch_add(1, ::std::memory_order_relaxed);
            ROCKET_ASSERT(nref_old >= 1);
          } else {
            // Decrement the reference count with acquire-release semantics to prevent races on `ptr->alloc`.
            const auto nref_old = ptr->nref.fetch_sub(1, ::std::memory_order_acq_rel);
            if(nref_old > 1) {
              return;
            }
            ROCKET_ASSERT(nref_old == 1);
            // If it has been decremented to zero, deallocate the block.
            auto st_alloc = storage_allocator(ptr->alloc);
            const auto nblk = ptr->nblk;
            noadl::destroy_at(noadl::unfancy(ptr));
#ifdef ROCKET_DEBUG
            ::std::memset(static_cast<void *>(noadl::unfancy(ptr)), '~', sizeof(storage) * nblk);
#endif
            allocator_traits<storage_allocator>::deallocate(st_alloc, ptr, nblk);
          }
        }

    public:
      const allocator_type & as_allocator() const noexcept
        {
          return static_cast<const allocator_base &>(*this);
        }
      allocator_type & as_allocator() noexcept
        {
          return static_cast<allocator_base &>(*this);
        }

      bool unique() const noexcept
        {
          const auto ptr = this->m_ptr;
          if(!ptr) {
            return false;
          }
          return ptr->nref.load(::std::memory_order_relaxed) == 1;
        }
      size_type capacity() const noexcept
        {
          const auto ptr = this->m_ptr;
          if(!ptr) {
            return 0;
          }
          return storage::max_nelem_for_nblk(ptr->nblk);
        }
      size_type max_size() const noexcept
        {
          auto st_alloc = storage_allocator(this->as_allocator());
          const auto max_nblk = allocator_traits<storage_allocator>::max_size(st_alloc);
          return storage::max_nelem_for_nblk(max_nblk / 2);
        }
      size_type check_size_add(size_type base, size_type add) const
        {
          const auto cap_max = this->max_size();
          ROCKET_ASSERT(base <= cap_max);
          if(cap_max - base < add) {
            noadl::throw_length_error("cow_vector: Increasing `%lld` by `%lld` would exceed the max size `%lld`.",
                                      static_cast<long long>(base), static_cast<long long>(add), static_cast<long long>(cap_max));
          }
          return base + add;
        }
      size_type round_up_capacity(size_type res_arg) const
        {
          const auto cap = this->check_size_add(0, res_arg);
          const auto nblk = storage::min_nblk_for_nelem(cap);
          return storage::max_nelem_for_nblk(nblk);
        }
      const value_type * data() const noexcept
        {
          const auto ptr = this->m_ptr;
          if(!ptr) {
            return nullptr;
          }
          return ptr->data;
        }
      size_type size() const noexcept
        {
          const auto ptr = this->m_ptr;
          if(!ptr) {
            return 0;
          }
          return ptr->nelem;
        }
      value_type * reallocate(size_type cnt_one, size_type off_two, size_type cnt_two, size_type res_arg)
        {
          if(res_arg == 0) {
            // Deallocate the block.
            this->deallocate();
            return nullptr;
          }
          const auto cap = this->check_size_add(0, res_arg);
          // Allocate an array of `storage` large enough for a header + `cap` instances of `value_type`.
          const auto nblk = storage::min_nblk_for_nelem(cap);
          auto st_alloc = storage_allocator(this->as_allocator());
          const auto ptr = allocator_traits<storage_allocator>::allocate(st_alloc, nblk);
#ifdef ROCKET_DEBUG
          ::std::memset(static_cast<void *>(noadl::unfancy(ptr)), '*', sizeof(storage) * nblk);
#endif
          noadl::construct_at(noadl::unfancy(ptr), this->as_allocator(), nblk);
          const auto ptr_old = this->m_ptr;
          if(ptr_old) {
            try {
              // Copy or move elements into the new block.
              // Moving is only viable if the old and new allocators compare equal and the old block is owned exclusively.
              if((ptr_old->alloc != ptr->alloc) || (ptr_old->nref.load(::std::memory_order_relaxed) != 1)) {
                copy_storage_helper<allocator_type>()(ptr, ptr_old,       0, cnt_one);
                copy_storage_helper<allocator_type>()(ptr, ptr_old, off_two, cnt_two);
              } else {
                move_storage_helper<allocator_type>()(ptr, ptr_old,       0, cnt_one);
                move_storage_helper<allocator_type>()(ptr, ptr_old, off_two, cnt_two);
              }
            } catch(...) {
              // If an exception is thrown, deallocate the new block, then rethrow the exception.
              noadl::destroy_at(noadl::unfancy(ptr));
              allocator_traits<storage_allocator>::deallocate(st_alloc, ptr, nblk);
              throw;
            }
          }
          // Replace the current block.
          this->do_reset(ptr, &do_manipulate_storage);
          return ptr->data;
        }
      void deallocate() noexcept
        {
          this->do_reset(storage_pointer(), nullptr);
        }

      void share_with(const storage_handle &other) noexcept
        {
          const auto ptr = other.m_ptr;
          if(ptr) {
            // Increment the reference count.
            (*(other.m_smf))(ptr, true);
          }
          this->do_reset(ptr, other.m_smf);
        }
      void share_with(storage_handle &&other) noexcept
        {
          const auto ptr = other.m_ptr;
          if(ptr) {
            // Detach the block.
            other.m_ptr = storage_pointer();
          }
          this->do_reset(ptr, other.m_smf);
        }
      void exchange_with(storage_handle &other) noexcept
        {
          ::std::swap(this->m_ptr, other.m_ptr);
          ::std::swap(this->m_smf, other.m_smf);
        }

      constexpr operator const storage_handle * () const noexcept
        {
          return this;
        }
      operator storage_handle * () noexcept
        {
          return this;
        }

      value_type * mut_data_unchecked() noexcept
        {
          auto ptr = this->m_ptr;
          if(!ptr) {
            return nullptr;
          }
          ROCKET_ASSERT(this->unique());
          return ptr->data;
        }
      template<typename ...paramsT>
        value_type * emplace_back_unchecked(paramsT &&...params)
        {
          ROCKET_ASSERT(this->unique());
          ROCKET_ASSERT(this->size() < this->capacity());
          const auto ptr = this->m_ptr;
          ROCKET_ASSERT(ptr);
          auto nelem = ptr->nelem;
          allocator_traits<allocator_type>::construct(ptr->alloc, ptr->data + nelem, ::std::forward<paramsT>(params)...);
          ptr->nelem = (nelem += 1);
          return ptr->data + nelem - 1;
        }
      void pop_back_n_unchecked(size_type n) noexcept
        {
          ROCKET_ASSERT(this->unique());
          ROCKET_ASSERT(n <= this->size());
          if(n == 0) {
            return;
          }
          const auto ptr = this->m_ptr;
          ROCKET_ASSERT(ptr);
          auto nelem = ptr->nelem;
          for(auto i = n; i != 0; --i) {
            ptr->nelem = (nelem -= 1);
            allocator_traits<allocator_type>::destroy(ptr->alloc, ptr->data + nelem);
          }
        }
    };

  template<typename vectorT, typename valueT>
    class vector_iterator
    {
      template<typename, typename>
        friend class vector_iterator;
      friend vectorT;

    public:
      using iterator_category  = ::std::random_access_iterator_tag;
      using value_type         = valueT;
      using pointer            = value_type *;
      using reference          = value_type &;
      using difference_type    = ptrdiff_t;

      using parent_type   = storage_handle<typename vectorT::allocator_type>;

    private:
      const parent_type *m_ref;
      value_type *m_ptr;

    private:
      constexpr vector_iterator(const parent_type *ref, value_type *ptr) noexcept
        : m_ref(ref), m_ptr(ptr)
        {
        }

    public:
      constexpr vector_iterator() noexcept
        : vector_iterator(nullptr, nullptr)
        {
        }
      template<typename yvalueT, typename enable_if<is_convertible<yvalueT *, valueT *>::value>::type * = nullptr>
        constexpr vector_iterator(const vector_iterator<vectorT, yvalueT> &other) noexcept
          : vector_iterator(other.m_ref, other.m_ptr)
        {
        }

    private:
      value_type * do_assert_valid_pointer(value_type *ptr, bool to_dereference) const noexcept
        {
          const auto ref = this->m_ref;
          ROCKET_ASSERT_MSG(ref, "This iterator has not been initialized.");
          const auto dist = static_cast<size_t>(ptr - ref->data());
          ROCKET_ASSERT_MSG(dist <= ref->size(), "This iterator has been invalidated.");
          ROCKET_ASSERT_MSG(!(to_dereference && (dist == ref->size())), "This iterator contains a past-the-end value and cannot be dereferenced.");
          return ptr;
        }

    public:
      const parent_type * parent() const noexcept
        {
          return this->m_ref;
        }

      value_type * tell() const noexcept
        {
          const auto ptr = this->do_assert_valid_pointer(this->m_ptr, false);
          return ptr;
        }
      value_type * tell_owned_by(const parent_type *ref) const noexcept
        {
          ROCKET_ASSERT_MSG(this->m_ref == ref, "This iterator does not refer to an element in the same container.");
          return this->tell();
        }
      vector_iterator & seek(value_type *ptr) noexcept
        {
          this->m_ptr = this->do_assert_valid_pointer(ptr, false);
          return *this;
        }

      reference operator*() const noexcept
        {
          const auto ptr = this->do_assert_valid_pointer(this->m_ptr, true);
          return *ptr;
        }
      pointer operator->() const noexcept
        {
          const auto ptr = this->do_assert_valid_pointer(this->m_ptr, true);
          return ptr;
        }
      reference operator[](difference_type off) const noexcept
        {
          const auto ptr = this->do_assert_valid_pointer(this->m_ptr + off, true);
          return *ptr;
        }
    };

  template<typename vectorT, typename valueT>
    inline vector_iterator<vectorT, valueT> & operator++(vector_iterator<vectorT, valueT> &rhs) noexcept
    {
      return rhs.seek(rhs.tell() + 1);
    }
  template<typename vectorT, typename valueT>
    inline vector_iterator<vectorT, valueT> & operator--(vector_iterator<vectorT, valueT> &rhs) noexcept
    {
      return rhs.seek(rhs.tell() - 1);
    }

  template<typename vectorT, typename valueT>
    inline vector_iterator<vectorT, valueT> operator++(vector_iterator<vectorT, valueT> &lhs, int) noexcept
    {
      auto res = lhs;
      lhs.seek(lhs.tell() + 1);
      return res;
    }
  template<typename vectorT, typename valueT>
    inline vector_iterator<vectorT, valueT> operator--(vector_iterator<vectorT, valueT> &lhs, int) noexcept
    {
      auto res = lhs;
      lhs.seek(lhs.tell() - 1);
      return res;
    }

  template<typename vectorT, typename valueT>
    inline vector_iterator<vectorT, valueT> & operator+=(vector_iterator<vectorT, valueT> &lhs, typename vector_iterator<vectorT, valueT>::difference_type rhs) noexcept
    {
      return lhs.seek(lhs.tell() + rhs);
    }
  template<typename vectorT, typename valueT>
    inline vector_iterator<vectorT, valueT> & operator-=(vector_iterator<vectorT, valueT> &lhs, typename vector_iterator<vectorT, valueT>::difference_type rhs) noexcept
    {
      return lhs.seek(lhs.tell() - rhs);
    }

  template<typename vectorT, typename valueT>
    inline vector_iterator<vectorT, valueT> operator+(const vector_iterator<vectorT, valueT> &lhs, typename vector_iterator<vectorT, valueT>::difference_type rhs) noexcept
    {
      auto res = lhs;
      res.seek(res.tell() + rhs);
      return res;
    }
  template<typename vectorT, typename valueT>
    inline vector_iterator<vectorT, valueT> operator-(const vector_iterator<vectorT, valueT> &lhs, typename vector_iterator<vectorT, valueT>::difference_type rhs) noexcept
    {
      auto res = lhs;
      res.seek(res.tell() - rhs);
      return res;
    }

  template<typename vectorT, typename valueT>
    inline vector_iterator<vectorT, valueT> operator+(typename vector_iterator<vectorT, valueT>::difference_type lhs, const vector_iterator<vectorT, valueT> &rhs) noexcept
    {
      auto res = rhs;
      res.seek(res.tell() + lhs);
      return res;
    }
  template<typename vectorT, typename xvalueT, typename yvalueT>
    inline typename vector_iterator<vectorT, xvalueT>::difference_type operator-(const vector_iterator<vectorT, xvalueT> &lhs, const vector_iterator<vectorT, yvalueT> &rhs) noexcept
    {
      return lhs.tell_owned_by(rhs.parent()) - rhs.tell();
    }

  template<typename vectorT, typename xvalueT, typename yvalueT>
    inline bool operator==(const vector_iterator<vectorT, xvalueT> &lhs, const vector_iterator<vectorT, yvalueT> &rhs) noexcept
    {
      return lhs.tell() == rhs.tell();
    }
  template<typename vectorT, typename xvalueT, typename yvalueT>
    inline bool operator!=(const vector_iterator<vectorT, xvalueT> &lhs, const vector_iterator<vectorT, yvalueT> &rhs) noexcept
    {
      return lhs.tell() != rhs.tell();
    }

  template<typename vectorT, typename xvalueT, typename yvalueT>
    inline bool operator<(const vector_iterator<vectorT, xvalueT> &lhs, const vector_iterator<vectorT, yvalueT> &rhs) noexcept
    {
      return lhs.tell_owned_by(rhs.parent()) < rhs.tell();
    }
  template<typename vectorT, typename xvalueT, typename yvalueT>
    inline bool operator>(const vector_iterator<vectorT, xvalueT> &lhs, const vector_iterator<vectorT, yvalueT> &rhs) noexcept
    {
      return lhs.tell_owned_by(rhs.parent()) > rhs.tell();
    }
  template<typename vectorT, typename xvalueT, typename yvalueT>
    inline bool operator<=(const vector_iterator<vectorT, xvalueT> &lhs, const vector_iterator<vectorT, yvalueT> &rhs) noexcept
    {
      return lhs.tell_owned_by(rhs.parent()) <= rhs.tell();
    }
  template<typename vectorT, typename xvalueT, typename yvalueT>
    inline bool operator>=(const vector_iterator<vectorT, xvalueT> &lhs, const vector_iterator<vectorT, yvalueT> &rhs) noexcept
    {
      return lhs.tell_owned_by(rhs.parent()) >= rhs.tell();
    }

  // Insertion helpers.
  struct append_tag
    {
    }
  constexpr append;

  template<typename vectorT, typename ...paramsT>
    inline void tagged_append(vectorT *vec, append_tag, paramsT &&...params)
    {
      vec->append(::std::forward<paramsT>(params)...);
    }

  struct emplace_back_tag
    {
    }
  constexpr emplace_back;

  template<typename vectorT, typename ...paramsT>
    inline void tagged_append(vectorT *vec, emplace_back_tag, paramsT &&...params)
    {
      vec->emplace_back(::std::forward<paramsT>(params)...);
    }

  struct push_back_tag
    {
    }
  constexpr push_back;

  template<typename vectorT, typename ...paramsT>
    inline void tagged_append(vectorT *vec, push_back_tag, paramsT &&...params)
    {
      vec->push_back(::std::forward<paramsT>(params)...);
    }

  }

template<typename valueT, typename allocatorT>
  class cow_vector
  {
    static_assert(!is_array<valueT>::value, "`valueT` must not be an array type.");
    static_assert(is_same<typename allocatorT::value_type, valueT>::value, "`allocatorT::value_type` must denote the same type as `valueT`.");

  public:
    // types
    using value_type      = valueT;
    using allocator_type  = allocatorT;

    using size_type        = typename allocator_traits<allocator_type>::size_type;
    using difference_type  = typename allocator_traits<allocator_type>::difference_type;
    using const_reference  = const value_type &;
    using reference        = value_type &;

    using const_iterator          = details_cow_vector::vector_iterator<cow_vector, const value_type>;
    using iterator                = details_cow_vector::vector_iterator<cow_vector, value_type>;
    using const_reverse_iterator  = ::std::reverse_iterator<const_iterator>;
    using reverse_iterator        = ::std::reverse_iterator<iterator>;

  private:
    details_cow_vector::storage_handle<allocator_type> m_sth;

  public:
    // 26.3.11.2, construct/copy/destroy
    explicit cow_vector(const allocator_type &alloc) noexcept
      : m_sth(alloc)
      {
      }
    cow_vector() noexcept(is_nothrow_constructible<allocator_type>::value)
      : cow_vector(allocator_type())
      {
      }
    cow_vector(const cow_vector &other) noexcept
      : cow_vector(allocator_traits<allocator_type>::select_on_container_copy_construction(other.m_sth.as_allocator()))
      {
        this->assign(other);
      }
    cow_vector(const cow_vector &other, const allocator_type &alloc) noexcept
      : cow_vector(alloc)
      {
        this->assign(other);
      }
    cow_vector(cow_vector &&other) noexcept
      : cow_vector(::std::move(other.m_sth.as_allocator()))
      {
        this->assign(::std::move(other));
      }
    cow_vector(cow_vector &&other, const allocator_type &alloc) noexcept
      : cow_vector(alloc)
      {
        this->assign(::std::move(other));
      }
    cow_vector(size_type n, const allocator_type &alloc = allocator_type())
      : cow_vector(alloc)
      {
        this->assign(n);
      }
    cow_vector(size_type n, const value_type &value, const allocator_type &alloc = allocator_type())
      : cow_vector(alloc)
      {
        this->assign(n, value);
      }
    template<typename inputT, typename iterator_traits<inputT>::iterator_category * = nullptr>
      cow_vector(inputT first, inputT last, const allocator_type &alloc = allocator_type())
      : cow_vector(alloc)
      {
        this->assign(::std::move(first), ::std::move(last));
      }
    cow_vector(initializer_list<value_type> init, const allocator_type &alloc = allocator_type())
      : cow_vector(alloc)
      {
        this->assign(init);
      }
    cow_vector & operator=(const cow_vector &other) noexcept
      {
        this->assign(other);
        allocator_copy_assigner<allocator_type>()(this->m_sth.as_allocator(), other.m_sth.as_allocator());
        return *this;
      }
    cow_vector & operator=(cow_vector &&other) noexcept
      {
        this->assign(::std::move(other));
        allocator_move_assigner<allocator_type>()(this->m_sth.as_allocator(), ::std::move(other.m_sth.as_allocator()));
        return *this;
      }
    cow_vector & operator=(initializer_list<value_type> init)
      {
        this->assign(init);
        return *this;
      }

  private:
    // Reallocate the storage to `res_arg` elements.
    // The storage is owned by the current vector exclusively after this function returns normally.
    value_type * do_reallocate(size_type cnt_one, size_type off_two, size_type cnt_two, size_type res_arg)
      {
        ROCKET_ASSERT(cnt_one <= off_two);
        ROCKET_ASSERT(off_two <= this->m_sth.size());
        ROCKET_ASSERT(cnt_two <= this->m_sth.size() - off_two);
        const auto ptr = this->m_sth.reallocate(cnt_one, off_two, cnt_two, res_arg);
        if(!ptr) {
          // The storage has been deallocated.
          return nullptr;
        }
        ROCKET_ASSERT(this->m_sth.unique());
        return ptr;
      }
    // Deallocate any dynamic storage.
    void deallocate() noexcept
      {
        this->m_sth.deallocate();
      }

    // Reallocate more storage as needed, without shrinking.
    void do_reserve_more(size_type cap_add)
      {
        const auto cnt = this->size();
        auto cap = this->m_sth.check_size_add(cnt, cap_add);
        if(!this->unique() || (this->capacity() < cap)) {
#ifndef ROCKET_DEBUG
          // Reserve more space for non-debug builds.
          cap = noadl::max(cap, cnt + cnt / 2 + 7);
#endif
          this->do_reallocate(0, 0, cnt, cap | 1);
        }
        ROCKET_ASSERT(this->capacity() >= cap);
      }

    template<typename ...paramsT>
      value_type * do_insert_no_bound_check(size_type tpos, paramsT &&...params)
      {
        const auto cnt_old = this->size();
        ROCKET_ASSERT(tpos <= cnt_old);
        details_cow_vector::tagged_append(this, ::std::forward<paramsT>(params)...);
        const auto cnt_add = this->size() - cnt_old;
        this->do_reserve_more(0);
        const auto ptr = this->m_sth.mut_data_unchecked();
        noadl::rotate(ptr, tpos, cnt_old, cnt_old + cnt_add);
        return ptr + tpos;
      }
    value_type * do_erase_no_bound_check(size_type tpos, size_type tn)
      {
        const auto cnt_old = this->size();
        ROCKET_ASSERT(tpos <= cnt_old);
        ROCKET_ASSERT(tn <= cnt_old - tpos);
        if(!this->unique()) {
          const auto ptr = this->do_reallocate(tpos, tpos + tn, cnt_old - (tpos + tn), cnt_old);
          return ptr + tpos;
        }
        const auto ptr = this->m_sth.mut_data_unchecked();
        noadl::rotate(ptr, tpos, tpos + tn, cnt_old);
        this->m_sth.pop_back_n_unchecked(tn);
        return ptr + tpos;
      }

  public:
    // iterators
    const_iterator begin() const noexcept
      {
        return const_iterator(this->m_sth, this->data());
      }
    const_iterator end() const noexcept
      {
        return const_iterator(this->m_sth, this->data() + this->size());
      }
    const_reverse_iterator rbegin() const noexcept
      {
        return const_reverse_iterator(this->end());
      }
    const_reverse_iterator rend() const noexcept
      {
        return const_reverse_iterator(this->begin());
      }

    const_iterator cbegin() const noexcept
      {
        return this->begin();
      }
    const_iterator cend() const noexcept
      {
        return this->end();
      }
    const_reverse_iterator crbegin() const noexcept
      {
        return this->rbegin();
      }
    const_reverse_iterator crend() const noexcept
      {
        return this->rend();
      }

    // N.B. This function may throw `std::bad_alloc`.
    // N.B. This is a non-standard extension.
    iterator mut_begin()
      {
        return iterator(this->m_sth, this->mut_data());
      }
    // N.B. This function may throw `std::bad_alloc`.
    // N.B. This is a non-standard extension.
    iterator mut_end()
      {
        return iterator(this->m_sth, this->mut_data() + this->size());
      }
    // N.B. This function may throw `std::bad_alloc`.
    // N.B. This is a non-standard extension.
    reverse_iterator mut_rbegin()
      {
        return reverse_iterator(this->mut_end());
      }
    // N.B. This function may throw `std::bad_alloc`.
    // N.B. This is a non-standard extension.
    reverse_iterator mut_rend()
      {
        return reverse_iterator(this->mut_begin());
      }

    // 26.3.11.3, capacity
    bool empty() const noexcept
      {
        return this->m_sth.size() == 0;
      }
    size_type size() const noexcept
      {
        return this->m_sth.size();
      }
    size_type max_size() const noexcept
      {
        return this->m_sth.max_size();
      }
    // N.B. The parameter pack is a non-standard extension.
    template<typename ...paramsT>
      void resize(size_type n, const paramsT &...params)
      {
        const auto cnt_old = this->size();
        if(cnt_old == n) {
          return;
        }
        if(cnt_old < n) {
          this->append(n - cnt_old, params...);
        } else {
          this->pop_back(cnt_old - n);
        }
        ROCKET_ASSERT(this->size() == n);
      }
    size_type capacity() const noexcept
      {
        return this->m_sth.capacity();
      }
    void reserve(size_type res_arg)
      {
        const auto cnt = this->size();
        const auto cap_new = this->m_sth.round_up_capacity(noadl::max(cnt, res_arg));
        // If the storage is shared with other vectors, force rellocation to prevent copy-on-write upon modification.
        if(this->unique() && (this->capacity() >= cap_new)) {
          return;
        }
        this->do_reallocate(0, 0, cnt, cap_new);
        ROCKET_ASSERT(this->capacity() >= res_arg);
      }
    void shrink_to_fit()
      {
        const auto cnt = this->size();
        const auto cap_min = this->m_sth.round_up_capacity(cnt);
        // Don't increase memory usage.
        if(!this->unique() || (this->capacity() <= cap_min)) {
          return;
        }
        this->do_reallocate(0, 0, cnt, cnt);
        ROCKET_ASSERT(this->capacity() <= cap_min);
      }
    void clear() noexcept
      {
        if(!this->unique()) {
          this->deallocate();
          return;
        }
        this->m_sth.pop_back_n_unchecked(this->size());
      }
    // N.B. This is a non-standard extension.
    bool unique() const noexcept
      {
        return this->m_sth.unique();
      }

    // element access
    const_reference at(size_type pos) const
      {
        const auto cnt = this->size();
        if(pos >= cnt) {
          noadl::throw_out_of_range("cow_vector: The subscript `%lld` is not a writable position within a vector of size `%lld`.",
                                    static_cast<long long>(pos), static_cast<long long>(cnt));
        }
        return this->data()[pos];
      }
    const_reference operator[](size_type pos) const noexcept
      {
        const auto cnt = this->size();
        ROCKET_ASSERT(pos < cnt);
        return this->data()[pos];
      }
    const_reference front() const noexcept
      {
        const auto cnt = this->size();
        ROCKET_ASSERT(cnt > 0);
        return this->data()[0];
      }
    const_reference back() const noexcept
      {
        const auto cnt = this->size();
        ROCKET_ASSERT(cnt > 0);
        return this->data()[cnt - 1];
      }

    // There is no `at()` overload that returns a non-const reference. This is the consequent overload which does that.
    // N.B. This is a non-standard extension.
    reference mut(size_type pos)
      {
        const auto cnt = this->size();
        if(pos >= cnt) {
          noadl::throw_out_of_range("cow_vector: The subscript `%lld` is not a writable position within a vector of size `%lld`.",
                                    static_cast<long long>(pos), static_cast<long long>(cnt));
        }
        return this->mut_data()[pos];
      }
    // N.B. This is a non-standard extension.
    reference mut_front()
      {
        const auto cnt = this->size();
        ROCKET_ASSERT(cnt > 0);
        return this->mut_data()[0];
      }
    // N.B. This is a non-standard extension.
    reference mut_back()
      {
        const auto cnt = this->size();
        ROCKET_ASSERT(cnt > 0);
        return this->mut_data()[cnt - 1];
      }

    // N.B. This is a non-standard extension.
    template<typename ...paramsT>
      cow_vector & append(size_type n, const paramsT &...params)
      {
        if(n == 0) {
          return *this;
        }
        this->do_reserve_more(n);
        noadl::ranged_do_while(size_type(0), n, [&](size_type, const paramsT &...fwdp) { this->m_sth.emplace_back_unchecked(fwdp...); }, params...);
        return *this;
      }
    // N.B. This is a non-standard extension.
    cow_vector & append(initializer_list<value_type> init)
      {
        return this->append(init.begin(), init.end());
      }
    // N.B. This is a non-standard extension.
    template<typename inputT, typename iterator_traits<inputT>::iterator_category * = nullptr>
      cow_vector & append(inputT first, inputT last)
      {
        if(first == last) {
          return *this;
        }
        const auto dist = noadl::estimate_distance(first, last);
        if(dist == 0) {
          noadl::ranged_do_while(::std::move(first), ::std::move(last), [&](const inputT &it) { this->emplace_back(*it); });
          return *this;
        }
        this->do_reserve_more(dist);
        noadl::ranged_do_while(::std::move(first), ::std::move(last), [&](const inputT &it) { this->m_sth.emplace_back_unchecked(*it); });
        return *this;
      }
    // 26.3.11.5, modifiers
    template<typename ...paramsT>
      reference emplace_back(paramsT &&...params)
      {
        this->do_reserve_more(1);
        const auto ptr = this->m_sth.emplace_back_unchecked(::std::forward<paramsT>(params)...);
        return *ptr;
      }
    // N.B. The return type is a non-standard extension.
    reference push_back(const value_type &value)
      {
        const auto cnt_old = this->size();
        // Check for overlapped elements before `do_reserve_more()`.
        const auto srpos = static_cast<uintptr_t>(::std::addressof(value) - this->data());
        this->do_reserve_more(1);
        if(srpos < cnt_old) {
          const auto ptr = this->m_sth.emplace_back_unchecked(this->m_sth.mut_data_unchecked()[srpos]);
          return *ptr;
        }
        const auto ptr = this->m_sth.emplace_back_unchecked(value);
        return *ptr;
      }
    // N.B. The return type is a non-standard extension.
    reference push_back(value_type &&value)
      {
        this->do_reserve_more(1);
        const auto ptr = this->m_sth.emplace_back_unchecked(::std::move(value));
        return *ptr;
      }

    iterator insert(const_iterator tins, const value_type &value)
      {
        const auto tpos = static_cast<size_type>(tins.tell_owned_by(this->m_sth) - this->data());
        const auto ptr = this->do_insert_no_bound_check(tpos, details_cow_vector::push_back, value);
        return iterator(this->m_sth, ptr);
      }
    iterator insert(const_iterator tins, value_type &&value)
      {
        const auto tpos = static_cast<size_type>(tins.tell_owned_by(this->m_sth) - this->data());
        const auto ptr = this->do_insert_no_bound_check(tpos, details_cow_vector::push_back, ::std::move(value));
        return iterator(this->m_sth, ptr);
      }
    // N.B. The parameter pack is a non-standard extension.
    template<typename ...paramsT>
      iterator insert(const_iterator tins, size_type n, const paramsT &...params)
      {
        const auto tpos = static_cast<size_type>(tins.tell_owned_by(this->m_sth) - this->data());
        const auto ptr = this->do_insert_no_bound_check(tpos, details_cow_vector::append, n, params...);
        return iterator(this->m_sth, ptr);
      }
    iterator insert(const_iterator tins, initializer_list<value_type> init)
      {
        const auto tpos = static_cast<size_type>(tins.tell_owned_by(this->m_sth) - this->data());
        const auto ptr = this->do_insert_no_bound_check(tpos, details_cow_vector::append, init);
        return iterator(this->m_sth, ptr);
      }
    template<typename inputT, typename iterator_traits<inputT>::iterator_category * = nullptr>
      iterator insert(const_iterator tins, inputT first, inputT last)
      {
        const auto tpos = static_cast<size_type>(tins.tell_owned_by(this->m_sth) - this->data());
        const auto ptr = this->do_insert_no_bound_check(tpos, details_cow_vector::append, ::std::move(first), ::std::move(last));
        return iterator(this->m_sth, ptr);
      }

    // N.B. This function may throw `std::bad_alloc`.
    iterator erase(const_iterator tfirst, const_iterator tlast)
      {
        const auto tpos = static_cast<size_type>(tfirst.tell_owned_by(this->m_sth) - this->data());
        const auto tn = static_cast<size_type>(tlast.tell_owned_by(this->m_sth) - tfirst.tell());
        const auto ptr = this->do_erase_no_bound_check(tpos, tn);
        return iterator(this->m_sth, ptr);
      }
    // N.B. This function may throw `std::bad_alloc`.
    iterator erase(const_iterator tfirst)
      {
        const auto tpos = static_cast<size_type>(tfirst.tell_owned_by(this->m_sth) - this->data());
        const auto ptr = this->do_erase_no_bound_check(tpos, 1);
        return iterator(this->m_sth, ptr);
      }
    // N.B. This function may throw `std::bad_alloc`.
    // N.B. The return type and parameter are non-standard extensions.
    cow_vector & pop_back(size_type n = 1)
      {
        const auto cnt_old = this->size();
        ROCKET_ASSERT(n <= cnt_old);
        if(n == 0) {
          return *this;
        }
        if(!this->unique()) {
          this->do_reallocate(0, 0, cnt_old - n, cnt_old);
          return *this;
        }
        this->m_sth.pop_back_n_unchecked(n);
        return *this;
      }

    // N.B. The return type is a non-standard extension.
    cow_vector & assign(const cow_vector &other) noexcept
      {
        this->m_sth.share_with(other.m_sth);
        return *this;
      }
    // N.B. The return type is a non-standard extension.
    cow_vector & assign(cow_vector &&other) noexcept
      {
        this->m_sth.share_with(::std::move(other.m_sth));
        return *this;
      }
    // N.B. The parameter pack is a non-standard extension.
    // N.B. The return type is a non-standard extension.
    template<typename ...paramsT>
      cow_vector & assign(size_type n, const paramsT &...params)
      {
        this->clear();
        this->append(n, params...);
        return *this;
      }
    // N.B. The return type is a non-standard extension.
    cow_vector & assign(initializer_list<value_type> init)
      {
        this->clear();
        this->append(init);
        return *this;
      }
    // N.B. The return type is a non-standard extension.
    template<typename inputT, typename iterator_traits<inputT>::iterator_category * = nullptr>
      cow_vector & assign(inputT first, inputT last)
      {
        this->clear();
        this->append(::std::move(first), ::std::move(last));
        return *this;
      }

    void swap(cow_vector &other) noexcept
      {
        this->m_sth.exchange_with(other.m_sth);
        allocator_swapper<allocator_type>()(this->m_sth.as_allocator(), other.m_sth.as_allocator());
      }

    // 26.3.11.4, data access
    const value_type * data() const noexcept
      {
        return this->m_sth.data();
      }

    // Get a pointer to mutable data. This function may throw `std::bad_alloc`.
    // N.B. This is a non-standard extension.
    value_type * mut_data()
      {
        if(!this->unique()) {
          return this->do_reallocate(0, 0, this->size(), this->size() | 1);
        }
        return this->m_sth.mut_data_unchecked();
      }

    // N.B. The return type differs from `std::vector`.
    const allocator_type & get_allocator() const noexcept
      {
        return this->m_sth.as_allocator();
      }
    allocator_type & get_allocator() noexcept
      {
        return this->m_sth.as_allocator();
      }
  };

template<typename valueT, typename allocatorT>
  inline void swap(cow_vector<valueT, allocatorT> &lhs, cow_vector<valueT, allocatorT> &rhs) noexcept
  {
    lhs.swap(rhs);
  }

}

#endif
