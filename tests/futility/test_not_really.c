#include <stdio.h>
#include "test_common.h"

int main(int argc, char *argv[])
{
  TEST_EQ(0, 0, "Not Really A");

  return !gTestSuccess;
}

