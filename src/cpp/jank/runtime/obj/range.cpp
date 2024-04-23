#include <jank/runtime/obj/range.hpp>
#include <jank/runtime/seq.hpp>
#include <jank/runtime/obj/number.hpp>
#include <jank/runtime/math.hpp>

namespace jank::runtime
{
  obj::range::static_object(object_ptr const end)
    : start{ make_box(0) }
    , end{ end }
    , step{ make_box(1) }
  {
  }

  obj::range::static_object(object_ptr const start, object_ptr const end)
    : start{ start }
    , end{ end }
    , step{ make_box(1) }
  {
  }

  obj::range::static_object(object_ptr const start, object_ptr const end, object_ptr const step)
    : start{ start }
    , end{ end }
    , step{ step }
  {
  }

  obj::range_ptr obj::range::seq()
  {
    return this;
  }

  obj::range_ptr obj::range::fresh_seq() const
  {
    return make_box<obj::range>(start, end, step);
  }

  object_ptr obj::range::first() const
  {
    return start;
  }

  obj::range_ptr obj::range::next() const
  {
    if(cached_next)
    {
      return cached_next;
    }

    auto next_start(add(start, step));
    if(!lt(next_start, end))
    {
      return nullptr;
    }

    auto const ret(make_box<obj::range>(next_start, end, step));
    cached_next = ret;
    return ret;
  }

  obj::range_ptr obj::range::next_in_place()
  {
    auto next_start(add(start, step));
    if(!lt(next_start, end))
    {
      return nullptr;
    }

    start = next_start;

    return this;
  }

  obj::cons_ptr obj::range::conj(object_ptr head) const
  {
    return make_box<obj::cons>(head, this);
  }

  native_bool obj::range::equal(object const &o) const
  {
    return visit_object(
      [this](auto const typed_o) {
        using T = typename decltype(typed_o)::value_type;

        if constexpr(!behavior::seqable<T>)
        {
          return false;
        }
        else
        {
          auto seq(typed_o->fresh_seq());
          /* TODO: This is common code; can it be shared? */
          for(auto it(fresh_seq()); it != nullptr;
              it = runtime::next_in_place(it), seq = runtime::next_in_place(seq))
          {
            if(seq == nullptr || !runtime::detail::equal(it, seq->first()))
            {
              return false;
            }
          }
          return true;
        }
      },
      &o);
  }

  void obj::range::to_string(fmt::memory_buffer &buff)
  {
    runtime::detail::to_string(seq(), buff);
  }

  native_persistent_string obj::range::to_string()
  {
    return runtime::detail::to_string(seq());
  }

  native_hash obj::range::to_hash() const
  {
    return hash::ordered(&base);
  }
}
