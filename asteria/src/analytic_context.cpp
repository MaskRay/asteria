// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "analytic_context.hpp"
#include "function_header.hpp"
#include "reference.hpp"
#include "utilities.hpp"

namespace Asteria {

Analytic_context::~Analytic_context()
  {
  }

bool Analytic_context::is_analytic() const noexcept
  {
    return true;
  }
const Abstract_context * Analytic_context::get_parent_opt() const noexcept
  {
    return this->m_parent_opt;
  }

const Reference * Analytic_context::get_named_reference_opt(const String &name) const
  {
    const auto qbase = this->Abstract_context::get_named_reference_opt(name);
    if(qbase) {
      return qbase;
    }
    // Deal with pre-defined variables.
    // If you add new entries or alter existent entries here, you must update `Executive_context` as well.
    if(name == "__file") {
      return &(this->m_dummy);
    }
    if(name == "__line") {
      return &(this->m_dummy);
    }
    if(name == "__func") {
      return &(this->m_dummy);
    }
    if(name == "__this") {
      return &(this->m_dummy);
    }
    if(name == "__varg") {
      return &(this->m_dummy);
    }
    return nullptr;
  }

void Analytic_context::initialize_for_function(const Function_header &head)
  {
    for(const auto &param : head.get_params()) {
      if(!param.empty()) {
        if(this->is_name_reserved(param)) {
          ASTERIA_THROW_RUNTIME_ERROR("The function parameter name `", param, "` is reserved and cannot be used.");
        }
        this->Abstract_context::set_named_reference(param, this->m_dummy);
      }
    }
  }

}
