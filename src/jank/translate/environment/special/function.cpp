#include <stdexcept>
#include <memory>

#include <jank/parse/expect/type.hpp>
#include <jank/translate/translate.hpp>
#include <jank/translate/function/argument/definition.hpp>
#include <jank/translate/function/return/parse.hpp>
#include <jank/translate/function/return/deduce.hpp>
#include <jank/translate/environment/scope.hpp>
#include <jank/translate/environment/special/function.hpp>
#include <jank/translate/environment/builtin/type/primitive.hpp>
#include <jank/translate/expect/error/syntax/exception.hpp>
#include <jank/translate/expect/error/type/overload.hpp>

namespace jank
{
  namespace translate
  {
    namespace environment
    {
      namespace special
      {
        cell::cell function
        (
          parse::cell::list const &input,
          std::shared_ptr<scope> const &outer_scope
        )
        {
          static std::size_t constexpr forms_required{ 4 };

          auto &data(input.data);
          if(data.size() < forms_required)
          {
            throw expect::error::syntax::exception<>
            { "invalid function definition" };
          }

          auto const name
          (parse::expect::type<parse::cell::type::ident>(data[1]));
          auto const args
          (parse::expect::type<parse::cell::type::list>(data[2]));
          auto const nested_scope
          (std::make_shared<scope>(outer_scope));
          auto const arg_definitions
          (function::argument::definition::parse_types(args, nested_scope));

          /* Add args to function's scope. */
          std::transform
          (
            arg_definitions.begin(), arg_definitions.end(),
            std::inserter
            (
              nested_scope->binding_definitions,
              nested_scope->binding_definitions.end()
            ),
            [](auto const &arg)
            {
              return std::make_pair
              (
                arg.name,
                cell::binding_definition
                { {
                  arg.name, arg.type, {}
                } }
              );
            }
          );

          /* TODO: Check native functions, too. */
          /* Check for an already-defined function of this type. */
          /* XXX: We're only checking *this* scope's functions, so
           * shadowing is allowed. */
          for
          (
            auto const &overload :
            outer_scope->function_definitions[name.data]
          )
          {
            if(overload.data.arguments == arg_definitions)
            {
              throw expect::error::type::overload
              { "multiple definition of: " + name.data };
            }
          }

          /* TODO: Add multiple return types into a tuple. */
          /* Parse return types. */
          auto const return_type_names
          (parse::expect::type<parse::cell::type::list>(data[3]));
          auto const return_types
          (function::ret::parse(return_type_names, nested_scope));
          auto const return_type(return_types[0].data);

          /* TODO: Recursion with auto return types? */
          /* Add an empty declaration first, to allow for recursive references. */
          auto &decls(outer_scope->function_definitions[name.data]);
          decls.emplace_back();
          auto &decl(decls.back());
          decl.data.name = name.data;
          decl.data.return_type = return_type;
          decl.data.arguments = arg_definitions;

          cell::function_definition ret
          {
            {
              name.data,
              arg_definitions,
              return_type,
              translate /* Recurse into translate for the body. */
              (
                jtl::it::make_range
                (
                  std::next
                  (
                    data.begin(),
                    forms_required
                  ),
                  data.end()
                ),
                nested_scope,
                { return_type }
              ).data,
              nested_scope
            }
          };

          /* Verify all paths return a value. */
          ret.data.body = function::ret::deduce
          (function::ret::validate(std::move(ret.data.body)));
          ret.data.return_type = ret.data.body.return_type;

          /* Add the function definition to the outer body's scope. */
          outer_scope->function_definitions[name.data].back() = ret;
          return { ret };
        }
      }
    }
  }
}
