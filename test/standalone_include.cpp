// Deliberately the ONLY #include in this translation unit. libkw.hpp must be fully self-contained
#include "libkw.hpp"

using libkw::kw;

int add(int x, int y)
{
  return x + y;
}

int main()
{
  return KW_CALL(add, kw<"x">, 1, kw<"y">, 2) == 3 ? 0 : 1;
}
