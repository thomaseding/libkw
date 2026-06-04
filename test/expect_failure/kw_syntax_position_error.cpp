#include "libkw.hpp"

using libkw::kw;

int add(int x, int y)
{
  return x + y;
}

int main()
{
  return KW_CALL(add, 1, kw<"x">, kw<"y">, 2);
}
