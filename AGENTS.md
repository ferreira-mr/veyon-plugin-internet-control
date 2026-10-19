# AGENTS.md - Developer & AI Agent Guidelines for Plugin Veyon InternetControl

This document serves as the architectural and technical specification for AI agents and human developers maintaining or extending the **InternetControl** plugin for **Veyon (v4.10+)**.

---

## 1. Project Mission & High-Level Architecture

The **InternetControl** plugin provides real-time, low-latency, tamper-resistant internet blocking and whitelist filtering for Windows 10/11 classrooms managed via Veyon.

```
                      +-----------------------------+
                      |   Veyon Master (Teacher)    |
                      |  InternetControlPlugin      |
                      |  InternetControlDialog      |
                      +--------------+--------------+
                                     |
                       Feature::Uid Network Messages
                       (7b9c1d2e-3f4a-5b6c-...)
                                     |
                                     v
                      +-----------------------------+
                      |    Veyon Server (Student)   |
                      |  Runs as SYSTEM in Session 1|
                      +--------------+--------------+
                                     |
          +--------------------------+--------------------------+
          |                                                     |
          v                                                     v
+-----------------------------+                       +-----------------------------+
|    WindowsFirewallManager   |                       |    WindowsPolicyManager     |
| - INetFwPolicy2 COM API     |                       | - Multi-Hive Registry       |
| - Outbound Ports 80 & 443   |                       |   (HKLM + HKU\<SID>)        |
| - Preserves RFC 1918 & Veyon|                       | - Edge Background Suppression|
| - Fast TCP table termination|                       | - Interactive Browser Detect|
+-----------------------------+                       |   and Relaunch as User      |
                                                      +-----------------------------+
```

### Core Binaries and Targets
- **Target Plugin:** `internetcontrol.dll` (installed to `C:\Program Files\Veyon\plugins\internetcontrol.dll`).
- **Build Toolchain:** MSYS2 MINGW64 (`g++`, `cmake`, `ninja`, Qt6). **Never use MSVC**; Veyon Windows binaries are built with MinGW GCC and are ABI-incompatible with MSVC.
- **Verification Tools:**
  - `find_missing_proc.exe`: Verifies PE import table of `internetcontrol.dll` against `veyon-core.dll` and system DLLs.
  - `test_loader.exe`: Instantiates `QPluginLoader` and validates `PluginInterface` without starting the full Veyon UI.

---

## 2. Directory Layout & Key Files

```
c:\Projetos\plugin-veyon\
├── build.ps1                       # Build script executing inside MSYS2 MINGW64 bash
├── CMakeLists.txt                  # CMake configuration linking Qt6Core, Qt6Widgets, VeyonCore, Win32 APIs
├── AGENTS.md                       # Developer & Agent instructions (this file)
├── README.md                       # Teacher-facing user guide & documentation
├── instalar_plugin.bat             # Master installer (copies DLL, sets Master configs, restarts service)
├── instalar_aluno.bat              # Student installer (stops Veyon, handles in-use DLL rename, starts service)
├── liberar_internet_emergencia.bat # Standalone emergency unblock script (firewall + registry cleanup)
├── find_missing_proc.cpp/.exe      # Dependency checker for Win32 & Qt imports
├── test_loader.cpp/.exe            # Qt plugin verification runner
└── src/
    ├── InternetControlPlugin.h/.cpp    # Plugin registration, master/server dispatch, serialization
    ├── InternetControlDialog.h/.cpp    # Configuration dialog for allowed URLs on Master
    ├── InternetControlLog.h            # Atomic file logging to ProgramData\VeyonInternetControl\internetcontrol.log
    ├── platform/
    │   ├── WindowsFirewallManager.h/.cpp # INetFwPolicy2 COM rules and TCP table reset
    │   └── WindowsPolicyManager.h/.cpp   # Registry policies, Edge hardening, browser restart logic
    └── resources/                      # Plugin icons (internet-block, internet-configure, etc.)
```

---

## 3. Critical Architectural Rules & Edge Cases

### A. Windows Session Architecture (`SYSTEM` in Session 1)
- In Veyon 4.10, `veyon-server.exe` runs as **NT AUTHORITY\SYSTEM** within the **interactive user session (Session 1)**.
- **Do NOT assume a standard GUI thread:** Background tasks run in worker threads via `runSerialized()` (`QThreadPool`).
- **Desktop Windows Enumeration:** `EnumWindows()` called from a worker thread will fail to enumerate desktop windows because the thread is not attached to the interactive desktop.
  - **Required Pattern:** Use `OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS | DESKTOP_ENUMERATE)` (with fallback to `OpenDesktopW(L"Default", ...)`) and pass the desktop handle to `EnumDesktopWindows()`.
- **Process Creation as User:** Never launch browsers as `SYSTEM`. Browsers will crash, run with elevated privileges, or display disabled profiles.
  - **Required Pattern:** Identify the active session's `explorer.exe`, call `OpenProcessToken` with `TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY`, duplicate token with `DuplicateTokenEx`, create environment block with `CreateEnvironmentBlock`, set `STARTUPINFO.lpDesktop = L"winsta0\\default"`, and call `CreateProcessAsUserW`.

### B. Microsoft Edge Hardening & Startup Boost
- **Startup Boost and Background Mode:** Edge starts at Windows logon in hidden background mode (`msedge.exe --no-startup-window --win-session-start`). Clicking `X` on Edge windows leaves the background process running.
- If background processes stay alive, Edge **never reloads registry policies** and retains unrestricted navigation.
- **Required Lockdown:**
  - Always set in both HKLM and all active user hives (`HKEY_USERS\S-1-5-21-*`):
    - `StartupBoostEnabled = 0`
    - `BackgroundModeEnabled = 0`
    - `GuidedSwitchEnabled = 0`
    - `ImplicitSignInEnabled = 0`
    - `NonMicrosoftAccountSignInEnabled = 0`
    - `BrowserAddProfileEnabled = 0`
    - `BrowserGuestModeEnabled = 0`
    - `InPrivateModeAvailability = 1`
    - `QuicAllowed = 0`
    - `BuiltInDnsClientEnabled = 0`
  - **MSA Account Bypass (Edge 116+):** If the user profile has signed into Edge with a personal Microsoft Account (`edge_account_type == 1` in `AppData\Local\Microsoft\Edge\User Data\Local State`), Edge officially ignores `URLBlocklist`. The plugin must detect this and apply `BrowserSignin = 0` and `NonRemovableProfileEnabled = 0` during active blocks.

### C. Mozilla Firefox Policy Engine
- Firefox reads `SOFTWARE\Policies\Mozilla\Firefox\WebsiteFilter` **only upon process launch**.
- There is no dynamic policy reload event for Firefox. Changing policies requires terminating all `firefox.exe` processes and relaunching them if a visible window was open.

### D. Dual-Layer Total Block & Proxy Bypass Prevention
- When `allowedUrls.isEmpty()` (Total Block):
  - In schools with local HTTP proxies on private IP subnets (`10.x.x.x`, `192.168.x.x`), a firewall-only block allows browsers to connect to the local proxy and browse the web.
  - Therefore, Total Block **must call both**:
    1. `WindowsFirewallManager::blockInternet()` (blocks external outbound ports 80/443).
    2. `WindowsPolicyManager::applyBlockAll()` (sets `URLBlocklist = *` in all browsers and restarts open browsers).

### E. StopBlock Low Latency & TCP Connection Safety
- `applyStop()` must achieve instant unblocking:
  1. Synchronously call `WindowsFirewallManager::restoreInternet()` (~5-10ms COM call).
  2. Asynchronously run `WindowsPolicyManager::clearPolicies(true)` in the worker thread.
  3. **CRITICAL:** **NEVER** call `killPublicTcpConnections()` during unblock! When internet is restored, students immediately open pages. Killing TCP connections on unblock causes `ERR_CONNECTION_RESET` in browsers. Only drop TCP connections when **activating** a block.

### F. Worker Thread Serialization
- `InternetControlPlugin::runSerialized()` dispatches tasks to a dedicated single-threaded `QThreadPool` (`maxThreadCount = 1`, `expiryTimeout = -1`).
- Never run heavy Win32 or COM operations synchronously on the main thread of `veyon-server.exe` as this will freeze the Veyon VNC/RFB frame pipeline.

---

## 4. Development & Build Commands

### Compiling
Run inside PowerShell (project root):
```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

### Deploying to Local Veyon
```powershell
# From an elevated Administrator PowerShell prompt:
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Deploy
```

### Verifying Binary Compatibility
Run via `cmd.exe`:
```cmd
set PATH=C:\Program Files\Veyon;C:\msys64\mingw64\bin;%PATH%
find_missing_proc.exe
test_loader.exe
```

Expected outputs:
- `find_missing_proc.exe` -> `LoadLibraryExA SUCCESS!`
- `test_loader.exe` -> `Plugin name: InternetControl`

---

## 5. Maintenance Checklist for Future Changes

When modifying policy lists, browser targets, or firewall rules:
1. Ensure new registry keys are written to **both** `KEY_WOW64_64KEY` and `KEY_WOW64_32KEY`.
2. Ensure both `HKLM` and all logged-on user SIDs (`HKEY_USERS\S-1-5-21-*`) are updated.
3. Update `liberar_internet_emergencia.bat` whenever new registry keys or firewall rule names are added.
4. Keep the compiled DLL in the root directory (`c:\Projetos\plugin-veyon\internetcontrol.dll`) synchronized with `build\internetcontrol.dll` so that batch installers (`instalar_aluno.bat`, `instalar_plugin.bat`) deploy the latest build.
