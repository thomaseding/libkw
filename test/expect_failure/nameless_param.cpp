#include "libkw.hpp"

using libkw::kw;

int add(int x, int)
{
  return x;
}

int main()
{
  return KW_CALL(add, kw<"x">, 1, kw<"y">, 2);
}
