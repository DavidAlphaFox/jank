#pragma once

#include <jank/parse/cell/cell.hpp>
#include <jank/translate/cell/cell.hpp>
#include <jank/translate/environment/scope.hpp>
#include <jank/translate/environment/special/all.hpp>

namespace jank
{
  namespace translate
  {
    template <typename Range>
    cell::function_body translate(Range const &range, std::shared_ptr<environment::scope> const &scope)
    {
      if(!std::distance(range.begin(), range.end()))
      { return { { {}, {} } }; }

      cell::function_body translated{ { {}, scope } };
      std::for_each
      (
        range.begin(), range.end(),
        [&](auto const &c)
        {
          /* Handle specials. */
          if(expect::is<parse::cell::type::list>(c))
          {
            auto const &list(expect::type<parse::cell::type::list>(c));

            auto const special_opt
            (
              environment::special::handle(list, translated)
            );
            if(special_opt)
            { translated.data.cells.push_back(special_opt.value()); }

            if(list.empty())
            { throw expect::error::syntax::syntax<>{ "invalid empty list" }; }

            auto const function_opt
            (
              scope->find_function
              (expect::type<parse::cell::type::ident>(list[0]))
            );
            if(function_opt)
            {
              /* TODO: Handle function calls. */
            }
          }

          /* TODO: Handle plain values (only useful at the end of a function?) */
        }
      );
      return translated;
    }
  }
}
