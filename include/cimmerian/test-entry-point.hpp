#pragma once

#include "cimmerian/test-runner.hpp"
#include "cimmerian/test-debug.hpp"

int main(int argc, char* argv[])
{
  Cimmerian::CheckDebug(argc, argv);
  Cimmerian::TestRunner runner = Cimmerian::TestRunner();
  Cimmerian::TestRunSummary summary = runner.RunAll(&Cimmerian::TestRegistry().GetInstance());
  return (summary.failed == 0) ? 0 : 1;
  return 0;
}
