#include "libkw.hpp"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>
#include <tuple>

#define CHECK(actual_expr, expected_expr) \
  do { \
    auto path = std::filesystem::path(__FILE__); \
    auto actual_val = actual_expr; \
    auto expected_val = expected_expr; \
    if ((actual_val) != (expected_val)) { \
      std::cout << std::format("[{}:{}] FAILED: (({} == {}) != {}))", \
                       path.filename(), \
                       __LINE__, \
                       #actual_expr, \
                       actual_val, \
                       #expected_expr) \
                << std::endl; \
      std::exit(1); \
    } \
    std::cout << std::format("[{}:{}] PASSED: {} == {}", path.filename(), __LINE__, #actual_expr, #expected_expr) \
              << std::endl; \
  } while (false)

struct NoCopyNoMove {
  ~NoCopyNoMove() noexcept = default;
  NoCopyNoMove(int in_value) : value(in_value) {}

  NoCopyNoMove(const NoCopyNoMove&) = delete;
  NoCopyNoMove(NoCopyNoMove&&) noexcept = delete;

  NoCopyNoMove& operator=(const NoCopyNoMove&) = delete;
  NoCopyNoMove& operator=(NoCopyNoMove&&) noexcept = delete;

  int value;
};

struct NoCopyYesMove {
  ~NoCopyYesMove() noexcept = default;
  NoCopyYesMove(int in_value) : value(in_value) {}

  NoCopyYesMove(const NoCopyYesMove&) = delete;
  NoCopyYesMove(NoCopyYesMove&&) noexcept = default;

  NoCopyYesMove& operator=(const NoCopyYesMove&) = delete;
  NoCopyYesMove& operator=(NoCopyYesMove&&) noexcept = default;

  int value;
};

struct YesCopyNoMove {
  ~YesCopyNoMove() noexcept = default;
  YesCopyNoMove(int in_value) : value(in_value) {}

  YesCopyNoMove(const YesCopyNoMove&) = default;
  YesCopyNoMove(YesCopyNoMove&&) noexcept = delete;

  YesCopyNoMove& operator=(const YesCopyNoMove&) = default;
  YesCopyNoMove& operator=(YesCopyNoMove&&) noexcept = delete;

  int value;
};

class DetectCopyAndMove {
public:
  struct Stats {
    size_t copy = 0;
    size_t move = 0;
  };

  ~DetectCopyAndMove() noexcept {}

  DetectCopyAndMove(Stats& stats) : stats_(&stats) {}

  DetectCopyAndMove(const DetectCopyAndMove& other) : stats_(other.stats_) { stats_->copy += 1; }

  DetectCopyAndMove(DetectCopyAndMove&& other) noexcept : stats_(other.stats_) { stats_->move += 1; }

  DetectCopyAndMove& operator=(const DetectCopyAndMove& other)
  {
    (void)other;
    stats_->copy += 1;
    return *this;
  }

  DetectCopyAndMove& operator=(DetectCopyAndMove&& other) noexcept
  {
    (void)other;
    stats_->move += 1;
    return *this;
  }

private:
  Stats* stats_;
};

[[nodiscard]] auto free0()
{
  return std::make_tuple();
}

[[nodiscard]] auto free1(int x)
{
  return std::make_tuple(x);
}

[[nodiscard]] auto free2(int x, int y)
{
  return std::make_tuple(x, y);
}

[[nodiscard]] auto free3(int x, int y, int z)
{
  return std::make_tuple(x, y, z);
}

[[nodiscard]] auto free_no_copy_no_move(const NoCopyNoMove& const_ref, NoCopyNoMove& lval_ref, NoCopyNoMove&& rval_ref)
{
  lval_ref.value += 1;
  return std::make_tuple(const_ref.value, lval_ref.value, rval_ref.value);
}

[[nodiscard]] auto free_no_copy_yes_move(
    const NoCopyYesMove& const_ref, NoCopyYesMove& lval_ref, NoCopyYesMove&& rval_ref, NoCopyYesMove by_moved_val)
{
  lval_ref.value += 1;
  return std::make_tuple(const_ref.value, lval_ref.value, rval_ref.value, by_moved_val.value);
}

[[nodiscard]] auto free_yes_copy_no_move(
    const YesCopyNoMove& const_ref, YesCopyNoMove& lval_ref, YesCopyNoMove&& rval_ref, YesCopyNoMove by_copied_val)
{
  lval_ref.value += 1;
  return std::make_tuple(const_ref.value, lval_ref.value, rval_ref.value, by_copied_val.value);
}

void free_detect_copy_and_move(const DetectCopyAndMove& const_ref, DetectCopyAndMove& lval_ref,
    DetectCopyAndMove&& rval_ref, DetectCopyAndMove by_copied_val, DetectCopyAndMove by_moved_val)
{
  (void)const_ref;
  (void)lval_ref;
  (void)rval_ref;
  (void)by_copied_val;
  (void)by_moved_val;
}

struct Class {
  [[nodiscard]] static auto static0() { return std::make_tuple(); }

  [[nodiscard]] static auto static1(int x) { return std::make_tuple(x); }

  [[nodiscard]] static auto static2(int x, int y) { return std::make_tuple(x, y); }

  [[nodiscard]] static auto static3(int x, int y, int z) { return std::make_tuple(x, y, z); }

  [[nodiscard]] auto method0() { return std::make_tuple(); }

  [[nodiscard]] auto method1(int x) { return std::make_tuple(x); }

  [[nodiscard]] auto method2(int x, int y) { return std::make_tuple(x, y); }

  [[nodiscard]] auto method3(int x, int y, int z) { return std::make_tuple(x, y, z); }
};

int main()
{
  using libkw::kw;

  CHECK(KW_CALL(free0), std::make_tuple());
  CHECK(KW_CALL(free1, kw<"x">, 1), std::make_tuple(1));
  CHECK(KW_CALL(free2, kw<"x">, 1, kw<"y">, 2), std::make_tuple(1, 2));
  CHECK(KW_CALL(free3, kw<"x">, 1, kw<"y">, 2, kw<"z">, 3), std::make_tuple(1, 2, 3));

  // out-of-order
  CHECK(KW_CALL(free2, kw<"y">, 2, kw<"x">, 1), std::make_tuple(1, 2));
  CHECK(KW_CALL(free3, kw<"z">, 3, kw<"y">, 2, kw<"x">, 1), std::make_tuple(1, 2, 3));
  CHECK(KW_CALL(free3, kw<"y">, 2, kw<"x">, 1, kw<"z">, 3), std::make_tuple(1, 2, 3));
  CHECK(KW_CALL(free3, kw<"z">, 3, kw<"x">, 1, kw<"y">, 2), std::make_tuple(1, 2, 3));

  CHECK(KW_CALL(Class::static0), std::make_tuple());
  CHECK(KW_CALL(Class::static1, kw<"x">, 1), std::make_tuple(1));
  CHECK(KW_CALL(Class::static2, kw<"x">, 1, kw<"y">, 2), std::make_tuple(1, 2));
  CHECK(KW_CALL(Class::static3, kw<"x">, 1, kw<"y">, 2, kw<"z">, 3), std::make_tuple(1, 2, 3));

  // out-of-order static
  CHECK(KW_CALL(Class::static2, kw<"y">, 2, kw<"x">, 1), std::make_tuple(1, 2));
  CHECK(KW_CALL(Class::static3, kw<"z">, 3, kw<"y">, 2, kw<"x">, 1), std::make_tuple(1, 2, 3));

  CHECK(KW_METHOD_CALL(Class(), method0), std::make_tuple());
  CHECK(KW_METHOD_CALL(Class(), method1, kw<"x">, 1), std::make_tuple(1));
  CHECK(KW_METHOD_CALL(Class(), method2, kw<"x">, 1, kw<"y">, 2), std::make_tuple(1, 2));
  CHECK(KW_METHOD_CALL(Class(), method3, kw<"x">, 1, kw<"y">, 2, kw<"z">, 3), std::make_tuple(1, 2, 3));

  // out-of-order method
  CHECK(KW_METHOD_CALL(Class(), method2, kw<"y">, 2, kw<"x">, 1), std::make_tuple(1, 2));
  CHECK(KW_METHOD_CALL(Class(), method3, kw<"z">, 3, kw<"y">, 2, kw<"x">, 1), std::make_tuple(1, 2, 3));

  {
    const auto const_ref = NoCopyNoMove(11);
    auto lval_ref = NoCopyNoMove(6);
    auto rval_ref = NoCopyNoMove(15);
    CHECK(KW_CALL(free_no_copy_no_move,
              kw<"const_ref">,
              const_ref,
              kw<"lval_ref">,
              lval_ref,
              kw<"rval_ref">,
              std::move(rval_ref)),
        std::make_tuple(11, 7, 15));
    CHECK(lval_ref.value, 7);
  }
  {
    const auto const_ref = NoCopyYesMove(11);
    auto lval_ref = NoCopyYesMove(6);
    auto rval_ref = NoCopyYesMove(15);
    auto by_moved_val = NoCopyYesMove(24);
    CHECK(KW_CALL(free_no_copy_yes_move,
              kw<"const_ref">,
              const_ref,
              kw<"lval_ref">,
              lval_ref,
              kw<"rval_ref">,
              std::move(rval_ref),
              kw<"by_moved_val">,
              std::move(by_moved_val)),
        std::make_tuple(11, 7, 15, 24));
    CHECK(lval_ref.value, 7);
  }
  {
    const auto const_ref = YesCopyNoMove(11);
    auto lval_ref = YesCopyNoMove(6);
    auto rval_ref = YesCopyNoMove(15);
    auto by_copied_val = YesCopyNoMove(24);
    CHECK(KW_CALL(free_yes_copy_no_move,
              kw<"const_ref">,
              const_ref,
              kw<"lval_ref">,
              lval_ref,
              kw<"rval_ref">,
              std::move(rval_ref),
              kw<"by_copied_val">,
              by_copied_val),
        std::make_tuple(11, 7, 15, 24));
    CHECK(lval_ref.value, 7);
  }
  {
    using Stats = DetectCopyAndMove::Stats;

    auto normal_stats_const_ref = Stats();
    const auto normal_const_ref = DetectCopyAndMove(normal_stats_const_ref);

    auto normal_stats_lval_ref = Stats();
    auto normal_lval_ref = DetectCopyAndMove(normal_stats_lval_ref);

    auto normal_stats_rval_ref = Stats();
    auto normal_rval_ref = DetectCopyAndMove(normal_stats_rval_ref);

    auto normal_stats_by_copied_val = Stats();
    auto normal_by_copied_val = DetectCopyAndMove(normal_stats_by_copied_val);

    auto normal_stats_by_moved_val = Stats();
    auto normal_by_moved_val = DetectCopyAndMove(normal_stats_by_moved_val);

    auto named_stats_const_ref = Stats();
    const auto named_const_ref = DetectCopyAndMove(named_stats_const_ref);

    auto named_stats_lval_ref = Stats();
    auto named_lval_ref = DetectCopyAndMove(named_stats_lval_ref);

    auto named_stats_rval_ref = Stats();
    auto named_rval_ref = DetectCopyAndMove(named_stats_rval_ref);

    auto named_stats_by_copied_val = Stats();
    auto named_by_copied_val = DetectCopyAndMove(named_stats_by_copied_val);

    auto named_stats_by_moved_val = Stats();
    auto named_by_moved_val = DetectCopyAndMove(named_stats_by_moved_val);

    free_detect_copy_and_move(normal_const_ref,
        normal_lval_ref,
        std::move(normal_rval_ref),
        normal_by_copied_val,
        std::move(normal_by_moved_val));

    KW_CALL(free_detect_copy_and_move,
        kw<"const_ref">,
        named_const_ref,
        kw<"lval_ref">,
        named_lval_ref,
        kw<"rval_ref">,
        std::move(named_rval_ref),
        kw<"by_copied_val">,
        named_by_copied_val,
        kw<"by_moved_val">,
        std::move(named_by_moved_val));

    CHECK(normal_stats_const_ref.copy, 0uz);
    CHECK(normal_stats_const_ref.move, 0uz);

    CHECK(normal_stats_lval_ref.copy, 0uz);
    CHECK(normal_stats_lval_ref.move, 0uz);

    CHECK(normal_stats_rval_ref.copy, 0uz);
    CHECK(normal_stats_rval_ref.move, 0uz);

    CHECK(normal_stats_by_copied_val.copy, 1uz);
    CHECK(normal_stats_by_copied_val.move, 0uz);

    CHECK(normal_stats_by_moved_val.copy, 0uz);
    CHECK(normal_stats_by_moved_val.move, 1uz);

    CHECK(named_stats_const_ref.copy, 0uz);
    CHECK(named_stats_const_ref.move, 0uz);

    CHECK(named_stats_lval_ref.copy, 0uz);
    CHECK(named_stats_lval_ref.move, 0uz);

    CHECK(named_stats_rval_ref.copy, 0uz);
    CHECK(named_stats_rval_ref.move, 0uz);

    CHECK(named_stats_by_copied_val.copy, 1uz);
    CHECK(named_stats_by_copied_val.move, 0uz);

    CHECK(named_stats_by_moved_val.copy, 0uz);
    CHECK(named_stats_by_moved_val.move, 1uz);

    CHECK(normal_stats_const_ref.copy, named_stats_const_ref.copy);
    CHECK(normal_stats_const_ref.move, named_stats_const_ref.move);

    CHECK(normal_stats_lval_ref.copy, named_stats_lval_ref.copy);
    CHECK(normal_stats_lval_ref.move, named_stats_lval_ref.move);

    CHECK(normal_stats_rval_ref.copy, named_stats_rval_ref.copy);
    CHECK(normal_stats_rval_ref.move, named_stats_rval_ref.move);

    CHECK(normal_stats_by_copied_val.copy, named_stats_by_copied_val.copy);
    CHECK(normal_stats_by_copied_val.move, named_stats_by_copied_val.move);

    CHECK(normal_stats_by_moved_val.copy, named_stats_by_moved_val.copy);
    CHECK(normal_stats_by_moved_val.move, named_stats_by_moved_val.move);
  }

  return 0;
}
