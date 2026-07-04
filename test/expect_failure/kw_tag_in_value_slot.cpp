#include "libkw.hpp"

using libkw::kw;

int add3(int x, int y, int z)
{
  return x + y + z;
}

int main()
{
  return KW_CALL(add3, kw<"x">, kw<"oops">, kw<"y">, 5, kw<"z">, 7);
}
