#pragma once

#include "cimmerian/test-runner.hpp"
#include "cimmerian/test-debug.hpp"

int main(int argc, char* argv[])
{
  Cimmerian::CheckDebug(argc, argv);
  Cimmerian::TestRunner* runner = new Cimmerian::TestRunner();
  Cimmerian::TestRegistry& registry = Cimmerian::TestRegistry().GetInstance();
  Cimmerian::TestRunSummary summary = runner->RunAll(&registry);
  delete runner;
  runner = nullptr;
  return (summary.failed == 0) ? 0 : 1;
  return 0;
}
