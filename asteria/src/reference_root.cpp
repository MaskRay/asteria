// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "reference_root.hpp"
#include "abstract_variable_callback.hpp"
#include "utilities.hpp"

namespace Asteria {

Reference_root::~Reference_root()
  {
  }

const Value & Reference_root::dereference_readonly() const
  {
    switch(this->index()) {
      case index_constant: {
        const auto &alt = this->check<S_constant>();
        return alt.src;
      }
      case index_temporary: {
        const auto &alt = this->check<S_temporary>();
        return alt.value;
      }
      case index_variable: {
        const auto &alt = this->check<S_variable>();
        return alt.var->get_value();
      }
      default: {
        ASTERIA_TERMINATE("An unknown reference root type enumeration `", this->index(), "` has been encountered.");
      }
    }
  }

Value & Reference_root::dereference_mutable() const
  {
    switch(this->index()) {
      case index_constant: {
        const auto &alt = this->check<S_constant>();
        ASTERIA_THROW_RUNTIME_ERROR("The constant `", alt.src, "` cannot be modified.");
      }
      case index_temporary: {
        const auto &alt = this->check<S_temporary>();
        ASTERIA_THROW_RUNTIME_ERROR("The temporary value `", alt.value, "` cannot be modified.");
      }
      case index_variable: {
        const auto &alt = this->check<S_variable>();
        if(alt.var->is_immutable()) {
          ASTERIA_THROW_RUNTIME_ERROR("The variable having value `", alt.var->get_value(), "` is immutable and cannot be modified.");
        }
        return alt.var->get_value();
      }
      default: {
        ASTERIA_TERMINATE("An unknown reference root type enumeration `", this->index(), "` has been encountered.");
      }
    }
  }

void Reference_root::enumerate_variables(const Abstract_variable_callback &callback) const
  {
    switch(this->index()) {
      case index_constant: {
        const auto &alt = this->check<S_constant>();
        alt.src.enumerate_variables(callback);
        return;
      }
      case index_temporary: {
        const auto &alt = this->check<S_temporary>();
        alt.value.enumerate_variables(callback);
        return;
      }
      case index_variable: {
        const auto &alt = this->check<S_variable>();
        if(callback.accept(alt.var)) {
          // Descend into this variable recursively when the callback returns `true`.
          alt.var->enumerate_variables(callback);
        }
        return;
      }
      default: {
        ASTERIA_TERMINATE("An unknown reference root type enumeration `", this->index(), "` has been encountered.");
      }
    }
  }

}
