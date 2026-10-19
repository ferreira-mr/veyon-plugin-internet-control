/*
 * WindowsPolicyManager.cpp - Implementation of WindowsPolicyManager
 *
 * Controls internet navigation via:
 * 1. Browser policies in HKLM\SOFTWARE\Policies and HKU\<SID>\Software\Policies (64-bit and 32-bit registry views)
 *    written with the native Win32 registry API.
 * 2. Edge Startup Boost / Background Mode suppression and profile lockdown so Edge cannot bypass policies.
 * 3. Automatic detection, termination (including background msedge.exe) and user-context relaunch of open browsers
 *    so policy updates (block, whitelist change, unblock) take effect immediately.
 *
 * Copyright (c) 2026 Veyon Community
 */

#include "WindowsPolicyManager.h"
#include "WindowsFirewallManager.h"
#include "InternetControlLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>

#include <windows.h>
#include <tlhelp32.h>
#include <userenv.h>
#include <vector>

// policy roots below HKLM / HKU\<SID> - each one is handled in both the 64-bit and the 32-bit registry view
static const QStringList s_chromiumPolicyRoots = {
	QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Edge"),
	QStringLiteral("SOFTWARE\\Policies\\Google\\Chrome"),
	QStringLiteral("SOFTWARE\\Policies\\BraveSoftware\\Brave"),
	QStringLiteral("SOFTWARE\\Policies\\Opera"),
	QStringLiteral("SOFTWARE\\Policies\\Opera Software\\Opera"),
	QStringLiteral("SOFTWARE\\Policies\\Opera Software\\Opera GX")
};

static const QString s_firefoxBasePolicyRoot = QStringLiteral("SOFTWARE\\Policies\\Mozilla\\Firefox");
static const QString s_firefoxPolicyRoot = QStringLiteral("SOFTWARE\\Policies\\Mozilla\\Firefox\\WebsiteFilter");

static const QStringList s_chromiumListNames = {
	QStringLiteral("URLBlocklist"), QStringLiteral("URLBlacklist"),
	QStringLiteral("URLAllowlist"), QStringLiteral("URLWhitelist")
};

static const QStringList s_policyDwordNames = {
	QStringLiteral("StartupBoostEnabled"),
	QStringLiteral("BackgroundModeEnabled"),
	QStringLiteral("GuidedSwitchEnabled"),
	QStringLiteral("ImplicitSignInEnabled"),
	QStringLiteral("NonMicrosoftAccountSignInEnabled"),
	QStringLiteral("BrowserAddProfileEnabled"),
	QStringLiteral("BrowserGuestModeEnabled"),
	QStringLiteral("InPrivateModeAvailability"),
	QStringLiteral("IncognitoModeAvailability"),
	QStringLiteral("QuicAllowed"),
	QStringLiteral("BuiltInDnsClientEnabled"),
	QStringLiteral("BrowserSignin"),
	QStringLiteral("NonRemovableProfileEnabled")
};

static const REGSAM s_registryViews[] = { KEY_WOW64_64KEY, KEY_WOW64_32KEY };

static const QString s_hostsPath = QStringLiteral("C:/Windows/System32/drivers/etc/hosts");
static const QString s_hostsMarkerStart = QStringLiteral("# --- VEYON INTERNET CONTROL START ---");
static const QString s_hostsMarkerEnd = QStringLiteral("# --- VEYON INTERNET CONTROL END ---");


static std::wstring toW(const QString& s)
{
	return s.toStdWString();
}


static QStringList activeUserSids()
{
	QStringList sids;
	HKEY hUsers = nullptr;
	if (RegOpenKeyExW(HKEY_USERS, nullptr, 0, KEY_ENUMERATE_SUB_KEYS, &hUsers) != ERROR_SUCCESS)
	{
		return sids;
	}

	wchar_t nameBuf[256];
	DWORD index = 0;
	while (true)
	{
		DWORD nameLen = 256;
		const LONG rc = RegEnumKeyExW(hUsers, index++, nameBuf, &nameLen, nullptr, nullptr, nullptr, nullptr);
		if (rc != ERROR_SUCCESS)
		{
			break;
		}
		const QString sid = QString::fromWCharArray(nameBuf, static_cast<int>(nameLen));
		if (sid.startsWith(QStringLiteral("S-1-5-21-")) && !sid.endsWith(QStringLiteral("_Classes")))
		{
			sids.append(sid);
		}
	}

	RegCloseKey(hUsers);
	return sids;
}


static void regDeleteTreeRoot(HKEY rootKey, const QString& subKey)
{
	const auto path = toW(subKey);
	for (const auto view : s_registryViews)
	{
		HKEY key = nullptr;
		if (RegOpenKeyExW(rootKey, path.c_str(), 0, KEY_ALL_ACCESS | view, &key) == ERROR_SUCCESS)
		{
			RegDeleteTreeW(key, nullptr);
			RegCloseKey(key);
			RegDeleteKeyExW(rootKey, path.c_str(), view, 0);
		}
	}
}


static void regDeleteTree(const QString& subKey)
{
	regDeleteTreeRoot(HKEY_LOCAL_MACHINE, subKey);
	for (const auto& sid : activeUserSids())
	{
		regDeleteTreeRoot(HKEY_USERS, sid + QLatin1Char('\\') + subKey);
	}
}


static void regDeleteValueRoot(HKEY rootKey, const QString& subKey, const QString& valueName)
{
	const auto path = toW(subKey);
	const auto val = toW(valueName);
	for (const auto view : s_registryViews)
	{
		HKEY key = nullptr;
		if (RegOpenKeyExW(rootKey, path.c_str(), 0, KEY_SET_VALUE | view, &key) == ERROR_SUCCESS)
		{
			RegDeleteValueW(key, val.c_str());
			RegCloseKey(key);
		}
	}
}


static void regDeleteValue(const QString& subKey, const QString& valueName)
{
	regDeleteValueRoot(HKEY_LOCAL_MACHINE, subKey, valueName);
	for (const auto& sid : activeUserSids())
	{
		regDeleteValueRoot(HKEY_USERS, sid + QLatin1Char('\\') + subKey, valueName);
	}
}


static void regWriteDwordRoot(HKEY rootKey, const QString& subKey, const QString& valueName, DWORD data)
{
	const auto path = toW(subKey);
	const auto val = toW(valueName);
	for (const auto view : s_registryViews)
	{
		HKEY key = nullptr;
		if (RegCreateKeyExW(rootKey, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
							KEY_SET_VALUE | view, nullptr, &key, nullptr) == ERROR_SUCCESS)
		{
			RegSetValueExW(key, val.c_str(), 0, REG_DWORD,
						   reinterpret_cast<const BYTE*>(&data), sizeof(DWORD));
			RegCloseKey(key);
		}
	}
}


static void regWriteDword(const QString& subKey, const QString& valueName, DWORD data)
{
	regWriteDwordRoot(HKEY_LOCAL_MACHINE, subKey, valueName, data);
	for (const auto& sid : activeUserSids())
	{
		regWriteDwordRoot(HKEY_USERS, sid + QLatin1Char('\\') + subKey, valueName, data);
	}
}


static void regWriteListRoot(HKEY rootKey, const QString& subKey, const QStringList& values)
{
	const auto path = toW(subKey);
	for (const auto view : s_registryViews)
	{
		HKEY key = nullptr;
		const auto rc = RegCreateKeyExW(rootKey, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
										KEY_SET_VALUE | view, nullptr, &key, nullptr);
		if (rc != ERROR_SUCCESS)
		{
			continue;
		}

		int index = 1;
		for (const auto& value : values)
		{
			const auto name = std::to_wstring(index++);
			const auto data = toW(value);
			RegSetValueExW(key, name.c_str(), 0, REG_SZ,
						   reinterpret_cast<const BYTE*>(data.c_str()),
						   static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
		}
		RegCloseKey(key);
	}
}


// writes the values "1".."n" (REG_SZ) to the given key in HKLM and HKU\<SID>
static void regWriteList(const QString& subKey, const QStringList& values)
{
	regWriteListRoot(HKEY_LOCAL_MACHINE, subKey, values);
	for (const auto& sid : activeUserSids())
	{
		regWriteListRoot(HKEY_USERS, sid + QLatin1Char('\\') + subKey, values);
	}
}


// Checks if any user profile on the machine has an Edge profile signed in with a personal Microsoft Account (MSA, edge_account_type == 1)
static bool hasEdgePersonalMsaProfile()
{
	QDir usersDir(QStringLiteral("C:/Users"));
	const auto userFolders = usersDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	static const QRegularExpression msaRegex(QStringLiteral("\"edge_account_type\"\\s*:\\s*1\\b"));

	for (const auto& folder : userFolders)
	{
		QFile localState(QStringLiteral("C:/Users/%1/AppData/Local/Microsoft/Edge/User Data/Local State").arg(folder));
		if (localState.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			const QString content = QString::fromUtf8(localState.readAll());
			localState.close();
			if (msaRegex.match(content).hasMatch())
			{
				return true;
			}
		}
	}
	return false;
}


static void notifyPolicyRefresh()
{
	// force the Group Policy engine to refresh machine and user policies
	RefreshPolicyEx(TRUE, RP_FORCE);
	RefreshPolicyEx(FALSE, RP_FORCE);

	DWORD_PTR result = 0;
	SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Policy"),
						SMTO_ABORTIFHUNG | SMTO_NOTIMEOUTIFNOTHUNG, 100, &result);
}


namespace {

struct BrowserTarget
{
	QString exeName;
	QString fullPath;
	bool hadVisibleWindow{false};
	bool isChromium{true};
	std::vector<HWND> windows;
};

struct EnumWindowsContext
{
	QMap<QString, BrowserTarget>* targets{nullptr};
};

QString queryProcessFullPath(DWORD pid)
{
	QString result;
	HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (hProc)
	{
		wchar_t buf[MAX_PATH * 2];
		DWORD size = MAX_PATH * 2;
		if (QueryFullProcessImageNameW(hProc, 0, buf, &size))
		{
			result = QString::fromWCharArray(buf, static_cast<int>(size));
		}
		CloseHandle(hProc);
	}
	return result;
}

BOOL CALLBACK enumDesktopWindowsProc(HWND hwnd, LPARAM lParam)
{
	auto* ctx = reinterpret_cast<EnumWindowsContext*>(lParam);
	if (!ctx || !ctx->targets)
	{
		return TRUE;
	}

	if (!IsWindowVisible(hwnd))
	{
		return TRUE;
	}

	const LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
	if ((exStyle & WS_EX_TOOLWINDOW) != 0)
	{
		return TRUE;
	}

	if (GetWindowTextLengthW(hwnd) <= 0)
	{
		return TRUE;
	}

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == 0)
	{
		return TRUE;
	}

	const QString fullPath = queryProcessFullPath(pid);
	if (fullPath.isEmpty())
	{
		return TRUE;
	}

	const QString exeName = QFileInfo(fullPath).fileName().toLower();
	auto it = ctx->targets->find(exeName);
	if (it != ctx->targets->end())
	{
		it->hadVisibleWindow = true;
		if (it->fullPath.isEmpty())
		{
			it->fullPath = fullPath;
		}
		it->windows.push_back(hwnd);
	}

	return TRUE;
}

DWORD findExplorerPidInActiveSession()
{
	DWORD targetSessionId = WTSGetActiveConsoleSessionId();
	if (targetSessionId == 0xFFFFFFFF)
	{
		ProcessIdToSessionId(GetCurrentProcessId(), &targetSessionId);
	}

	DWORD explorerPid = 0;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof(pe);
	if (Process32FirstW(snap, &pe))
	{
		do
		{
			if (_wcsicmp(pe.szExeFile, L"explorer.exe") == 0)
			{
				DWORD sessId = 0;
				if (ProcessIdToSessionId(pe.th32ProcessID, &sessId) && sessId == targetSessionId)
				{
					explorerPid = pe.th32ProcessID;
					break;
				}
				if (explorerPid == 0)
				{
					explorerPid = pe.th32ProcessID;
				}
			}
		} while (Process32NextW(snap, &pe));
	}

	CloseHandle(snap);
	return explorerPid;
}

bool launchBrowserAsLoggedOnUser(const QString& exePath, bool isChromium)
{
	const DWORD explorerPid = findExplorerPidInActiveSession();
	if (explorerPid == 0)
	{
		InternetControlLog::write(QStringLiteral("WARNING: explorer.exe not found to relaunch %1").arg(exePath));
		return false;
	}

	HANDLE hExplorer = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, explorerPid);
	if (!hExplorer)
	{
		InternetControlLog::write(QStringLiteral("WARNING: OpenProcess(explorer) failed (%1)").arg(GetLastError()));
		return false;
	}

	HANDLE hUserToken = nullptr;
	if (!OpenProcessToken(hExplorer, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, &hUserToken))
	{
		InternetControlLog::write(QStringLiteral("WARNING: OpenProcessToken(explorer) failed (%1)").arg(GetLastError()));
		CloseHandle(hExplorer);
		return false;
	}

	HANDLE hPrimaryToken = nullptr;
	if (!DuplicateTokenEx(hUserToken, MAXIMUM_ALLOWED, nullptr, SecurityImpersonation, TokenPrimary, &hPrimaryToken))
	{
		InternetControlLog::write(QStringLiteral("WARNING: DuplicateTokenEx failed (%1)").arg(GetLastError()));
		CloseHandle(hUserToken);
		CloseHandle(hExplorer);
		return false;
	}

	LPVOID envBlock = nullptr;
	CreateEnvironmentBlock(&envBlock, hPrimaryToken, FALSE);

	const QString cmdStr = isChromium
		? QStringLiteral("\"%1\" --restore-last-session").arg(exePath)
		: QStringLiteral("\"%1\"").arg(exePath);

	std::wstring cmdLine = cmdStr.toStdWString();
	std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
	cmdBuf.push_back(L'\0');

	const std::wstring exeW = exePath.toStdWString();
	const std::wstring workDirW = QFileInfo(exePath).absolutePath().toStdWString();

	wchar_t desktopName[] = L"winsta0\\default";
	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.lpDesktop = desktopName;
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOWNORMAL;

	PROCESS_INFORMATION pi{};
	const BOOL launched = CreateProcessAsUserW(
		hPrimaryToken,
		exeW.c_str(),
		cmdBuf.data(),
		nullptr,
		nullptr,
		FALSE,
		CREATE_UNICODE_ENVIRONMENT | NORMAL_PRIORITY_CLASS,
		envBlock,
		workDirW.c_str(),
		&si,
		&pi);

	if (launched)
	{
		InternetControlLog::write(QStringLiteral("Relaunched browser as user: %1 (PID %2)").arg(cmdStr).arg(pi.dwProcessId));
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	else
	{
		InternetControlLog::write(QStringLiteral("ERROR: CreateProcessAsUserW failed for %1 (error %2)").arg(cmdStr).arg(GetLastError()));
	}

	if (envBlock)
	{
		DestroyEnvironmentBlock(envBlock);
	}
	CloseHandle(hPrimaryToken);
	CloseHandle(hUserToken);
	CloseHandle(hExplorer);

	return launched != FALSE;
}

} // namespace


void WindowsPolicyManager::restartOpenBrowsers()
{
	QMap<QString, BrowserTarget> targets = {
		{ QStringLiteral("msedge.exe"),  { QStringLiteral("msedge.exe"),  {}, false, true,  {} } },
		{ QStringLiteral("chrome.exe"),  { QStringLiteral("chrome.exe"),  {}, false, true,  {} } },
		{ QStringLiteral("firefox.exe"), { QStringLiteral("firefox.exe"), {}, false, false, {} } },
		{ QStringLiteral("brave.exe"),   { QStringLiteral("brave.exe"),   {}, false, true,  {} } },
		{ QStringLiteral("opera.exe"),   { QStringLiteral("opera.exe"),   {}, false, true,  {} } }
	};

	// 1. Detect visible windows on the interactive user desktop (winsta0\default)
	EnumWindowsContext ctx{ &targets };
	HDESK hDesk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS | DESKTOP_ENUMERATE);
	if (!hDesk)
	{
		hDesk = OpenDesktopW(L"Default", 0, FALSE, DESKTOP_READOBJECTS | DESKTOP_ENUMERATE);
	}

	if (hDesk)
	{
		EnumDesktopWindows(hDesk, enumDesktopWindowsProc, reinterpret_cast<LPARAM>(&ctx));
		CloseDesktop(hDesk);
	}
	else
	{
		EnumWindows(enumDesktopWindowsProc, reinterpret_cast<LPARAM>(&ctx));
	}

	// 2. Ask visible browser windows to close gracefully first so session state is saved cleanly
	bool anyVisibleWindow = false;
	for (auto it = targets.begin(); it != targets.end(); ++it)
	{
		if (it->hadVisibleWindow)
		{
			anyVisibleWindow = true;
			InternetControlLog::write(QStringLiteral("Detected open browser window: %1 (%2)")
									  .arg(it->exeName, it->fullPath));
			for (HWND hwnd : it->windows)
			{
				PostMessageW(hwnd, WM_CLOSE, 0, 0);
			}
		}
	}

	if (anyVisibleWindow)
	{
		Sleep(250);
	}

	// 3. Force-terminate all remaining processes of the target browsers (including background msedge.exe)
	for (int pass = 0; pass < 2; ++pass)
	{
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snap == INVALID_HANDLE_VALUE)
		{
			break;
		}

		std::vector<HANDLE> waitHandles;
		PROCESSENTRY32W pe{};
		pe.dwSize = sizeof(pe);

		if (Process32FirstW(snap, &pe))
		{
			do
			{
				const QString exeName = QString::fromWCharArray(pe.szExeFile).toLower();
				auto it = targets.find(exeName);
				if (it != targets.end())
				{
					if (it->fullPath.isEmpty())
					{
						it->fullPath = queryProcessFullPath(pe.th32ProcessID);
					}

					HANDLE hProc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
					if (hProc)
					{
						TerminateProcess(hProc, 0);
						waitHandles.push_back(hProc);
					}
				}
			} while (Process32NextW(snap, &pe));
		}
		CloseHandle(snap);

		if (waitHandles.empty())
		{
			break;
		}

		for (HANDLE hProc : waitHandles)
		{
			WaitForSingleObject(hProc, 1500);
			CloseHandle(hProc);
		}

		Sleep(100);
	}

	// 4. Relaunch only the browsers that had a visible window open on the student's desktop
	for (auto it = targets.cbegin(); it != targets.cend(); ++it)
	{
		if (it->hadVisibleWindow && !it->fullPath.isEmpty())
		{
			launchBrowserAsLoggedOnUser(it->fullPath, it->isChromium);
		}
	}
}


void WindowsPolicyManager::flushDnsFast()
{
	static auto dnsFlush = reinterpret_cast<BOOL(WINAPI*)()>(
		GetProcAddress(GetModuleHandleW(L"dnsapi.dll"), "DnsFlushResolverCache"));
	if (!dnsFlush)
	{
		HMODULE hDns = LoadLibraryW(L"dnsapi.dll");
		if (hDns)
		{
			dnsFlush = reinterpret_cast<BOOL(WINAPI*)()>(
				GetProcAddress(hDns, "DnsFlushResolverCache"));
		}
	}
	if (dnsFlush)
	{
		dnsFlush();
	}
	else
	{
		QProcess::execute(QStringLiteral("ipconfig.exe"), { QStringLiteral("/flushdns") });
	}
}


void WindowsPolicyManager::applyCommonBrowserLockdown()
{
	const QString edgeRoot = QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Edge");
	regWriteDword(edgeRoot, QStringLiteral("StartupBoostEnabled"), 0);
	regWriteDword(edgeRoot, QStringLiteral("BackgroundModeEnabled"), 0);
	regWriteDword(edgeRoot, QStringLiteral("GuidedSwitchEnabled"), 0);
	regWriteDword(edgeRoot, QStringLiteral("ImplicitSignInEnabled"), 0);
	regWriteDword(edgeRoot, QStringLiteral("NonMicrosoftAccountSignInEnabled"), 0);
	regWriteDword(edgeRoot, QStringLiteral("BrowserAddProfileEnabled"), 0);
	regWriteDword(edgeRoot, QStringLiteral("BrowserGuestModeEnabled"), 0);
	regWriteDword(edgeRoot, QStringLiteral("InPrivateModeAvailability"), 1);
	regWriteDword(edgeRoot, QStringLiteral("QuicAllowed"), 0);
	regWriteDword(edgeRoot, QStringLiteral("BuiltInDnsClientEnabled"), 0);

	// If a student signed into Edge with a personal Microsoft Account (MSA), Edge 116+ ignores URLBlocklist
	// unless browser sign-in is disabled. Organizational/school Entra ID accounts (edge_account_type == 2) are preserved.
	if (hasEdgePersonalMsaProfile())
	{
		InternetControlLog::write(QStringLiteral("Personal MSA profile detected in Edge - disabling BrowserSignin during block"));
		regWriteDword(edgeRoot, QStringLiteral("BrowserSignin"), 0);
		regWriteDword(edgeRoot, QStringLiteral("NonRemovableProfileEnabled"), 0);
	}

	for (const auto& root : s_chromiumPolicyRoots)
	{
		if (root != edgeRoot)
		{
			regWriteDword(root, QStringLiteral("BackgroundModeEnabled"), 0);
			regWriteDword(root, QStringLiteral("BrowserAddProfileEnabled"), 0);
			regWriteDword(root, QStringLiteral("BrowserGuestModeEnabled"), 0);
			regWriteDword(root, QStringLiteral("IncognitoModeAvailability"), 1);
			regWriteDword(root, QStringLiteral("QuicAllowed"), 0);
			regWriteDword(root, QStringLiteral("BuiltInDnsClientEnabled"), 0);
		}
	}

	regWriteDword(s_firefoxBasePolicyRoot, QStringLiteral("DisablePrivateBrowsing"), 1);
}


void WindowsPolicyManager::removeAllPolicyKeys()
{
	for (const auto& root : s_chromiumPolicyRoots)
	{
		for (const auto& list : s_chromiumListNames)
		{
			regDeleteTree(root + QLatin1Char('\\') + list);
		}
		for (const auto& valName : s_policyDwordNames)
		{
			regDeleteValue(root, valName);
		}
	}

	regDeleteTree(s_firefoxPolicyRoot + QStringLiteral("\\Block"));
	regDeleteTree(s_firefoxPolicyRoot + QStringLiteral("\\Exceptions"));
	regDeleteTree(s_firefoxPolicyRoot);
	regDeleteValue(s_firefoxBasePolicyRoot, QStringLiteral("DisablePrivateBrowsing"));
}


void WindowsPolicyManager::clearHosts()
{
	// removes entries written by older plugin versions
	QFile file(s_hostsPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		return;
	}

	QString content = QString::fromUtf8(file.readAll());
	file.close();

	const auto startPos = content.indexOf(s_hostsMarkerStart);
	const auto endPos = content.indexOf(s_hostsMarkerEnd);

	if (startPos >= 0 && endPos > startPos)
	{
		content.remove(startPos, endPos + s_hostsMarkerEnd.length() - startPos);

		while (content.endsWith(QStringLiteral("\n\n")))
		{
			content.chop(1);
		}

		if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		{
			file.write(content.toUtf8());
			file.close();
			InternetControlLog::write(QStringLiteral("Hosts file cleaned"));
		}
	}
}


QString WindowsPolicyManager::normalizeEntry(const QString& input)
{
	QString s = input.trimmed().toLower();
	if (s.isEmpty() || s == QStringLiteral("*") || s == QStringLiteral("<all_urls>"))
	{
		return {};
	}

	const auto schemePos = s.indexOf(QStringLiteral("://"));
	if (schemePos >= 0)
	{
		s = s.mid(schemePos + 3);
	}

	s.remove(QLatin1Char('*'));
	while (s.startsWith(QLatin1Char('.')))
	{
		s.remove(0, 1);
	}
	if (s.startsWith(QStringLiteral("www.")))
	{
		s.remove(0, 4);
	}

	while (s.endsWith(QLatin1Char('/')))
	{
		s.chop(1);
	}

	return s;
}


bool WindowsPolicyManager::hasActivePolicies()
{
	for (const auto& root : s_chromiumPolicyRoots)
	{
		for (const auto& list : s_chromiumListNames)
		{
			const auto path = toW(root + QLatin1Char('\\') + list);
			for (const auto view : s_registryViews)
			{
				HKEY key = nullptr;
				if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ | view, &key) == ERROR_SUCCESS)
				{
					RegCloseKey(key);
					return true;
				}
			}
		}
	}

	const auto ffBlock = toW(s_firefoxPolicyRoot + QStringLiteral("\\Block"));
	for (const auto view : s_registryViews)
	{
		HKEY key = nullptr;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, ffBlock.c_str(), 0, KEY_READ | view, &key) == ERROR_SUCCESS)
		{
			RegCloseKey(key);
			return true;
		}
	}

	return false;
}


void WindowsPolicyManager::clearPolicies(bool restartBrowsers)
{
	clearHosts();
	removeAllPolicyKeys();
	flushDnsFast();
	notifyPolicyRefresh();

	if (restartBrowsers)
	{
		restartOpenBrowsers();
	}

	InternetControlLog::write(QStringLiteral("Browser policies cleared; browsers updated"));
}


void WindowsPolicyManager::applyBlockAll()
{
	clearHosts();
	removeAllPolicyKeys();
	applyCommonBrowserLockdown();

	const QStringList all{ QStringLiteral("*") };
	const QStringList internalAllow{
		QStringLiteral("about:blank"),
		QStringLiteral("edge://newtab"),
		QStringLiteral("chrome://newtab"),
		QStringLiteral("chrome-untrusted://*")
	};

	for (const auto& root : s_chromiumPolicyRoots)
	{
		regWriteList(root + QStringLiteral("\\URLBlocklist"), all);
		regWriteList(root + QStringLiteral("\\URLBlacklist"), all);
		regWriteList(root + QStringLiteral("\\URLAllowlist"), internalAllow);
		regWriteList(root + QStringLiteral("\\URLWhitelist"), internalAllow);
	}
	regWriteList(s_firefoxPolicyRoot + QStringLiteral("\\Block"), { QStringLiteral("<all_urls>") });

	flushDnsFast();
	WindowsFirewallManager::killPublicTcpConnections();
	notifyPolicyRefresh();
	restartOpenBrowsers();

	InternetControlLog::write(QStringLiteral("Total browser block applied and open browsers restarted"));
}


void WindowsPolicyManager::applyWhitelist(const QStringList& allowedUrls)
{
	QStringList domains;
	for (const auto& url : allowedUrls)
	{
		const auto d = normalizeEntry(url);
		if (!d.isEmpty() && !domains.contains(d))
		{
			domains.append(d);
		}
	}

	if (domains.isEmpty())
	{
		InternetControlLog::write(QStringLiteral("Whitelist is empty - applying total block"));
		applyBlockAll();
		return;
	}

	clearHosts();
	removeAllPolicyKeys();
	applyCommonBrowserLockdown();

	QStringList chromiumAllow = domains;
	chromiumAllow.append(QStringLiteral("about:blank"));
	chromiumAllow.append(QStringLiteral("edge://newtab"));
	chromiumAllow.append(QStringLiteral("chrome://newtab"));
	chromiumAllow.append(QStringLiteral("chrome-untrusted://*"));

	const QStringList all{ QStringLiteral("*") };
	for (const auto& root : s_chromiumPolicyRoots)
	{
		regWriteList(root + QStringLiteral("\\URLBlocklist"), all);
		regWriteList(root + QStringLiteral("\\URLBlacklist"), all);
		regWriteList(root + QStringLiteral("\\URLAllowlist"), chromiumAllow);
		regWriteList(root + QStringLiteral("\\URLWhitelist"), chromiumAllow);
	}

	QStringList firefoxExceptions;
	for (const auto& d : domains)
	{
		firefoxExceptions.append(QStringLiteral("*://*.%1/*").arg(d));
		firefoxExceptions.append(QStringLiteral("*://%1/*").arg(d));
	}
	regWriteList(s_firefoxPolicyRoot + QStringLiteral("\\Block"), { QStringLiteral("<all_urls>") });
	regWriteList(s_firefoxPolicyRoot + QStringLiteral("\\Exceptions"), firefoxExceptions);

	flushDnsFast();
	WindowsFirewallManager::killPublicTcpConnections();
	notifyPolicyRefresh();
	restartOpenBrowsers();

	InternetControlLog::write(QStringLiteral("Whitelist applied (%1 allowed domains) and open browsers restarted").arg(domains.size()));
}
