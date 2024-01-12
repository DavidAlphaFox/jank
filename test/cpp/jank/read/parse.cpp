#include <unistd.h>

#include <array>
#include <iostream>

#include <jank/read/lex.hpp>
#include <jank/read/parse.hpp>
#include <jank/runtime/seq.hpp>
#include <jank/runtime/obj/number.hpp>
#include <jank/runtime/obj/symbol.hpp>
#include <jank/runtime/obj/keyword.hpp>
#include <jank/runtime/obj/vector.hpp>
#include <jank/runtime/obj/persistent_array_map.hpp>
#include <jank/runtime/obj/string.hpp>
#include <jank/runtime/obj/list.hpp>
#include <jank/runtime/detail/object_util.hpp>

/* This must go last; doctest and glog both define CHECK and family. */
#include <doctest/doctest.h>

namespace jank::read::parse
{
  TEST_SUITE("parse")
  {
    TEST_CASE("Empty")
    {
      runtime::context rt_ctx;
      lex::processor lp{ "" };
      processor p{ rt_ctx, lp.begin(), lp.end() };
      auto const r(p.next());
      CHECK(r.is_ok());
      CHECK(r.expect_ok() == nullptr);
    }

    TEST_CASE("Nil")
    {
      lex::processor lp{ "nil" };
      runtime::context rt_ctx;
      processor p{ rt_ctx, lp.begin(), lp.end() };
      auto const r(p.next());
      CHECK(r.is_ok());
      CHECK(runtime::detail::equal(r.expect_ok(), make_box(nullptr)));
    }

    TEST_CASE("Boolean")
    {
      lex::processor lp{ "true false" };
      runtime::context rt_ctx;
      processor p{ rt_ctx, lp.begin(), lp.end() };
      auto const t(p.next());
      CHECK(t.is_ok());
      CHECK(runtime::detail::equal(t.expect_ok(), make_box(true)));
      auto const f(p.next());
      CHECK(runtime::detail::equal(f.expect_ok(), make_box(false)));
    }

    TEST_CASE("Integer")
    {
      lex::processor lp{ "1234" };
      runtime::context rt_ctx;
      processor p{ rt_ctx, lp.begin(), lp.end() };
      auto const r(p.next());
      CHECK(r.is_ok());
      CHECK(runtime::detail::equal(r.expect_ok(), make_box(1234)));
    }

    TEST_CASE("Comments")
    {
      lex::processor lp{ ";meow \n1234 ; bar\n;\n\n" };
      runtime::context rt_ctx;
      processor p{ rt_ctx, lp.begin(), lp.end() };
      auto const i(p.next());
      CHECK(i.is_ok());
      CHECK(runtime::detail::equal(i.expect_ok(), make_box(1234)));

      auto const eof(p.next());
      CHECK(eof.is_ok());
      CHECK(eof.expect_ok() == nullptr);
    }

    TEST_CASE("Real")
    {
      lex::processor lp{ "12.34" };
      runtime::context rt_ctx;
      processor p{ rt_ctx, lp.begin(), lp.end() };
      auto const r(p.next());
      CHECK(r.is_ok());
      CHECK(runtime::detail::equal(r.expect_ok(), make_box(12.34l)));
    }

    TEST_CASE("String")
    {
      SUBCASE("Unescaped")
      {
        lex::processor lp{R"("foo" "bar")"};
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(auto const &s : { "foo", "bar" })
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(runtime::detail::equal(r.expect_ok(), make_box(s)));
        }
      }

      SUBCASE("Escaped")
      {
        lex::processor lp{R"("foo\n" "\t\"bar\"")"};
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(auto const &s : { "foo\n", "\t\"bar\"" })
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(runtime::detail::equal(r.expect_ok(), make_box(s)));
        }
      }
    }

    TEST_CASE("Symbol")
    {
      SUBCASE("Unqualified")
      {
        lex::processor lp{ "foo bar spam" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(auto const &s : { "foo", "bar", "spam" })
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(runtime::detail::equal(r.expect_ok(), make_box<runtime::obj::symbol>("", s)));
        }
      }

      SUBCASE("Slash")
      {
        lex::processor lp{ "/" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r(p.next());
        CHECK(r.is_ok());
        CHECK(runtime::detail::equal(r.expect_ok(), make_box<runtime::obj::symbol>("", "/")));
      }

      SUBCASE("Qualified")
      {
        lex::processor lp{ "foo/foo foo.bar/bar spam.bar/spam" };
        runtime::context rt_ctx;
        rt_ctx.intern_ns(make_box<runtime::obj::symbol>("foo"));
        rt_ctx.intern_ns(make_box<runtime::obj::symbol>("foo.bar"));
        rt_ctx.intern_ns(make_box<runtime::obj::symbol>("spam.bar"));
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(auto const &s : { std::make_pair("foo", "foo"), std::make_pair("foo.bar", "bar"), std::make_pair("spam.bar", "spam") })
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(runtime::detail::equal(r.expect_ok(), make_box<runtime::obj::symbol>(s.first, s.second)));
        }
      }

      SUBCASE("Qualified, aliased")
      {
        lex::processor lp{ "foo.bar/bar" };
        runtime::context rt_ctx;
        auto const meow(rt_ctx.intern_ns(make_box<runtime::obj::symbol>("meow")));
        rt_ctx.current_ns()->add_alias(make_box<runtime::obj::symbol>("foo.bar"), meow).expect_ok();
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(auto const &s : { std::make_pair("meow", "bar") })
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(runtime::detail::equal(r.expect_ok(), make_box<runtime::obj::symbol>(s.first, s.second)));
        }
      }

      SUBCASE("Qualified, non-existent ns")
      {
        lex::processor lp{ "foo.bar/bar" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r(p.next());
        CHECK(r.is_err());
      }

      SUBCASE("Quoted")
      {
        lex::processor lp{ "'foo 'bar/spam 'foo.bar/bar" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(auto const &s : { std::make_pair("", "foo"), std::make_pair("bar", "spam"), std::make_pair("foo.bar", "bar") })
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK
          (
            runtime::detail::equal
            (
              r.expect_ok(),
              make_box<runtime::obj::list>
              (
                make_box<runtime::obj::symbol>("quote"),
                make_box<runtime::obj::symbol>(s.first, s.second)
              )
            )
          );
        }
      }
    }

    TEST_CASE("Keyword")
    {
      SUBCASE("Unqualified")
      {
        lex::processor lp{ ":foo :bar :spam" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(auto const &s : { "foo", "bar", "spam" })
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(runtime::detail::equal(r.expect_ok(), rt_ctx.intern_keyword(runtime::obj::symbol{ "", s }, true).expect_ok()));
        }
      }

      SUBCASE("Qualified")
      {
        lex::processor lp{ ":foo/foo :foo.bar/bar :spam.bar/spam" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(auto const &s : { std::make_pair("foo", "foo"), std::make_pair("foo.bar", "bar"), std::make_pair("spam.bar", "spam") })
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(runtime::detail::equal(r.expect_ok(), rt_ctx.intern_keyword(runtime::obj::symbol{ s.first, s.second }, true).expect_ok()));
        }
      }

      SUBCASE("Auto-resolved unqualified")
      {
        lex::processor lp{ "::foo ::spam" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(auto const &s : { "foo", "spam" })
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(runtime::detail::equal(r.expect_ok(), rt_ctx.intern_keyword(runtime::obj::symbol{ "", native_persistent_string{ s } }, false).expect_ok()));
        }
      }

      SUBCASE("Auto-resolved qualified, missing alias")
      {
        lex::processor lp{ "::foo/foo" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r(p.next());
        CHECK(r.is_err());
      }

      SUBCASE("Auto-resolved qualified, with alias")
      {
        lex::processor lp{ "::foo/foo" };
        runtime::context rt_ctx;
        auto const foo_ns(rt_ctx.intern_ns(make_box<runtime::obj::symbol>("foo.bar.spam")));
        auto const clojure_ns(rt_ctx.find_ns(make_box<runtime::obj::symbol>("clojure.core")));
        clojure_ns.unwrap()->add_alias(make_box<runtime::obj::symbol>("foo"), foo_ns).expect_ok();
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r(p.next());
        CHECK(r.is_ok());
        CHECK(runtime::detail::equal(r.expect_ok(), rt_ctx.intern_keyword(runtime::obj::symbol{ "foo.bar.spam", "foo" }, true).expect_ok()));
      }
    }

    TEST_CASE("List")
    {
      SUBCASE("Empty")
      {
        lex::processor lp{ "() ( ) (   )" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(size_t i{}; i < 3; ++i)
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(r.expect_ok() != nullptr);
          CHECK(runtime::detail::equal(r.expect_ok(), make_box<runtime::obj::list>()));
        }
      }

      SUBCASE("Non-empty")
      {
        lex::processor lp{ "(1 2 3 4) ( 2, 4 6, 8 )" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(size_t i{ 1 }; i < 3; ++i)
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK
          (
            runtime::detail::equal
            (
              r.expect_ok(),
              make_box<runtime::obj::list>
              (
                make_box<runtime::obj::integer>(1 * i),
                make_box<runtime::obj::integer>(2 * i),
                make_box<runtime::obj::integer>(3 * i),
                make_box<runtime::obj::integer>(4 * i)
              )
            )
          );
        }
      }

      SUBCASE("Mixed")
      {
        lex::processor lp{ "(def foo-bar 1) foo-bar" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r1(p.next());
        CHECK(r1.is_ok());
        CHECK
        (
          runtime::detail::equal
          (
            r1.expect_ok(),
            make_box<runtime::obj::list>
            (
              make_box<runtime::obj::symbol>("def"),
              make_box<runtime::obj::symbol>("foo-bar"),
              make_box<runtime::obj::integer>(1)
            )
          )
        );
        auto const r2(p.next());
        CHECK(r2.is_ok());
        CHECK(runtime::detail::equal(r2.expect_ok(), make_box<runtime::obj::symbol>("foo-bar")));
      }

      SUBCASE("Extra close")
      {
        lex::processor lp{ "1)" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r1(p.next());
        CHECK(r1.is_ok());
        CHECK(runtime::detail::equal(r1.expect_ok(), make_box(1)));
        auto const r2(p.next());
        CHECK(r2.is_err());
      }

      SUBCASE("Unterminated")
      {
        lex::processor lp{ "(1" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r1(p.next());
        CHECK(r1.is_err());
      }
    }

    TEST_CASE("Vector")
    {
      SUBCASE("Empty")
      {
        lex::processor lp{ "[] [ ] [   ]" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(size_t i{}; i < 3; ++i)
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(r.expect_ok() != nullptr);
          CHECK(runtime::detail::equal(r.expect_ok(), make_box<runtime::obj::vector>()));
        }
      }

      SUBCASE("Non-empty")
      {
        lex::processor lp{ "[1 2 3 4] [ 2, 4 6, 8 ]" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(size_t i{ 1 }; i < 3; ++i)
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK
          (
            runtime::detail::equal
            (
              r.expect_ok(),
              make_box<runtime::obj::vector>
              (
                runtime::detail::native_persistent_vector
                {
                  make_box<runtime::obj::integer>(1 * i),
                  make_box<runtime::obj::integer>(2 * i),
                  make_box<runtime::obj::integer>(3 * i),
                  make_box<runtime::obj::integer>(4 * i),
                }
              )
            )
          );
        }
      }

      SUBCASE("Extra close")
      {
        lex::processor lp{ "1]" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r1(p.next());
        CHECK(r1.is_ok());
        CHECK(runtime::detail::equal(r1.expect_ok(), make_box(1)));
        auto const r2(p.next());
        CHECK(r2.is_err());
      }

      SUBCASE("Unterminated")
      {
        lex::processor lp{ "[1" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r1(p.next());
        CHECK(r1.is_err());
      }
    }

    TEST_CASE("Map")
    {
      SUBCASE("Empty")
      {
        lex::processor lp{ "{} { } {,,,}" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(size_t i{}; i < 3; ++i)
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK(r.expect_ok() != nullptr);
          CHECK(runtime::detail::equal(r.expect_ok(), make_box<runtime::obj::persistent_array_map>()));
        }
      }

      SUBCASE("Non-empty")
      {
        lex::processor lp{ "{1 2 3 4} { 2, 4 6, 8 }" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        for(size_t i{ 1 }; i < 3; ++i)
        {
          auto const r(p.next());
          CHECK(r.is_ok());
          CHECK
          (
            runtime::detail::equal
            (
              r.expect_ok(),
              make_box<runtime::obj::persistent_array_map>
              (
                runtime::detail::in_place_unique{},
                make_array_box<runtime::object_ptr>
                (
                  make_box<runtime::obj::integer>(1 * i),
                  make_box<runtime::obj::integer>(2 * i),
                  make_box<runtime::obj::integer>(3 * i),
                  make_box<runtime::obj::integer>(4 * i)
                ),
                4
              )
            )
          );
        }
      }

      SUBCASE("Heterogeneous")
      {
        lex::processor lp{R"({:foo true 1 :one "meow" "meow"})"};
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r(p.next());
        CHECK(r.is_ok());
        CHECK(r.expect_ok() != nullptr);
        CHECK
        (
          runtime::detail::equal
          (
            r.expect_ok(),
            make_box<runtime::obj::persistent_array_map>
            (
              runtime::detail::in_place_unique{},
              make_array_box<runtime::object_ptr>
              (
                rt_ctx.intern_keyword(runtime::obj::symbol{ "foo" }, true).expect_ok(),
                make_box<runtime::obj::boolean>(true),
                make_box<runtime::obj::integer>(1),
                rt_ctx.intern_keyword(runtime::obj::symbol{ "one" }, true).expect_ok(),
                make_box<runtime::obj::string>("meow"),
                make_box<runtime::obj::string>("meow")
              ),
              6
            )
          )
        );
      }

      SUBCASE("Odd elements")
      {
        lex::processor lp{ "{1 2 3}" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r1(p.next());
        CHECK(r1.is_err());
      }

      SUBCASE("Extra close")
      {
        lex::processor lp{ ":foo}" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r1(p.next());
        CHECK(r1.is_ok());
        CHECK(runtime::detail::equal(r1.expect_ok(), rt_ctx.intern_keyword(runtime::obj::symbol{ "foo" }, true).expect_ok()));
        auto const r2(p.next());
        CHECK(r2.is_err());
      }

      SUBCASE("Unterminated")
      {
        lex::processor lp{ "{1" };
        runtime::context rt_ctx;
        processor p{ rt_ctx, lp.begin(), lp.end() };
        auto const r1(p.next());
        CHECK(r1.is_err());
      }
    }
  }
}
