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

# Application.cpp: keep the original feature/state/timing logic and allow
# ordinary Win32 SendInput ONLY while the foreground process is notepad.exe.
# There is no driver path, anti-cheat bypass, process injection, or suppression.
app = read('Application.cpp')
if '#include <cwchar>' not in app:
    app = app.replace('#include <memory>\n', '#include <cwchar>\n#include <memory>\n', 1)
app = sub_once(
    app,
    r'bool InjectKeys\(const int \*keys, std::size_t count, bool keyDown\) noexcept \{.*?\n\}\n\nbool InjectKey\(int vk, bool keyDown\) noexcept \{.*?\n\}',
    '''bool ForegroundIsNotepad() noexcept {
  HWND foreground = GetForegroundWindow();
  if (!foreground) {
    return false;
  }

  DWORD processId = 0;
  GetWindowThreadProcessId(foreground, &processId);
  if (processId == 0) {
    return false;
  }

  HANDLE process =
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
  if (!process) {
    return false;
  }

  wchar_t imagePath[32768]{};
  DWORD imagePathLength = static_cast<DWORD>(std::size(imagePath));
  const bool queried =
      QueryFullProcessImageNameW(process, 0, imagePath, &imagePathLength) != 0;
  CloseHandle(process);
  if (!queried) {
    return false;
  }

  const wchar_t *fileName = std::wcsrchr(imagePath, L'\\\\');
  fileName = fileName ? fileName + 1 : imagePath;
  return _wcsicmp(fileName, L"notepad.exe") == 0;
}

bool InjectKeys(const int *keys, std::size_t count, bool keyDown) noexcept {
  if (!keys || count == 0) {
    return true;
  }

  // TEST-ONLY SAFETY BOUNDARY: real user-mode injection is permitted only
  // when Windows Notepad is the foreground process.
  if (!ForegroundIsNotepad()) {
    return true;
  }

  bool allInjected = true;
  for (std::size_t i = 0; i < count; ++i) {
    const UINT mapped =
        MapVirtualKeyW(static_cast<UINT>(keys[i]), MAPVK_VK_TO_VSC_EX);
    if (mapped == 0) {
      allInjected = false;
      continue;
    }

    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 0;
    input.ki.wScan = static_cast<WORD>(mapped & 0xFFu);
    input.ki.dwFlags = KEYEVENTF_SCANCODE;
    if ((mapped & 0xFF00u) == 0xE000u) {
      input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    if (!keyDown) {
      input.ki.dwFlags |= KEYEVENTF_KEYUP;
    }
    input.ki.dwExtraInfo = NEO_SYNTHETIC_INFORMATION;

    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
      allInjected = false;
      ReportInjectionFailure();
    }
  }

  return allInjected;
}

bool InjectKey(int vk, bool keyDown) noexcept {
  return InjectKeys(&vk, 1, keyDown);
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
    ImGui::TextColored(safeColor, "Notepad test injection ready");
    ImGui::Spacing();

    int backend = 0;
    ImGui::RadioButton("WinHook + Notepad test", &backend, 0);
    ImGui::SameLine();
    ImGui::BeginDisabled();
    ImGui::RadioButton("Interception", &backend, 1);
    ImGui::EndDisabled();
    ImGui::TextDisabled("  Test injection is limited to foreground notepad.exe; Interception is disabled.");
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
if 'return 1; // Suppress event' in checks['KbdHookBackend.cpp']:
    raise RuntimeError('KbdHookBackend.cpp: physical-key suppression remains')
for token in ('LoadLibrary', 'interception_send', 'interception_create_context'):
    if token in checks['InterceptionBackend.cpp']:
        raise RuntimeError(f'InterceptionBackend.cpp: unsafe driver token remains: {token}')
if 'LoadLibraryExW(availableDllPath' in checks['gui/GuiManager.cpp']:
    raise RuntimeError('GuiManager.cpp: Interception driver probing remains')

# Validate the intentionally narrow test-injection boundary.
app_after = checks['Application.cpp']
for token in ('SendInput(', 'GetForegroundWindow()', 'notepad.exe'):
    if token not in app_after:
        raise RuntimeError(f'Application.cpp: missing Notepad-only injection guard token: {token}')

# SendInput is allowed only in Application.cpp, where the foreground Notepad
# process check gates it. No backend or other translation unit may inject.
for cpp in root.rglob('*.cpp'):
    rel = str(cpp.relative_to(root)).replace('\\\\', '/')
    text = cpp.read_text(encoding='utf-8-sig')
    if rel != 'Application.cpp' and 'SendInput(' in text:
        raise RuntimeError(f'{rel}: unexpected SendInput call remains after safe patch')

print('Safe Notepad-test patch validated successfully.')
print(' - Original ImGui/DX11 UI and feature/state logic retained')
print(' - Standard SendInput enabled only for foreground notepad.exe')
print(' - Physical keyboard suppression disabled')
print(' - Interception driver backend/probing disabled')
