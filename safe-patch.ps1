$ErrorActionPreference = "Stop"
$root = Join-Path $PSScriptRoot "upstream"

# Safety boundary 1: the single application-wide injection point becomes a
# simulator sink. Feature/timing threads still execute, so the original UI,
# state monitor, counters and timing behavior remain observable, but no
# synthesized keyboard event is emitted.
$appPath = Join-Path $root "Application.cpp"
$app = Get-Content $appPath -Raw
$pattern = '(?s)bool InjectKeys\(const int \*keys, std::size_t count, bool keyDown\) noexcept \{.*?\n\}\n\nbool InjectKey\(int vk, bool keyDown\) noexcept \{\n  return InjectKeys\(&vk, 1, keyDown\);\n\}'
$replacement = @'
bool InjectKeys(const int *keys, std::size_t count, bool keyDown) noexcept {
  // SAFE EDITION: preserve feature/timing execution but never synthesize
  // keyboard input into Windows or another application.
  (void)keys;
  (void)count;
  (void)keyDown;
  return true;
}

bool InjectKey(int vk, bool keyDown) noexcept {
  // SAFE EDITION: simulation only.
  (void)vk;
  (void)keyDown;
  return true;
}
'@
$app2 = [regex]::Replace($app, $pattern, $replacement)
if ($app2 -eq $app) { throw "Application.cpp injection patch did not match" }
Set-Content $appPath $app2 -Encoding UTF8

# Safety boundary 2: WinHook remains read-only for the Monitor/config UI.
# Feature callbacks may request suppression, but Safe Edition always forwards
# the physical event to Windows.
$hookPath = Join-Path $root "KbdHookBackend.cpp"
$hook = Get-Content $hookPath -Raw
$injectPattern = '(?s)bool KbdHookBackend::InjectKey\(uint16_t scanCode, uint16_t flags\) noexcept \{.*?\n\}'
$injectReplacement = @'
bool KbdHookBackend::InjectKey(uint16_t scanCode, uint16_t flags) noexcept {
  // SAFE EDITION: deliberately no SendInput.
  (void)scanCode;
  (void)flags;
  return true;
}
'@
$hook2 = [regex]::Replace($hook, $injectPattern, $injectReplacement, 1)
if ($hook2 -eq $hook) { throw "KbdHookBackend InjectKey patch did not match" }

$suppressPattern = '(?s)  if \(instance_->callback_\) \{\n    const bool suppress = instance_->callback_\(evt\);\n    if \(suppress\) \{\n      instance_->eventsDropped_\.fetch_add\(1, std::memory_order_relaxed\);\n      return 1; // Suppress event from reaching the rest of OS hook chain\n    \}\n  \}'
$suppressReplacement = @'
  if (instance_->callback_) {
    // SAFE EDITION: feed state/feature logic, but never suppress the user's
    // physical keyboard event.
    (void)instance_->callback_(evt);
  }
'@
$hook3 = [regex]::Replace($hook2, $suppressPattern, $suppressReplacement, 1)
if ($hook3 -eq $hook2) { throw "KbdHookBackend suppression patch did not match" }
Set-Content $hookPath $hook3 -Encoding UTF8

# Safety boundary 3: replace the kernel-driver backend implementation with a
# permanently unavailable stub. The original UI option/status remains visible
# for visual fidelity, but it cannot load, talk to, or send through Interception.
$icPath = Join-Path $root "InterceptionBackend.cpp"
$icStub = @'
#include "InterceptionBackend.h"

InterceptionBackend::~InterceptionBackend() noexcept { Shutdown(); }

bool InterceptionBackend::Initialize() noexcept {
  initialized_ = false;
  running_.store(false, std::memory_order_relaxed);
  healthy_.store(false, std::memory_order_relaxed);
  return false;
}

void InterceptionBackend::Shutdown() noexcept {
  initialized_ = false;
  running_.store(false, std::memory_order_relaxed);
  healthy_.store(false, std::memory_order_relaxed);
  callback_ = nullptr;
}

void InterceptionBackend::SetCallback(EventCallback cb) noexcept {
  callback_ = cb;
}

bool InterceptionBackend::InjectKey(uint16_t scanCode, uint16_t flags) noexcept {
  (void)scanCode;
  (void)flags;
  return false;
}

bool InterceptionBackend::GetStatus(BackendStatus &out) noexcept {
  out.driverActive = false;
  out.eventsCaptured = 0;
  out.eventsDropped = 0;
  out.eventsInjected = 0;
  return true;
}
'@
Set-Content $icPath $icStub -Encoding UTF8

# Mark the executable as Safe Edition in the console log without changing the
# original ImGui layout or StrafeHelper branding.
$mainPath = Join-Path $root "main.cpp"
$main = Get-Content $mainPath -Raw
$main = $main.Replace(' + " starting...");', ' + " SAFE EDITION starting...");')
Set-Content $mainPath $main -Encoding UTF8

Write-Host "Safe patch applied: no SendInput path, no physical-key suppression, Interception backend disabled."
