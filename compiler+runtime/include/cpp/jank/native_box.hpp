#pragma once

#include <fmt/ostream.h>

#include <jank/runtime/object.hpp>

namespace jank
{
  template <typename T>
  struct native_box
  {
    using value_type = T;

    native_box() = default;

    native_box(std::nullptr_t)
    {
    }

    native_box(std::remove_const_t<value_type> * const data)
      : data{ data }
    {
    }

    native_box(value_type const * const data)
      : data{ const_cast<value_type *>(data) }
    {
    }

    value_type *operator->() const
    {
      assert(data);
      return data;
    }

    native_bool operator!() const
    {
      return !data;
    }

    value_type &operator*() const
    {
      assert(data);
      return *data;
    }

    native_bool operator==(std::nullptr_t) const
    {
      return data == nullptr;
    }

    native_bool operator==(native_box const &rhs) const
    {
      return data == rhs.data;
    }

    native_bool operator!=(std::nullptr_t) const
    {
      return data != nullptr;
    }

    native_bool operator!=(native_box const &rhs) const
    {
      return data != rhs.data;
    }

    native_bool operator<(native_box const &rhs) const
    {
      return data < rhs.data;
    }

    operator native_box<value_type const>() const
    {
      return data;
    }

    operator value_type *() const
    {
      return data;
    }

    operator runtime::object *() const
    {
      return &data->base;
    }

    explicit operator native_bool() const
    {
      return data;
    }

    value_type *data{};
  };

  template <>
  struct native_box<runtime::object>
  {
    using value_type = runtime::object;

    native_box() = default;

    native_box(std::nullptr_t)
    {
    }

    native_box(value_type * const data)
      : data{ data }
    {
    }

    template <runtime::object_type T>
    native_box(runtime::static_object<T> * const typed_data)
      : data{ &typed_data->base }
    {
    }

    template <runtime::object_type T>
    native_box(runtime::static_object<T> const * const typed_data)
      : data{ typed_data ? const_cast<runtime::object *>(&typed_data->base) : nullptr }
    {
    }

    template <runtime::object_type T>
    native_box(native_box<runtime::static_object<T>> const typed_data)
      : data{ typed_data ? &typed_data->base : nullptr }
    {
    }

    value_type *operator->() const
    {
      assert(data);
      return data;
    }

    native_bool operator!() const
    {
      return !data;
    }

    value_type &operator*() const
    {
      assert(data);
      return *data;
    }

    native_bool operator==(std::nullptr_t) const
    {
      return data == nullptr;
    }

    native_bool operator==(native_box const &rhs) const
    {
      return data == rhs.data;
    }

    template <runtime::object_type T>
    native_bool operator==(runtime::static_object<T> const &rhs) const
    {
      return data == &rhs->base;
    }

    template <runtime::object_type T>
    native_bool operator==(native_box<runtime::static_object<T>> const &rhs) const
    {
      return data == &rhs->base;
    }

    native_bool operator!=(std::nullptr_t) const
    {
      return data != nullptr;
    }

    native_bool operator!=(native_box const &rhs) const
    {
      return data != rhs.data;
    }

    template <runtime::object_type T>
    native_bool operator!=(runtime::static_object<T> const &rhs) const
    {
      return data != &rhs->base;
    }

    template <runtime::object_type T>
    native_bool operator!=(native_box<runtime::static_object<T>> const &rhs) const
    {
      return data != &rhs->base;
    }

    native_bool operator<(native_box const &rhs) const
    {
      return data < rhs.data;
    }

    operator native_box<value_type const>() const
    {
      return data;
    }

    operator value_type *() const
    {
      return data;
    }

    explicit operator native_bool() const
    {
      return data;
    }

    value_type *data{};
  };

  template <typename T>
  struct remove_box
  {
    using type = T;
  };

  template <typename T>
  struct remove_box<native_box<T>>
  {
    using type = T;
  };

  template <typename T>
  using remove_box_t = typename remove_box<T>::type;

  /* TODO: Constexpr these. */

  template <typename T>
  constexpr native_box<T> make_box(native_box<T> const &o)
  {
    return o;
  }

  template <typename T, typename... Args>
  native_box<T> make_box(Args &&...args)
  {
    native_box<T> ret;
    if constexpr(T::pointer_free)
    {
      ret = new(PointerFreeGC) T{ std::forward<Args>(args)... };
    }
    else
    {
      ret = new(GC) T{ std::forward<Args>(args)... };
    }

    if(!ret)
    {
      throw std::runtime_error{ "unable to allocate box" };
    }
    return ret;
  }

  template <typename T>
  constexpr native_box<T> make_array_box()
  {
    return nullptr;
  }

  template <typename T, size_t N>
  constexpr native_box<T> make_array_box()
  {
    auto const ret(new(GC) T[N]{});
    if(!ret)
    {
      throw std::runtime_error{ "unable to allocate array box" };
    }
    return ret;
  }

  template <typename T>
  constexpr native_box<T> make_array_box(size_t const length)
  {
    auto const ret(new(GC) T[length]{});
    if(!ret)
    {
      throw std::runtime_error{ "unable to allocate array box" };
    }
    return ret;
  }

  template <typename T, typename... Args>
  native_box<T> make_array_box(Args &&...args)
  {
    /* TODO: pointer_free? */
    auto const ret(new(GC) T[sizeof...(Args)]{ std::forward<Args>(args)... });
    if(!ret)
    {
      throw std::runtime_error{ "unable to allocate array box" };
    }
    return ret;
  }

  template <typename T>
  std::ostream &operator<<(std::ostream &os, native_box<T> const &o)
  {
    return os << "box(" << o.data << ")";
  }
}

namespace fmt
{
  template <typename T>
  struct formatter<jank::native_box<T>> : fmt::ostream_formatter
  {
  };
}
