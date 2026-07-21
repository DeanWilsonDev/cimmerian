#include "cimmerian/visual/visual-test-runner.hpp"
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
#include "cimmerian/test-log.hpp"

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

} // namespace

VisualTestRunner::VisualTestRunner()
    : store(std::make_unique<SnapshotStore>(this->snapshotRoot))
{
  activeInstance = this;
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
  if (newInjector && !newInjector->Probe()) {
    TEST_LOG_WARN(
        "VisualTestRunner: registered IEventInjector reports it is not "
        "functional - SEND() in visual tests will silently no-op instead of "
        "actually driving the UI, which can produce false passes."
    );
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
}

bool VisualTestRunner::MatchesFilter(const std::string& fullyQualifiedName) const
{
  if (this->filter.empty()) {
    return true;
  }
  return fullyQualifiedName.find(this->filter) != std::string::npos;
}

void VisualTestRunner::SendEvent(const UIEvent& event)
{
  if (const WaitEvent* wait = std::get_if<WaitEvent>(&event)) {
    std::this_thread::sleep_for(wait->duration);
    return;
  }

  IEventInjector* activeInjector = ActiveEventInjector::GetInstance().Get();
  if (!activeInjector) {
    TEST_LOG_ERROR("SEND: no IEventInjector registered (call VisualTestRunner::SetInjector first)");
    return;
  }
  if (!activeInjector->IsFunctional()) {
    TEST_LOG_ERROR(
        "SEND: active IEventInjector reports it is not functional - this "
        "event is likely a no-op and any following ASSERT_SNAPSHOT may pass "
        "against pre-existing UI state rather than the intended interaction"
    );
  }
  if (this->currentWindowHandle && !activeInjector->IsWindowFocused(this->currentWindowHandle)) {
    TEST_LOG_WARN(
        "SEND: target window does not currently have OS input focus - both "
        "provided Linux backends inject real, screen-absolute OS input with "
        "no concept of 'target window', so this event may land in a "
        "different window if focus shifted since the test started (see "
        "docs/cimmerian_live_app_visual_testing_gap.md gap 4)"
    );
  }
  activeInjector->Inject(event);
}

void VisualTestRunner::AssertSnapshot(const std::string& label, const DiffOptions& options)
{
  this->AssertSnapshotAgainst(label, this->currentWindowHandle, options);
}

void VisualTestRunner::AssertSnapshot(const std::string& label, void* windowHandle, const DiffOptions& options)
{
  this->AssertSnapshotAgainst(label, windowHandle, options);
}

void VisualTestRunner::AssertSnapshotAgainst(const std::string& label, void* windowHandle, const DiffOptions& options)
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
    this->currentTestFailed = true;

    if (this->mode != VisualRunMode::Review) {
      TEST_LOG_ERROR(
          "[FAIL] {} > {} snapshot '{}': {:.2f}% different (actual: {}, diff: {})", groupName, testName,
          label, diff.differencePercent, entry.actualPath, entry.diffPath
      );
    }
  }

  this->reportEntries.push_back(entry);
}

void VisualTestRunner::RunOne(const VisualTestGroup* group, const VisualTestCase* test, VisualTestRunSummary* summary)
{
  const std::string groupPath = BuildVisualGroupPath(group);
  const std::string fullyQualifiedName = groupPath + " > " + test->GetName();
  if (!this->MatchesFilter(fullyQualifiedName)) {
    return;
  }

  this->currentGroupPath = groupPath;
  this->currentTestName = test->GetName();
  this->currentWindowHandle = group->GetWindowHandle();
  this->currentTestFailed = false;

  const std::size_t entriesBefore = this->reportEntries.size();

  try {
    test->Run();
  }
  catch (const std::exception& ex) {
    this->currentTestFailed = true;
    TEST_LOG_ERROR("[{}] {} threw: {}", groupPath, test->GetName(), ex.what());
  }
  catch (...) {
    this->currentTestFailed = true;
    TEST_LOG_ERROR("[{}] {} threw an unknown exception", groupPath, test->GetName());
  }

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

  if (this->mode == VisualRunMode::Verify && this->currentTestFailed) {
    summary->failed++;
    TEST_LOG_PRINT(Log::LogColor::Red, "[FAIL] [{}] {}", groupPath, test->GetName());
  }
  else {
    summary->passed++;
    if (anyMissing) {
      TEST_LOG_PRINT(Log::LogColor::Yellow, "[NEW]  [{}] {} (golden captured)", groupPath, test->GetName());
    }
    else {
      TEST_LOG_PRINT(Log::LogColor::Green, "[PASS] [{}] {}", groupPath, test->GetName());
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

  if (!registry || !registry->GetRootGroup()) {
    TEST_LOG_ERROR("root VisualTestGroup was unable to be set on default registry");
    return summary;
  }

  std::printf("\n");
  std::printf("── Visual Regression ──────────────────────────\n");

  this->RunGroup(registry->GetRootGroup(), &summary);

  std::printf(
      "\nVisual Summary: %d total, %d passed, %d failed, %d updated, %d missing\n", summary.total,
      summary.passed, summary.failed, summary.updatedGoldens, summary.missingGoldens
  );
  std::printf("────────────────────────────────────────────────\n");

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
  std::printf("Review report written to %s\n", reportPath.c_str());
}

} // namespace Cimmerian::Visual
