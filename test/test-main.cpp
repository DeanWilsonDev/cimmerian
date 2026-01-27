#include "umbra/test-registry.hpp"
#include "umbra/test-runner.hpp"
#include "umbra/test-debug.hpp"

int main(int argc, char* argv[])
{
  CheckDebug(argc, argv);
  Umbra::Test::TestRunner* runner = new Umbra::Test::TestRunner();
  Umbra::Test::TestRegistry& registry = Umbra::Test::TestRegistry().GetInstance();
  Umbra::Test::TestRunSummary summary = runner->RunAll(&registry);
  return (summary.failed == 0) ? 0 : 1;
}
