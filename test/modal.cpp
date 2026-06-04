#include "libkw.hpp"

#include <cstdlib>
#include <iostream>

#if defined(TEST_NOIPA)
#define TEST_ATTR [[gnu::noipa]]
#endif

#if defined(TEST_NOINLINE)
#define TEST_ATTR [[gnu::noinline]]
#endif

#if defined(TEST_ALWAYS_INLINE)
#define TEST_ATTR [[gnu::always_inline]] inline
#endif

#if defined(TEST_EMPTY_ATTR)
#define TEST_ATTR
#endif

struct Foo {
  TEST_ATTR ~Foo() noexcept { std::cout << __PRETTY_FUNCTION__ << std::endl; }

  TEST_ATTR Foo() { std::cout << __PRETTY_FUNCTION__ << std::endl; }

  TEST_ATTR Foo(const Foo&) { std::cout << __PRETTY_FUNCTION__ << std::endl; }

  TEST_ATTR Foo(Foo&&) noexcept { std::cout << __PRETTY_FUNCTION__ << std::endl; }

  TEST_ATTR Foo& operator=(const Foo&)
  {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    return *this;
  }

  TEST_ATTR Foo& operator=(Foo&&) noexcept
  {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    return *this;
  }
};

TEST_ATTR int foo(const Foo& const_ref, Foo& lval_ref, Foo&& rval_ref, Foo by_copied_val, Foo by_moved_val)
{
  (void)const_ref;
  (void)lval_ref;
  (void)rval_ref;
  (void)by_copied_val;
  (void)by_moved_val;
  return 0;
}

TEST_ATTR int test()
{
  using libkw::kw;

  const auto const_ref = Foo();
  auto lval_ref = Foo();
  auto rval_ref = Foo();
  auto by_copied_val = Foo();
  auto by_moved_val = Foo();

#if defined(TEST_POSITIONAL_CALL)
  return foo(const_ref, lval_ref, std::move(rval_ref), by_copied_val, std::move(by_moved_val));
#endif

#if defined(TEST_KW_CALL_IN_ORDER)
  return KW_CALL(foo,
      kw<"const_ref">,
      const_ref,
      kw<"lval_ref">,
      lval_ref,
      kw<"rval_ref">,
      std::move(rval_ref),
      kw<"by_copied_val">,
      by_copied_val,
      kw<"by_moved_val">,
      std::move(by_moved_val));
#endif

#if defined(TEST_KW_CALL_OUT_OF_ORDER)
  return KW_CALL(foo,
      kw<"by_copied_val">,
      by_copied_val,
      kw<"const_ref">,
      const_ref,
      kw<"by_moved_val">,
      std::move(by_moved_val),
      kw<"rval_ref">,
      std::move(rval_ref),
      kw<"lval_ref">,
      lval_ref);
#endif
}
