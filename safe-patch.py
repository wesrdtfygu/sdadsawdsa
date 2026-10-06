from __future__ import annotations
import pathlib, re, sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'upstream').resolve()

def read(rel: str) -> str:
    return (root / rel).read_text(encoding='utf-8-sig')

def write(rel: str, text: str) -> None:
    (root / rel).write_text(text, encoding='utf-8', newline='\n')

def sub_once(text: str, pattern: str, replacement: str, label: str) -> str:
    out, n = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if n != 1:
        raise RuntimeError(f'{label}: expected exactly one match, got {n}')
    return out

# Application.cpp: keep all feature/state/timing logic, but make the single
# application-wide injection boundary a no-op simulator sink.
app = read('Application.cpp')
app = sub_once(
    app,
    r'bool InjectKeys\(const int \*keys, std::size_t count, bool keyDown\) noexcept \{.*?\n\}\n\nbool InjectKey\(int vk, bool keyDown\) noexcept \{.*?\n\}',
    '''bool InjectKeys(const int *keys, std::size_t count, bool keyDown) noexcept {
  // SAFE EDITION: feature/timing logic may execute, but no keyboard event is
  // synthesized into Windows or another application.
  (void)keys;
  (void)count;
  (void)keyDown;
  return true;
}

bool InjectKey(int vk, bool keyDown) noexcept {
  (void)vk;
  (void)keyDown;
  return true;
}''',
    'Application injection boundary',
)
write('Application.cpp', app)

# KbdHookBackend.cpp: retain read-only low-level keyboard monitoring for the
# authentic Monitor/config behavior. Never synthesize or suppress OS input.
hook = read('KbdHookBackend.cpp')
hook = sub_once(
    hook,
    r'bool KbdHookBackend::InjectKey\(uint16_t scanCode, uint16_t flags\) noexcept \{.*?\n\}',
    '''bool KbdHookBackend::InjectKey(uint16_t scanCode, uint16_t flags) noexcept {
  // SAFE EDITION: deliberately no SendInput.
  (void)scanCode;
  (void)flags;
  return true;
}''',
    'KbdHook InjectKey',
)
hook = sub_once(
    hook,
    r'  if \(instance_->callback_\) \{\n    const bool suppress = instance_->callback_\(evt\);\n    if \(suppress\) \{\n      instance_->eventsDropped_\.fetch_add\(1, std::memory_order_relaxed\);\n      return 1; // Suppress event from reaching the rest of OS hook chain\n    \}\n  \}',
    '''  if (instance_->callback_) {
    // SAFE EDITION: feed the original state/feature logic, but always allow
    // the user's physical key event to continue through Windows.
    (void)instance_->callback_(evt);
  }''',
    'KbdHook suppression path',
)
write('KbdHookBackend.cpp', hook)

# InterceptionBackend.cpp: keep the class/interface so the original project and
# UI architecture compile unchanged, but make the kernel-driver backend inert.
interception_stub = r'''#define WIN32_LEAN_AND_MEAN
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
'''
write('InterceptionBackend.cpp', interception_stub)

# GuiManager.cpp: preserve the real layout/theme/widgets. Replace only the
# driver-probing portion of the Input Backend panel with a safe status block.
gui = read('gui/GuiManager.cpp')
gui = sub_once(
    gui,
    r'  \{\n    // --- Cached interception availability check \(refresh once per second\) ---.*?\n  \}\n\n  ImGui::PopStyleVar\(2\);',
    '''  {
    const ImVec4 safeColor(0.25f, 0.85f, 0.25f, 1.0f);
    ImGui::TextColored(safeColor, "[  OK  ]");
    ImGui::SameLine();
    ImGui::TextColored(safeColor, "Safe monitor backend ready");
    ImGui::Spacing();

    int backend = 0;
    ImGui::RadioButton("WinHook (default)", &backend, 0);
    ImGui::SameLine();
    ImGui::BeginDisabled();
    ImGui::RadioButton("Interception", &backend, 1);
    ImGui::EndDisabled();
    ImGui::TextDisabled("  Safe Edition: Interception and input injection are disabled.");
  }

  ImGui::PopStyleVar(2);''',
    'GUI input backend panel',
)
write('gui/GuiManager.cpp', gui)

# Make the console/log identity explicit without altering the visible title/layout.
main = read('main.cpp')
needle = ' + " starting...");'
if needle not in main:
    raise RuntimeError('main.cpp startup log marker not found')
main = main.replace(needle, ' + " SAFE EDITION starting...");', 1)
write('main.cpp', main)

# Deterministic safety/patch validation.
checks = {
    'Application.cpp': read('Application.cpp'),
    'KbdHookBackend.cpp': read('KbdHookBackend.cpp'),
    'InterceptionBackend.cpp': read('InterceptionBackend.cpp'),
    'gui/GuiManager.cpp': read('gui/GuiManager.cpp'),
}
for rel, text in checks.items():
    if 'SendInput(' in text:
        raise RuntimeError(f'{rel}: SendInput remains after safe patch')
if 'return 1; // Suppress event' in checks['KbdHookBackend.cpp']:
    raise RuntimeError('KbdHookBackend.cpp: physical-key suppression remains')
for token in ('LoadLibrary', 'interception_send', 'interception_create_context'):
    if token in checks['InterceptionBackend.cpp']:
        raise RuntimeError(f'InterceptionBackend.cpp: unsafe driver token remains: {token}')
if 'LoadLibraryExW(availableDllPath' in checks['gui/GuiManager.cpp']:
    raise RuntimeError('GuiManager.cpp: Interception driver probing remains')

print('Safe patch validated successfully.')
print(' - Original ImGui/DX11 UI and feature/state logic retained')
print(' - SendInput removed from active backends')
print(' - Physical keyboard suppression disabled')
print(' - Interception driver backend/probing disabled')
