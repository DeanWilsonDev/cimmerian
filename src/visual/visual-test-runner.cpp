#include "cimmerian/visual/visual-test-runner.hpp"
#include "cimmerian/crash-report.hpp"
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include "cimmerian/snapshot/snapshot-run-mode.hpp"
#include "cimmerian/test-fail-handler-registry.hpp"
#include "cimmerian/test-log.hpp"
#include "cimmerian/test-runner.hpp"
#include "cimmerian/test-mode.hpp"
#include "cimmerian/test-selection.hpp"

namespace Cimmerian::Visual {

namespace fs = std::filesystem;

namespace {

std::string Base64Encode(const std::vector<uint8_t>& bytes)
{
  static constexpr char TABLE[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

  std::string out;
  out.reserve(((bytes.size() + 2) / 3) * 4);

  std::size_t i = 0;
  while (i + 3 <= bytes.size()) {
    const uint32_t chunk = (static_cast<uint32_t>(bytes[i]) << 16) |
                            (static_cast<uint32_t>(bytes[i + 1]) << 8) | static_cast<uint32_t>(bytes[i + 2]);
    out += TABLE[(chunk >> 18) & 0x3F];
    out += TABLE[(chunk >> 12) & 0x3F];
    out += TABLE[(chunk >> 6) & 0x3F];
    out += TABLE[chunk & 0x3F];
    i += 3;
  }

  const std::size_t remaining = bytes.size() - i;
  if (remaining == 1) {
    const uint32_t chunk = static_cast<uint32_t>(bytes[i]) << 16;
    out += TABLE[(chunk >> 18) & 0x3F];
    out += TABLE[(chunk >> 12) & 0x3F];
    out += "==";
  }
  else if (remaining == 2) {
    const uint32_t chunk = (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8);
    out += TABLE[(chunk >> 18) & 0x3F];
    out += TABLE[(chunk >> 12) & 0x3F];
    out += TABLE[(chunk >> 6) & 0x3F];
    out += '=';
  }

  return out;
}

std::string PngDataUri(const std::string& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return "";
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  const std::string raw = buffer.str();
  std::vector<uint8_t> bytes(raw.begin(), raw.end());
  return "data:image/png;base64," + Base64Encode(bytes);
}

std::string HtmlEscape(const std::string& text)
{
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c;
    }
  }
  return out;
}

// Thrown by SEND() after it has reported why it can't inject, to end the
// test without a second failure. RunOne catches it.
struct AbortVisualTest {};

void CollectForbiddenTests(const VisualTestGroup* group, std::vector<std::string>* forbiddenTests)
{
  const TestModeRegistry& modes = TestModeRegistry::GetInstance();
  for (const VisualTestCase& test : group->GetTests()) {
    if (modes.IsForbidden(test.GetMode())) {
      const std::string groupPath = BuildVisualGroupPath(group);
      forbiddenTests->push_back(
          (groupPath.empty() ? "" : groupPath + " > ") + test.GetName() + "  (" +
          ForbiddenModeReason(test.GetMode()) + ")"
      );
    }
  }
  for (std::size_t i = 0; i < group->GetChildCount(); ++i) {
    CollectForbiddenTests(group->GetChild(i), forbiddenTests);
  }
}

} // namespace

VisualTestRunner::VisualTestRunner()
    : store(std::make_unique<SnapshotStore>(this->snapshotRoot))
{
  this->previousActive = activeInstance;
  activeInstance = this;
}

VisualTestRunner::~VisualTestRunner()
{
  if (activeInstance == this) {
    activeInstance = this->previousActive;
  }
}

void VisualTestRunner::OnTestFail(const TestFailRecord& failure)
{
  if (!this->inTest) {
    std::fprintf(
        stderr, "%s" TAG_ERROR "%s:%d: visual test failure outside of a running test: %s%s\n",
        Ansi::ANSI_COLOR_BRIGHT_RED, failure.file.c_str(), failure.line, failure.message.c_str(),
        Ansi::ANSI_RESET
    );
    for (const std::string& detailLine : failure.details) {
      std::fprintf(stderr, "    %s\n", detailLine.c_str());
    }
    return;
  }
  this->currentTestFailed = true;
  this->pendingFailures.push_back(failure);
}

void VisualTestRunner::SetSnapshotRoot(const std::string& root)
{
  this->snapshotRoot = root;
  this->store = std::make_unique<SnapshotStore>(root);
}

void VisualTestRunner::SetCapture(std::shared_ptr<IScreenCapture> newCapture)
{
  this->capture = newCapture;
  ActiveScreenCapture::GetInstance().Set(newCapture);
}

void VisualTestRunner::SetInjector(std::shared_ptr<IEventInjector> newInjector)
{
  if (newInjector) {
    const bool functional = newInjector->Probe();
    const std::string setupError = newInjector->SetupError();
    if (!setupError.empty()) {
      Log::TestLogPrintTo(
          this->output, Log::LogColor::Yellow,
          TAG_WARN "VisualTestRunner: {}. Visual tests that SEND() events will fail; the others "
                   "still run.",
          setupError
      );
    }
    else if (!functional) {
      Log::TestLogPrintTo(
          this->output, Log::LogColor::Yellow,
          TAG_WARN "VisualTestRunner: registered IEventInjector reports it is not functional - "
                   "SEND() in visual tests will silently no-op instead of actually driving the "
                   "UI, which can produce false passes."
      );
    }
  }
  this->injector = newInjector;
  ActiveEventInjector::GetInstance().Set(newInjector);
}

void VisualTestRunner::ParseArgs(int argc, char* argv[])
{
  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "--update-snapshots") {
      this->SetMode(VisualRunMode::Update);
    }
    else if (arg == "--review-snapshots") {
      this->SetMode(VisualRunMode::Review);
    }
    else if (arg == "--snapshot-dir" && i + 1 < argc) {
      this->SetSnapshotRoot(argv[++i]);
    }
    else if (arg == "--filter" && i + 1 < argc) {
      this->SetFilter(argv[++i]);
    }
  }
  TestSelectionRegistry::GetInstance().ParseArgs(argc, argv);
}

const std::string& VisualTestRunner::GetEffectiveFilter() const
{
  return this->filter.empty() ? TestSelectionRegistry::GetInstance().Get().filter : this->filter;
}

bool VisualTestRunner::MatchesFilter(const std::string& fullyQualifiedName) const
{
  return MatchesTestFilter(fullyQualifiedName, this->GetEffectiveFilter());
}

// Prints the full name of each test a run would execute, one per line, as the
// unit runner does under --list.
void VisualTestRunner::ListTests(const VisualTestGroup* group) const
{
  if (!group) {
    return;
  }
  const std::string groupPath = BuildVisualGroupPath(group);
  for (const VisualTestCase& test : group->GetTests()) {
    const std::string fullName = JoinTestName(groupPath, test.GetName());
    if (this->MatchesFilter(fullName) &&
        TestModeRegistry::GetInstance().ShouldRun(test.GetMode())) {
      std::fprintf(this->output, "%s\n", fullName.c_str());
    }
  }
  for (std::size_t i = 0; i < group->GetChildCount(); ++i) {
    this->ListTests(group->GetChild(i));
  }
}

void VisualTestRunner::SendEvent(const UIEvent& event, std::source_location where)
{
  if (const WaitEvent* wait = std::get_if<WaitEvent>(&event)) {
    std::this_thread::sleep_for(wait->duration);
    return;
  }

  // An event that can't be sent would leave the test asserting against
  // whatever the UI already showed, so the test fails here instead.
  IEventInjector* activeInjector = ActiveEventInjector::GetInstance().Get();
  const std::string unavailable =
      activeInjector ? activeInjector->SetupError()
                     : "no IEventInjector is registered (call VisualTestRunner::SetInjector first)";
  if (!unavailable.empty()) {
    TestFailHandlerRegistry::GetInstance().NotifyTestFail(
        {.file = where.file_name(),
         .line = static_cast<int>(where.line()),
         .message = "SEND() can't inject events:",
         .details = SplitLines(unavailable)}
    );
    if (this->inTest) {
      throw AbortVisualTest{};
    }
    return;
  }
  if (!activeInjector->IsFunctional()) {
    Log::TestLogPrintTo(
        this->output, Log::LogColor::Red,
        TAG_ERROR "SEND: active IEventInjector reports it is not functional - this "
                  "event is likely a no-op and any following ASSERT_SNAPSHOT may pass "
                  "against pre-existing UI state rather than the intended interaction"
    );
  }
  if (this->currentWindowHandle && !activeInjector->IsWindowFocused(this->currentWindowHandle)) {
    Log::TestLogPrintTo(
        this->output, Log::LogColor::Yellow,
        TAG_WARN "SEND: target window does not currently have OS input focus - both "
                 "provided Linux backends inject real, screen-absolute OS input with "
                 "no concept of 'target window', so this event may land in a "
                 "different window if focus shifted since the test started (see "
                 "docs/cimmerian_live_app_visual_testing_gap.md gap 4)"
    );
  }
  activeInjector->Inject(event);
}

void VisualTestRunner::AssertSnapshot(
    const std::string& label,
    const DiffOptions& options,
    std::source_location where
)
{
  this->AssertSnapshotAgainst(label, this->currentWindowHandle, options, where);
}

void VisualTestRunner::AssertSnapshot(
    const std::string& label,
    void* windowHandle,
    const DiffOptions& options,
    std::source_location where
)
{
  this->AssertSnapshotAgainst(label, windowHandle, options, where);
}

void VisualTestRunner::AssertSnapshotAgainst(
    const std::string& label,
    void* windowHandle,
    const DiffOptions& options,
    const std::source_location& where
)
{
  const std::string groupName = this->currentGroupPath;
  const std::string testName = this->currentTestName ? this->currentTestName : "";

  Screenshot actual = Screenshot::Capture(windowHandle);

  VisualSnapshotReportEntry entry;
  entry.groupName = groupName;
  entry.testName = testName;
  entry.label = label;
  entry.goldenPath = this->store->GoldenPath(groupName, testName, label);
  entry.actualPath = this->store->ActualPath(groupName, testName, label);
  entry.diffPath = this->store->DiffPath(groupName, testName, label);

  fs::create_directories(fs::path(entry.goldenPath).parent_path());

  if (this->mode == VisualRunMode::Update) {
    actual.SavePNG(entry.goldenPath);
    fs::remove(entry.actualPath);
    fs::remove(entry.diffPath);
    entry.passed = true;
    entry.missingGolden = false;
    entry.differencePercent = 0.0f;
    this->reportEntries.push_back(entry);
    return;
  }

  if (!this->store->GoldenExists(groupName, testName, label)) {
    actual.SavePNG(entry.goldenPath);
    entry.passed = true;
    entry.missingGolden = true;
    entry.differencePercent = 0.0f;
    this->reportEntries.push_back(entry);
    return;
  }

  Screenshot golden = Screenshot::LoadPNG(entry.goldenPath);
  DiffResult diff = CompareScreenshots(golden, actual, options);

  entry.passed = diff.passed;
  entry.missingGolden = false;
  entry.differencePercent = diff.differencePercent;

  if (diff.passed) {
    fs::remove(entry.actualPath);
    fs::remove(entry.diffPath);
  }
  else {
    actual.SavePNG(entry.actualPath);
    diff.diffImage.SavePNG(entry.diffPath);

    // Review mode shows mismatches in its report instead of failing tests.
    if (this->mode == VisualRunMode::Verify) {
      TestFailHandlerRegistry::GetInstance().NotifyTestFail(
          {.file = where.file_name(),
           .line = static_cast<int>(where.line()),
           .message = std::format(
               "Snapshot '{}' differs from its golden in {:.2f}% of pixels (allowed {:.2f}%):",
               label, diff.differencePercent, options.maxDifferencePercent
           ),
           .details = {
               "Golden  " + entry.goldenPath,
               "Actual  " + entry.actualPath,
               "Diff    " + entry.diffPath,
               "",
               "Run with --update-snapshots to accept the new image.",
           }}
      );
    }
  }

  this->reportEntries.push_back(entry);
}

void VisualTestRunner::RunOne(const VisualTestGroup* group, const VisualTestCase* test, VisualTestRunSummary* summary)
{
  const std::string groupPath = BuildVisualGroupPath(group);
  const std::string fullyQualifiedName = JoinTestName(groupPath, test->GetName());
  if (!this->MatchesFilter(fullyQualifiedName)) {
    return;
  }
  this->filterMatchCount++;

  if (test->GetMode() == TestMode::Skip) {
    summary->total++;
    summary->skipped++;
    Log::TestLogPrintTo(
        this->output, Log::LogColor::Yellow, "[SKIP] [{}] {}", groupPath, test->GetName()
    );
    return;
  }

  if (!TestModeRegistry::GetInstance().ShouldRun(test->GetMode())) {
    summary->total++;
    summary->skipped++;
    return;
  }

  this->currentGroupPath = groupPath;
  this->currentTestName = test->GetName();
  this->currentWindowHandle = group->GetWindowHandle();
  this->currentTestFailed = false;
  this->pendingFailures.clear();

  const std::size_t entriesBefore = this->reportEntries.size();

  TestFailHandlerRegistry& handlers = TestFailHandlerRegistry::GetInstance();
  ITestFailHandler* const previousHandler = handlers.GetHandler();
  handlers.RegisterHandler(this);
  this->inTest = true;

  // A crash ends the process without flushing stdio, so print everything so
  // far first, and say where a crash would be.
  std::fflush(this->output);
  SetCrashContext(std::format("[{}] {}", groupPath, test->GetName()));
  try {
    test->Run();
  }
  catch (const AbortVisualTest&) {
  }
  catch (const std::exception& exception) {
    this->OnTestFail(
        {.file = "",
         .line = 0,
         .message = "The test threw an exception:",
         .details = SplitLines(exception.what())}
    );
  }
  catch (...) {
    this->OnTestFail(
        {.file = "",
         .line = 0,
         .message = "The test threw something that isn't a std::exception",
         .details = {}}
    );
  }
  SetCrashContext("");

  this->inTest = false;
  handlers.RegisterHandler(previousHandler);

  summary->total++;
  bool anyMissing = false;
  for (std::size_t i = entriesBefore; i < this->reportEntries.size(); ++i) {
    if (this->reportEntries[i].missingGolden) {
      anyMissing = true;
      summary->missingGoldens++;
    }
    if (this->mode == VisualRunMode::Update) {
      summary->updatedGoldens++;
    }
  }

  // Every mode fails a test that failed an assertion or threw. Only Verify
  // mode records a snapshot mismatch as a failure.
  if (this->currentTestFailed) {
    summary->failed++;
    Log::TestLogPrintTo(
        this->output, Log::LogColor::Red, "[FAIL] [{}] {}", groupPath, test->GetName()
    );
    for (const TestFailRecord& failure : this->pendingFailures) {
      PrintFailureBlock(this->output, failure);
    }
    this->pendingFailures.clear();
    std::fprintf(this->output, "\n");
  }
  else {
    summary->passed++;
    if (anyMissing) {
      Log::TestLogPrintTo(
          this->output, Log::LogColor::Yellow, "[NEW]  [{}] {} (golden captured)", groupPath,
          test->GetName()
      );
    }
    else {
      Log::TestLogPrintTo(
          this->output, Log::LogColor::Green, "[PASS] [{}] {}", groupPath, test->GetName()
      );
    }
  }
}

void VisualTestRunner::RunGroup(const VisualTestGroup* group, VisualTestRunSummary* summary)
{
  if (!group) {
    return;
  }

  for (const VisualTestCase& test : group->GetTests()) {
    this->RunOne(group, &test, summary);
  }

  for (std::size_t i = 0; i < group->GetChildCount(); ++i) {
    this->RunGroup(group->GetChild(i), summary);
  }
}

VisualTestRunSummary VisualTestRunner::RunAll(const VisualTestRegistry* registry)
{
  VisualTestRunSummary summary;
  this->reportEntries.clear();
  this->filterMatchCount = 0;

  if (!registry || !registry->GetRootGroup()) {
    Log::TestLogPrintTo(
        this->output, Log::LogColor::Red,
        TAG_ERROR "root VisualTestGroup was unable to be set on default registry"
    );
    return summary;
  }

  // Command-line errors are counted as a failure, like forbidden tests below.
  const TestSelectionOptions& selectionOptions = TestSelectionRegistry::GetInstance().Get();
  if (!selectionOptions.error.empty()) {
    Log::TestLogPrintTo(
        this->output, Log::LogColor::Red, TAG_ERROR "{}; not running any visual tests",
        selectionOptions.error
    );
    summary.failed = 1;
    return summary;
  }

  // Every visual test runs in shard 1, so the shards together still run each
  // one once, and only one process drives the screen.
  const bool runsInThisShard = !selectionOptions.shard || selectionOptions.shard->index == 1;

  if (selectionOptions.listOnly) {
    if (runsInThisShard) {
      this->ListTests(registry->GetRootGroup());
    }
    return summary;
  }

  std::fprintf(this->output, "\n");
  std::fprintf(this->output, "── Visual Regression ──────────────────────────\n");

  std::vector<std::string> forbiddenTests;
  CollectForbiddenTests(registry->GetRootGroup(), &forbiddenTests);
  if (!forbiddenTests.empty()) {
    Log::TestLogPrintTo(
        this->output, Log::LogColor::Red,
        TAG_ERROR "{} forbidden visual test(s) found, not running any visual tests:",
        forbiddenTests.size()
    );
    for (const std::string& forbiddenTest : forbiddenTests) {
      Log::TestLogPrintTo(this->output, Log::LogColor::Red, "  {}", forbiddenTest);
    }
    // Counted as failures so entry points that exit on summary.failed fail the run.
    summary.failed = static_cast<int>(forbiddenTests.size());
    return summary;
  }

  if (!runsInThisShard) {
    std::fprintf(
        this->output, "Shard %d/%d runs no visual tests; they all run in shard 1/%d\n",
        selectionOptions.shard->index, selectionOptions.shard->count, selectionOptions.shard->count
    );
    std::fprintf(this->output, "────────────────────────────────────────────────\n");
    return summary;
  }

  const CrashReportScope crashReport;
  this->RunGroup(registry->GetRootGroup(), &summary);

  std::fprintf(
      this->output, "\nVisual Summary: %d total, %d passed, %d failed, %d updated, %d missing",
      summary.total, summary.passed, summary.failed, summary.updatedGoldens, summary.missingGoldens
  );
  if (summary.skipped > 0) {
    std::fprintf(this->output, ", %d skipped", summary.skipped);
  }
  std::fprintf(this->output, "\n");
  if (!this->GetEffectiveFilter().empty() && this->filterMatchCount == 0) {
    Log::TestLogPrintTo(
        this->output, Log::LogColor::Yellow, "--filter \"{}\" matched no visual tests",
        this->GetEffectiveFilter()
    );
  }
  if (selectionOptions.shard) {
    std::fprintf(
        this->output, "Shard 1/%d runs every visual test\n", selectionOptions.shard->count
    );
  }
  std::fprintf(this->output, "────────────────────────────────────────────────\n");

  if (this->mode == VisualRunMode::Review) {
    this->WriteReport(&summary);
  }

  return summary;
}

void VisualTestRunner::WriteReport(VisualTestRunSummary* summary) const
{
  const std::string reportPath = this->snapshotRoot + "/visual-report.html";
  fs::create_directories(fs::path(reportPath).parent_path());

  std::ostringstream html;
  html << "<!doctype html><html><head><meta charset=\"utf-8\">"
       << "<title>Cimmerian Visual Regression Report</title><style>"
       << "body{font-family:sans-serif;background:#1e1e1e;color:#eee;margin:2rem;}"
       << "h1{font-size:1.4rem;} h2{font-size:1.1rem;margin-top:2rem;border-bottom:1px solid #444;padding-bottom:.3rem;}"
       << ".test{margin-bottom:2rem;} .pass{color:#7CFC7C;} .fail{color:#FF6B6B;} .missing{color:#FFD166;}"
       << ".snapshot{display:flex;gap:1rem;flex-wrap:wrap;margin:.5rem 0 1.5rem;}"
       << ".snapshot figure{margin:0;text-align:center;} .snapshot img{max-width:320px;border:1px solid #444;}"
       << ".label{font-weight:bold;} table{border-collapse:collapse;} </style></head><body>";

  html << "<h1>Cimmerian Visual Regression Report</h1>";
  html << "<p>" << summary->total << " total, " << summary->passed << " passed, " << summary->failed
       << " failed, " << summary->missingGoldens << " missing goldens</p>";

  std::string currentSection;
  for (const VisualSnapshotReportEntry& entry : this->reportEntries) {
    const std::string sectionKey = entry.groupName + " > " + entry.testName;
    if (sectionKey != currentSection) {
      if (!currentSection.empty()) {
        html << "</div>";
      }
      currentSection = sectionKey;
      const char* statusClass = entry.missingGolden ? "missing" : (entry.passed ? "pass" : "fail");
      html << "<h2>" << HtmlEscape(sectionKey) << " <span class=\"" << statusClass << "\">("
           << (entry.missingGolden ? "captured" : (entry.passed ? "pass" : "fail")) << ")</span></h2>";
      html << "<div class=\"test\">";
    }

    html << "<div class=\"snapshot\"><div><p class=\"label\">" << HtmlEscape(entry.label) << " &mdash; "
         << (entry.missingGolden ? "newly captured" : std::format("{:.2f}% different", entry.differencePercent))
         << "</p><div style=\"display:flex;gap:1rem;\">";

    html << "<figure><figcaption>golden</figcaption><img src=\"" << PngDataUri(entry.goldenPath) << "\"></figure>";
    if (!entry.missingGolden) {
      html << "<figure><figcaption>actual</figcaption><img src=\""
           << (entry.passed ? PngDataUri(entry.goldenPath) : PngDataUri(entry.actualPath)) << "\"></figure>";
      if (!entry.passed) {
        html << "<figure><figcaption>diff</figcaption><img src=\"" << PngDataUri(entry.diffPath) << "\"></figure>";
      }
    }
    html << "</div></div></div>";
  }
  if (!currentSection.empty()) {
    html << "</div>";
  }

  html << "</body></html>";

  std::ofstream out(reportPath, std::ios::binary | std::ios::trunc);
  out << html.str();

  summary->reportPath = reportPath;
  std::fprintf(this->output, "Review report written to %s\n", reportPath.c_str());
}

} // namespace Cimmerian::Visual
