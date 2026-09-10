#include "test_support.h"

namespace strokes::tests {
void run_performance_tests();
}

int main() {
  strokes::tests::run_performance_tests();
  return strokes::tests::failures == 0 ? 0 : 1;
}
