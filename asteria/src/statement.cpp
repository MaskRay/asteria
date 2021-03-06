// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "statement.hpp"
#include "xpnode.hpp"
#include "global_context.hpp"
#include "analytic_context.hpp"
#include "executive_context.hpp"
#include "variable.hpp"
#include "instantiated_function.hpp"
#include "exception.hpp"
#include "utilities.hpp"

namespace Asteria {

Statement::~Statement()
  {
  }

  namespace {

  void do_safe_set_named_reference(Abstract_context &ctx_io, const char *desc, const String &name, Reference ref)
    {
      if(name.empty()) {
        return;
      }
      if(ctx_io.is_name_reserved(name)) {
        ASTERIA_THROW_RUNTIME_ERROR("The name `", name, "` of this ", desc, " is reserved and cannot be used.");
      }
      ctx_io.set_named_reference(name, std::move(ref));
    }

  }

void Statement::fly_over_in_place(Abstract_context &ctx_io) const
  {
    switch(Index(this->m_stor.index())) {
      case index_expr:
      case index_block: {
        return;
      }
      case index_var_def: {
        const auto &alt = this->m_stor.as<S_var_def>();
        // Create a dummy reference for further name lookups.
        do_safe_set_named_reference(ctx_io, "skipped variable", alt.name, { });
        return;
      }
      case index_func_def: {
        const auto &alt = this->m_stor.as<S_func_def>();
        // Create a dummy reference for further name lookups.
        do_safe_set_named_reference(ctx_io, "skipped function", alt.head.get_func(), { });
        return;
      }
      case index_if:
      case index_switch:
      case index_do_while:
      case index_while:
      case index_for:
      case index_for_each:
      case index_try:
      case index_break:
      case index_continue:
      case index_throw:
      case index_return: {
        break;
      }
      default: {
        ASTERIA_TERMINATE("An unknown statement type enumeration `", this->m_stor.index(), "` has been encountered.");
      }
    }
  }

Statement Statement::bind_in_place(Analytic_context &ctx_io, const Global_context &global) const
  {
    switch(Index(this->m_stor.index())) {
      case index_expr: {
        const auto &alt = this->m_stor.as<S_expr>();
        // Bind the expression recursively.
        auto expr_bnd = alt.expr.bind(global, ctx_io);
        Statement::S_expr alt_bnd = { std::move(expr_bnd) };
        return std::move(alt_bnd);
      }
      case index_block: {
        const auto &alt = this->m_stor.as<S_block>();
        // Bind the body recursively.
        auto body_bnd = alt.body.bind(global, ctx_io);
        Statement::S_block alt_bnd = { std::move(body_bnd) };
        return std::move(alt_bnd);
      }
      case index_var_def: {
        const auto &alt = this->m_stor.as<S_var_def>();
        // Create a dummy reference for further name lookups.
        do_safe_set_named_reference(ctx_io, "variable", alt.name, { });
        // Bind the initializer recursively.
        auto init_bnd = alt.init.bind(global, ctx_io);
        Statement::S_var_def alt_bnd = { alt.name, alt.immutable, std::move(init_bnd) };
        return std::move(alt_bnd);
      }
      case index_func_def: {
        const auto &alt = this->m_stor.as<S_func_def>();
        // Create a dummy reference for further name lookups.
        do_safe_set_named_reference(ctx_io, "function", alt.head.get_func(), { });
        // Bind the function body recursively.
        Analytic_context ctx_next(&ctx_io);
        ctx_next.initialize_for_function(alt.head);
        auto body_bnd = alt.body.bind_in_place(ctx_next, global);
        Statement::S_func_def alt_bnd = { alt.head, std::move(body_bnd) };
        return std::move(alt_bnd);
      }
      case index_if: {
        const auto &alt = this->m_stor.as<S_if>();
        // Bind the condition and both branches recursively.
        auto cond_bnd = alt.cond.bind(global, ctx_io);
        auto branch_true_bnd = alt.branch_true.bind(global, ctx_io);
        auto branch_false_bnd = alt.branch_false.bind(global, ctx_io);
        Statement::S_if alt_bnd = { std::move(cond_bnd), std::move(branch_true_bnd), std::move(branch_false_bnd) };
        return std::move(alt_bnd);
      }
      case index_switch: {
        const auto &alt = this->m_stor.as<S_switch>();
        // Bind the control expression and all clauses recursively.
        auto ctrl_bnd = alt.ctrl.bind(global, ctx_io);
        // Note that all `switch` clauses share the same context.
        Analytic_context ctx_next(&ctx_io);
        Bivector<Expression, Block> clauses_bnd;
        clauses_bnd.reserve(alt.clauses.size());
        for(const auto &pair : alt.clauses) {
          auto first_bnd = pair.first.bind(global, ctx_next);
          auto second_bnd = pair.second.bind_in_place(ctx_next, global);
          clauses_bnd.emplace_back(std::move(first_bnd), std::move(second_bnd));
        }
        Statement::S_switch alt_bnd = { std::move(ctrl_bnd), std::move(clauses_bnd) };
        return std::move(alt_bnd);
      }
      case index_do_while: {
        const auto &alt = this->m_stor.as<S_do_while>();
        // Bind the loop body and condition recursively.
        auto body_bnd = alt.body.bind(global, ctx_io);
        auto cond_bnd = alt.cond.bind(global, ctx_io);
        Statement::S_do_while alt_bnd = { std::move(body_bnd), std::move(cond_bnd) };
        return std::move(alt_bnd);
      }
      case index_while: {
        const auto &alt = this->m_stor.as<S_while>();
        // Bind the condition and loop body recursively.
        auto cond_bnd = alt.cond.bind(global, ctx_io);
        auto body_bnd = alt.body.bind(global, ctx_io);
        Statement::S_while alt_bnd = { std::move(cond_bnd), std::move(body_bnd) };
        return std::move(alt_bnd);
      }
      case index_for: {
        const auto &alt = this->m_stor.as<S_for>();
        // If the initialization part is a variable definition, the variable defined shall not outlast the loop body.
        Analytic_context ctx_next(&ctx_io);
        // Bind the loop initializer, condition, step expression and loop body recursively.
        auto init_bnd = alt.init.bind(global, ctx_next);
        auto cond_bnd = alt.cond.bind(global, ctx_next);
        auto step_bnd = alt.step.bind(global, ctx_next);
        auto body_bnd = alt.body.bind(global, ctx_next);
        Statement::S_for alt_bnd = { std::move(init_bnd), std::move(cond_bnd), std::move(step_bnd), std::move(body_bnd) };
        return std::move(alt_bnd);
      }
      case index_for_each: {
        const auto &alt = this->m_stor.as<S_for_each>();
        // The key and mapped variables shall not outlast the loop body.
        Analytic_context ctx_next(&ctx_io);
        do_safe_set_named_reference(ctx_next, "`for each` key", alt.key_name, { });
        do_safe_set_named_reference(ctx_next, "`for each` reference", alt.mapped_name, { });
        // Bind the range initializer and loop body recursively.
        auto init_bnd = alt.init.bind(global, ctx_next);
        auto body_bnd = alt.body.bind(global, ctx_next);
        Statement::S_for_each alt_bnd = { alt.key_name, alt.mapped_name, std::move(init_bnd), std::move(body_bnd) };
        return std::move(alt_bnd);
      }
      case index_try: {
        const auto &alt = this->m_stor.as<S_try>();
        // The `try` branch needs no special treatement.
        auto body_try_bnd = alt.body_try.bind(global, ctx_io);
        // The exception variable shall not outlast the `catch` body.
        Analytic_context ctx_next(&ctx_io);
        do_safe_set_named_reference(ctx_next, "exception", alt.except_name, { });
        // Bind the `catch` branch recursively.
        auto body_catch_bnd = alt.body_catch.bind_in_place(ctx_next, global);
        Statement::S_try alt_bnd = { std::move(body_try_bnd), alt.except_name, std::move(body_catch_bnd) };
        return std::move(alt_bnd);
      }
      case index_break: {
        const auto &alt = this->m_stor.as<S_break>();
        // Copy it as-is.
        Statement::S_break alt_bnd = { alt.target };
        return std::move(alt_bnd);
      }
      case index_continue: {
        const auto &alt = this->m_stor.as<S_continue>();
        // Copy it as-is.
        Statement::S_continue alt_bnd = { alt.target };
        return std::move(alt_bnd);
      }
      case index_throw: {
        const auto &alt = this->m_stor.as<S_throw>();
        // Bind the exception initializer recursively.
        auto expr_bnd = alt.expr.bind(global, ctx_io);
        Statement::S_throw alt_bnd = { alt.loc, std::move(expr_bnd) };
        return std::move(alt_bnd);
      }
      case index_return: {
        const auto &alt = this->m_stor.as<S_return>();
        // Bind the result initializer recursively.
        auto expr_bnd = alt.expr.bind(global, ctx_io);
        Statement::S_return alt_bnd = { alt.by_ref, std::move(expr_bnd) };
        return std::move(alt_bnd);
      }
      default: {
        ASTERIA_TERMINATE("An unknown statement type enumeration `", this->m_stor.index(), "` has been encountered.");
      }
    }
  }

Block::Status Statement::execute_in_place(Reference &ref_out, Executive_context &ctx_io, Global_context &global) const
  {
    switch(Index(this->m_stor.index())) {
      case index_expr: {
        const auto &alt = this->m_stor.as<S_expr>();
        // Evaluate the expression.
        ref_out = alt.expr.evaluate(global, ctx_io);
        return Block::status_next;
      }
      case index_block: {
        const auto &alt = this->m_stor.as<S_block>();
        // Execute the body.
        return alt.body.execute(ref_out, global, ctx_io);
      }
      case index_var_def: {
        const auto &alt = this->m_stor.as<S_var_def>();
        // Create a dummy reference for further name lookups.
        // A variable becomes visible before its initializer, where it is initialized to `null`.
        const auto var = global.create_tracked_variable();
        Reference_root::S_variable ref_c = { var };
        do_safe_set_named_reference(ctx_io, "variable", alt.name, std::move(ref_c));
        // Create a variable using the initializer.
        ref_out = alt.init.evaluate(global, ctx_io);
        auto value = ref_out.read();
        ASTERIA_DEBUG_LOG("Creating named variable: name = ", alt.name, ", immutable = ", alt.immutable, ": ", value);
        var->reset(std::move(value), alt.immutable);
        return Block::status_next;
      }
      case index_func_def: {
        const auto &alt = this->m_stor.as<S_func_def>();
        // Create a dummy reference for further name lookups.
        // A function becomes visible before its definition, where it is initialized to `null`.
        const auto var = global.create_tracked_variable();
        Reference_root::S_variable ref_c = { var };
        do_safe_set_named_reference(ctx_io, "function", alt.head.get_func(), std::move(ref_c));
        // Instantiate the function here.
        auto func = alt.body.instantiate_function(global, ctx_io, alt.head);
        ASTERIA_DEBUG_LOG("Creating named function: prototype = ", alt.head, ", location = ", alt.head.get_location());
        var->reset(D_function(std::move(func)), true);
        return Block::status_next;
      }
      case index_if: {
        const auto &alt = this->m_stor.as<S_if>();
        // Evaluate the condition and pick a branch.
        ref_out = alt.cond.evaluate(global, ctx_io);
        const auto status = (ref_out.read().test() ? alt.branch_true : alt.branch_false).execute(ref_out, global, ctx_io);
        if(status != Block::status_next) {
          // Forward anything unexpected to the caller.
          return status;
        }
        return Block::status_next;
      }
      case index_switch: {
        const auto &alt = this->m_stor.as<S_switch>();
        // Evaluate the control expression.
        ref_out = alt.ctrl.evaluate(global, ctx_io);
        const auto value_ctrl = ref_out.read();
        // Note that all `switch` clauses share the same context.
        // We will iterate from the first clause to the last one. If a `default` clause is encountered in the middle
        // and there is no match `case` clause, we will have to jump back into half of the scope. To simplify design,
        // a nested scope is created when a `default` clause is encountered. When jumping to the `default` scope, we
        // simply discard the new scope.
        Executive_context ctx_first(&ctx_io);
        Executive_context ctx_second(&ctx_first);
        auto ctx_next = std::ref(ctx_first);
        // There is a 'match' at the end of the clause array initially.
        auto match = alt.clauses.end();
        // This is a pointer to where new references are created.
        auto ctx_test = ctx_next;
        for(auto it = alt.clauses.begin(); it != alt.clauses.end(); ++it) {
          if(it->first.empty()) {
            // This is a `default` clause.
            if(match != alt.clauses.end()) {
              ASTERIA_THROW_RUNTIME_ERROR("Multiple `default` clauses exist in the same `switch` statement.");
            }
            // From now on, all declarations go into the second context.
            match = it;
            ctx_test = std::ref(ctx_second);
          } else {
            // This is a `case` clause.
            ref_out = it->first.evaluate(global, ctx_next);
            const auto value_comp = ref_out.read();
            if(value_ctrl.compare(value_comp) == Value::compare_equal) {
              // If this is a match, we resume from wherever `ctx_test` is pointing.
              match = it;
              ctx_next = ctx_test;
              break;
            }
          }
          // Create null references for declarations in the clause skipped.
          it->second.fly_over_in_place(ctx_test);
        }
        // Iterate from the match clause to the end of the body, falling through clause boundaries if any.
        for(auto it = match; it != alt.clauses.end(); ++it) {
          const auto status = it->second.execute_in_place(ref_out, ctx_next, global);
          if(rocket::is_any_of(status, { Block::status_break_unspec, Block::status_break_switch })) {
            // Break out of the body as requested.
            break;
          }
          if(status != Block::status_next) {
            // Forward anything unexpected to the caller.
            return status;
          }
        }
        return Block::status_next;
      }
      case index_do_while: {
        const auto &alt = this->m_stor.as<S_do_while>();
        for(;;) {
          // Execute the loop body.
          Executive_context ctx_next(&ctx_io);
          const auto status = alt.body.execute_in_place(ref_out, ctx_next, global);
          if(rocket::is_any_of(status, { Block::status_break_unspec, Block::status_break_while })) {
            // Break out of the body as requested.
            break;
          }
          if(rocket::is_none_of(status, { Block::status_next, Block::status_continue_unspec, Block::status_continue_while })) {
            // Forward anything unexpected to the caller.
            return status;
          }
          // Check the loop condition.
          // This differs from a `while` loop where the context for the loop body is destroyed before this check.
          ref_out = alt.cond.evaluate(global, ctx_next);
          if(!ref_out.read().test()) {
            break;
          }
        }
        return Block::status_next;
      }
      case index_while: {
        const auto &alt = this->m_stor.as<S_while>();
        for(;;) {
          // Check the loop condition.
          ref_out = alt.cond.evaluate(global, ctx_io);
          if(!ref_out.read().test()) {
            break;
          }
          // Execute the loop body.
          const auto status = alt.body.execute(ref_out, global, ctx_io);
          if(rocket::is_any_of(status, { Block::status_break_unspec, Block::status_break_while })) {
            // Break out of the body as requested.
            break;
          }
          if(rocket::is_none_of(status, { Block::status_next, Block::status_continue_unspec, Block::status_continue_while })) {
            // Forward anything unexpected to the caller.
            return status;
          }
        }
        return Block::status_next;
      }
      case index_for: {
        const auto &alt = this->m_stor.as<S_for>();
        // If the initialization part is a variable definition, the variable defined shall not outlast the loop body.
        Executive_context ctx_next(&ctx_io);
        // Execute the initializer. The status is ignored.
        ASTERIA_DEBUG_LOG("Begin running `for` initialization...");
        alt.init.execute_in_place(ref_out, ctx_next, global);
        ASTERIA_DEBUG_LOG("Done running `for` initialization: ", ref_out.read());
        for(;;) {
          // Check the loop condition.
          if(!alt.cond.empty()) {
            ref_out = alt.cond.evaluate(global, ctx_next);
            if(!ref_out.read().test()) {
              break;
            }
          }
          // Execute the loop body.
          const auto status = alt.body.execute(ref_out, global, ctx_next);
          if(rocket::is_any_of(status, { Block::status_break_unspec, Block::status_break_for })) {
            // Break out of the body as requested.
            break;
          }
          if(rocket::is_none_of(status, { Block::status_next, Block::status_continue_unspec, Block::status_continue_for })) {
            // Forward anything unexpected to the caller.
            return status;
          }
          // Evaluate the loop step expression.
          alt.step.evaluate(global, ctx_next);
        }
        return Block::status_next;
      }
      case index_for_each: {
        const auto &alt = this->m_stor.as<S_for_each>();
        // The key and mapped variables shall not outlast the loop body.
        Executive_context ctx_for(&ctx_io);
        // A variable becomes visible before its initializer, where it is initialized to `null`.
        do_safe_set_named_reference(ctx_for, "`for each` key", alt.key_name, { });
        do_safe_set_named_reference(ctx_for, "`for each` reference", alt.mapped_name, { });
        // Calculate the range using the initializer.
        auto mapped = alt.init.evaluate(global, ctx_for);
        const auto range_value = mapped.read();
        switch(rocket::weaken_enum(range_value.type())) {
          case Value::type_array: {
            const auto &array = range_value.check<D_array>();
            for(auto it = array.begin(); it != array.end(); ++it) {
              Executive_context ctx_next(&ctx_for);
              // Initialize the per-loop key constant.
              auto key = D_integer(it - array.begin());
              ASTERIA_DEBUG_LOG("Creating key constant with `for each` scope: name = ", alt.key_name, ": ", key);
              Reference_root::S_constant ref_c = { std::move(key) };
              do_safe_set_named_reference(ctx_for, "`for each` key", alt.key_name, std::move(ref_c));
              // Initialize the per-loop value reference.
              Reference_modifier::S_array_index refmod_c = { it - array.begin() };
              mapped.zoom_in(std::move(refmod_c));
              do_safe_set_named_reference(ctx_for, "`for each` reference", alt.mapped_name, mapped);
              ASTERIA_DEBUG_LOG("Created value reference with `for each` scope: name = ", alt.mapped_name, ": ", mapped.read());
              mapped.zoom_out();
              // Execute the loop body.
              const auto status = alt.body.execute_in_place(ref_out, ctx_next, global);
              if(rocket::is_any_of(status, { Block::status_break_unspec, Block::status_break_for })) {
                // Break out of the body as requested.
                break;
              }
              if(rocket::is_none_of(status, { Block::status_next, Block::status_continue_unspec, Block::status_continue_for })) {
                // Forward anything unexpected to the caller.
                return status;
              }
            }
            break;
          }
          case Value::type_object: {
            const auto &object = range_value.check<D_object>();
            for(auto it = object.begin(); it != object.end(); ++it) {
              Executive_context ctx_next(&ctx_for);
              // Initialize the per-loop key constant.
              auto key = D_string(it->first);
              ASTERIA_DEBUG_LOG("Creating key constant with `for each` scope: name = ", alt.key_name, ": ", key);
              Reference_root::S_constant ref_c = { std::move(key) };
              do_safe_set_named_reference(ctx_for, "`for each` key", alt.key_name, std::move(ref_c));
              // Initialize the per-loop value reference.
              Reference_modifier::S_object_key refmod_c = { it->first };
              mapped.zoom_in(std::move(refmod_c));
              do_safe_set_named_reference(ctx_for, "`for each` reference", alt.mapped_name, mapped);
              ASTERIA_DEBUG_LOG("Created value reference with `for each` scope: name = ", alt.mapped_name, ": ", mapped.read());
              mapped.zoom_out();
              // Execute the loop body.
              const auto status = alt.body.execute_in_place(ref_out, ctx_next, global);
              if(rocket::is_any_of(status, { Block::status_break_unspec, Block::status_break_for })) {
                // Break out of the body as requested.
                break;
              }
              if(rocket::is_none_of(status, { Block::status_next, Block::status_continue_unspec, Block::status_continue_for })) {
                // Forward anything unexpected to the caller.
                return status;
              }
            }
            break;
          }
          default: {
            ASTERIA_THROW_RUNTIME_ERROR("The `for each` statement does not accept a range of type `", Value::get_type_name(range_value.type()), "`.");
          }
        }
        return Block::status_next;
      }
      case index_try: {
        const auto &alt = this->m_stor.as<S_try>();
        try {
          // Execute the `try` body.
          // This is straightforward and hopefully zero-cost if no exception is thrown.
          const auto status = alt.body_try.execute(ref_out, global, ctx_io);
          if(status != Block::status_next) {
            // Forward anything unexpected to the caller.
            return status;
          }
        } catch(std::exception &stdex) {
          // Prepare the backtrace as an `array` for processing by code inside `catch`.
          D_array backtrace;
          const auto push_backtrace =
            [&](const Source_location &loc)
              {
                D_object elem;
                elem.insert_or_assign(String::shallow("file"), D_string(loc.get_file()));
                elem.insert_or_assign(String::shallow("line"), D_integer(loc.get_line()));
                backtrace.emplace_back(std::move(elem));
              };
          // The exception variable shall not outlast the `catch` body.
          Executive_context ctx_next(&ctx_io);
          try {
            // Translate the exception as needed.
            throw;
          } catch(Exception &except) {
            // Handle an `Asteria::Exception`.
            ASTERIA_DEBUG_LOG("Creating exception reference with `catch` scope: name = ", alt.except_name, ": ", except.get_value());
            Reference_root::S_temporary ref_c = { except.get_value() };
            do_safe_set_named_reference(ctx_next, "exception", alt.except_name, std::move(ref_c));
            // Unpack the backtrace array.
            backtrace.reserve(1 + except.get_backtrace().size());
            push_backtrace(except.get_location());
            std::for_each(except.get_backtrace().begin(), except.get_backtrace().end(), push_backtrace);
          } catch(...) {
            // Handle an `std::exception`.
            ASTERIA_DEBUG_LOG("Creating exception reference with `catch` scope: name = ", alt.except_name, ": ", stdex.what());
            Reference_root::S_temporary ref_c = { D_string(stdex.what()) };
            do_safe_set_named_reference(ctx_next, "exception", alt.except_name, std::move(ref_c));
            // We say the exception was thrown from native code.
            push_backtrace(Exception(stdex).get_location());
          }
          ASTERIA_DEBUG_LOG("Exception backtrace:\n", Value(backtrace));
          Reference_root::S_temporary ref_c = { std::move(backtrace) };
          ctx_next.set_named_reference(String::shallow("__backtrace"), std::move(ref_c));
          // Execute the `catch` body.
          const auto status = alt.body_catch.execute(ref_out, global, ctx_next);
          if(status != Block::status_next) {
            // Forward anything unexpected to the caller.
            return status;
          }
        }
        return Block::status_next;
      }
      case index_break: {
        const auto &alt = this->m_stor.as<S_break>();
        switch(rocket::weaken_enum(alt.target)) {
          case Statement::target_switch: {
            return Block::status_break_switch;
          }
          case Statement::target_while: {
            return Block::status_break_while;
          }
          case Statement::target_for: {
            return Block::status_break_for;
          }
        }
        return Block::status_break_unspec;
      }
      case index_continue: {
        const auto &alt = this->m_stor.as<S_continue>();
        switch(rocket::weaken_enum(alt.target)) {
          case Statement::target_switch: {
            ASTERIA_TERMINATE("`target_switch` is not allowed to follow `continue`.");
          }
          case Statement::target_while: {
            return Block::status_continue_while;
          }
          case Statement::target_for: {
            return Block::status_continue_for;
          }
        }
        return Block::status_continue_unspec;
      }
      case index_throw: {
        const auto &alt = this->m_stor.as<S_throw>();
        // Evaluate the expression.
        ref_out = alt.expr.evaluate(global, ctx_io);
        auto value = ref_out.read();
        ASTERIA_DEBUG_LOG("Throwing exception: ", value);
        throw Exception(alt.loc, std::move(value));
      }
      case index_return: {
        const auto &alt = this->m_stor.as<S_return>();
        // Evaluate the expression.
        ref_out = alt.expr.evaluate(global, ctx_io);
        // If `by_ref` is `false`, replace it with a temporary value.
        if(!alt.by_ref) {
          ref_out.convert_to_temporary();
        }
        return Block::status_return;
      }
      default: {
        ASTERIA_TERMINATE("An unknown statement type enumeration `", this->m_stor.index(), "` has been encountered.");
      }
    }
  }

void Statement::enumerate_variables(const Abstract_variable_callback &callback) const
  {
    switch(Index(this->m_stor.index())) {
      case index_expr: {
        const auto &alt = this->m_stor.as<S_expr>();
        alt.expr.enumerate_variables(callback);
        return;
      }
      case index_block: {
        const auto &alt = this->m_stor.as<S_block>();
        alt.body.enumerate_variables(callback);
        return;
      }
      case index_var_def: {
        const auto &alt = this->m_stor.as<S_var_def>();
        alt.init.enumerate_variables(callback);
        return;
      }
      case index_func_def: {
        const auto &alt = this->m_stor.as<S_func_def>();
        alt.body.enumerate_variables(callback);
        return;
      }
      case index_if: {
        const auto &alt = this->m_stor.as<S_if>();
        alt.cond.enumerate_variables(callback);
        alt.branch_true.enumerate_variables(callback);
        alt.branch_false.enumerate_variables(callback);
        return;
      }
      case index_switch: {
        const auto &alt = this->m_stor.as<S_switch>();
        alt.ctrl.enumerate_variables(callback);
        for(const auto &pair : alt.clauses) {
          pair.first.enumerate_variables(callback);
          pair.second.enumerate_variables(callback);
        }
        return;
      }
      case index_do_while: {
        const auto &alt = this->m_stor.as<S_do_while>();
        alt.body.enumerate_variables(callback);
        alt.cond.enumerate_variables(callback);
        return;
      }
      case index_while: {
        const auto &alt = this->m_stor.as<S_while>();
        alt.cond.enumerate_variables(callback);
        alt.body.enumerate_variables(callback);
        return;
      }
      case index_for: {
        const auto &alt = this->m_stor.as<S_for>();
        alt.init.enumerate_variables(callback);
        alt.cond.enumerate_variables(callback);
        alt.step.enumerate_variables(callback);
        alt.body.enumerate_variables(callback);
        return;
      }
      case index_for_each: {
        const auto &alt = this->m_stor.as<S_for_each>();
        alt.init.enumerate_variables(callback);
        alt.body.enumerate_variables(callback);
        return;
      }
      case index_try: {
        const auto &alt = this->m_stor.as<S_try>();
        alt.body_try.enumerate_variables(callback);
        alt.body_catch.enumerate_variables(callback);
        return;
      }
      case index_break:
      case index_continue: {
        return;
      }
      case index_throw: {
        const auto &alt = this->m_stor.as<S_throw>();
        alt.expr.enumerate_variables(callback);
        return;
      }
      case index_return: {
        const auto &alt = this->m_stor.as<S_return>();
        alt.expr.enumerate_variables(callback);
        return;
      }
      default: {
        ASTERIA_TERMINATE("An unknown statement type enumeration `", this->m_stor.index(), "` has been encountered.");
      }
    }
  }

}
