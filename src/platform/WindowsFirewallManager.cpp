/*
 * WindowsFirewallManager.cpp - Implementation of Windows Defender Firewall management
 *
 * Strategy:
 *  - block ALL protocols (TCP + UDP/QUIC) to public IP addresses only, so that the
 *    school LAN (Veyon, file shares, local DNS) keeps working
 *  - use native Windows Defender Firewall COM API (INetFwPolicy2 / INetFwRules) for
 *    instantaneous, in-memory rule manipulation (sub-millisecond execution, zero process
 *    creation overhead, no antivirus interception delays)
 *  - reset already established TCP connections to public addresses upon blocking so that open
 *    tabs/streams stop immediately (never kill connections upon releasing)
 *  - fallback to netsh only in the unlikely event of COM failure
 *
 * Copyright (c) 2026 Veyon Community
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netfw.h>
#include <oleauto.h>

#include "WindowsFirewallManager.h"
#include "InternetControlLog.h"

#include <QProcess>
#include <QSettings>
#include <vector>

const QString WindowsFirewallManager::s_blockRuleNameV4 = QStringLiteral("Veyon_Internet_Block_v4");
const QString WindowsFirewallManager::s_blockRuleNameV6 = QStringLiteral("Veyon_Internet_Block_v6");
const QString WindowsFirewallManager::s_allowVeyonRuleName = QStringLiteral("Veyon_Internet_AllowVeyon");

static const QString s_stateKey = QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\VeyonInternetControl");

// Public IPv4 addresses (excluding private 10.0.0.0/8, 127.0.0.0/8, 169.254.0.0/16, 172.16.0.0/12, 192.168.0.0/16, multicast)
static const QString s_publicIPv4Addresses = QStringLiteral(
	"1.0.0.0-9.255.255.255,"
	"11.0.0.0-126.255.255.255,"
	"128.0.0.0-169.253.255.255,"
	"169.255.0.0-172.15.255.255,"
	"172.32.0.0-192.167.255.255,"
	"192.169.0.0-223.255.255.255");

static const QString s_publicIPv6Addresses = QStringLiteral("2000::/3");

static const QStringList s_firewallProfileKeys = {
	QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\DomainProfile"),
	QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile"),
	QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\PublicProfile")
};

namespace {

class ScopedCom
{
public:
	ScopedCom() : m_hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
	~ScopedCom()
	{
		if (SUCCEEDED(m_hr))
		{
			CoUninitialize();
		}
	}
	bool isOk() const { return SUCCEEDED(m_hr) || m_hr == RPC_E_CHANGED_MODE; }

private:
	HRESULT m_hr;
};

class Bstr
{
public:
	explicit Bstr(const QString& str) :
		m_bstr(SysAllocString(reinterpret_cast<const OLECHAR*>(str.utf16())))
	{
	}
	~Bstr()
	{
		if (m_bstr)
		{
			SysFreeString(m_bstr);
		}
	}
	operator BSTR() const { return m_bstr; }

private:
	BSTR m_bstr;
};

INetFwPolicy2* getPolicy2()
{
	INetFwPolicy2* pPolicy = nullptr;
	HRESULT hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER,
								  __uuidof(INetFwPolicy2), reinterpret_cast<void**>(&pPolicy));
	if (SUCCEEDED(hr))
	{
		return pPolicy;
	}
	return nullptr;
}

} // namespace


bool WindowsFirewallManager::removeRuleCom(const QString& ruleName)
{
	ScopedCom com;
	if (!com.isOk())
	{
		return false;
	}

	INetFwPolicy2* pPolicy = getPolicy2();
	if (!pPolicy)
	{
		return false;
	}

	INetFwRules* pRules = nullptr;
	HRESULT hr = pPolicy->get_Rules(&pRules);
	if (SUCCEEDED(hr) && pRules)
	{
		Bstr bName(ruleName);
		hr = pRules->Remove(bName);
		pRules->Release();
	}
	pPolicy->Release();

	return SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}


bool WindowsFirewallManager::addOutboundBlockRuleCom(const QString& ruleName, const QString& remoteAddresses)
{
	ScopedCom com;
	if (!com.isOk())
	{
		return false;
	}

	INetFwPolicy2* pPolicy = getPolicy2();
	if (!pPolicy)
	{
		return false;
	}

	INetFwRules* pRules = nullptr;
	HRESULT hr = pPolicy->get_Rules(&pRules);
	if (!SUCCEEDED(hr) || !pRules)
	{
		pPolicy->Release();
		return false;
	}

	// Remove duplicate first if present
	Bstr bName(ruleName);
	pRules->Remove(bName);

	INetFwRule* pRule = nullptr;
	hr = CoCreateInstance(__uuidof(NetFwRule), nullptr, CLSCTX_INPROC_SERVER,
						  __uuidof(INetFwRule), reinterpret_cast<void**>(&pRule));
	if (SUCCEEDED(hr) && pRule)
	{
		pRule->put_Name(bName);
		Bstr bDesc(QStringLiteral("Veyon Internet Control block rule"));
		pRule->put_Description(bDesc);
		pRule->put_Direction(NET_FW_RULE_DIR_OUT);
		pRule->put_Action(NET_FW_ACTION_BLOCK);
		pRule->put_Protocol(256); // 256 = NET_FW_IP_PROTOCOL_ANY
		Bstr bRem(remoteAddresses);
		pRule->put_RemoteAddresses(bRem);
		pRule->put_Profiles(NET_FW_PROFILE2_ALL);
		pRule->put_Enabled(VARIANT_TRUE);

		hr = pRules->Add(pRule);
		pRule->Release();
	}

	pRules->Release();
	pPolicy->Release();
	return SUCCEEDED(hr);
}


bool WindowsFirewallManager::addInboundAllowPortRuleCom(const QString& ruleName, const QString& localPorts)
{
	ScopedCom com;
	if (!com.isOk())
	{
		return false;
	}

	INetFwPolicy2* pPolicy = getPolicy2();
	if (!pPolicy)
	{
		return false;
	}

	INetFwRules* pRules = nullptr;
	HRESULT hr = pPolicy->get_Rules(&pRules);
	if (!SUCCEEDED(hr) || !pRules)
	{
		pPolicy->Release();
		return false;
	}

	Bstr bName(ruleName);
	pRules->Remove(bName);

	INetFwRule* pRule = nullptr;
	hr = CoCreateInstance(__uuidof(NetFwRule), nullptr, CLSCTX_INPROC_SERVER,
						  __uuidof(INetFwRule), reinterpret_cast<void**>(&pRule));
	if (SUCCEEDED(hr) && pRule)
	{
		pRule->put_Name(bName);
		Bstr bDesc(QStringLiteral("Veyon communication allow rule"));
		pRule->put_Description(bDesc);
		pRule->put_Direction(NET_FW_RULE_DIR_IN);
		pRule->put_Action(NET_FW_ACTION_ALLOW);
		pRule->put_Protocol(6); // 6 = TCP
		Bstr bPorts(localPorts);
		pRule->put_LocalPorts(bPorts);
		pRule->put_Profiles(NET_FW_PROFILE2_ALL);
		pRule->put_Enabled(VARIANT_TRUE);

		hr = pRules->Add(pRule);
		pRule->Release();
	}

	pRules->Release();
	pPolicy->Release();
	return SUCCEEDED(hr);
}


bool WindowsFirewallManager::isAnyProfileDisabledCom()
{
	ScopedCom com;
	if (!com.isOk())
	{
		return false;
	}

	INetFwPolicy2* pPolicy = getPolicy2();
	if (!pPolicy)
	{
		return false;
	}

	const NET_FW_PROFILE_TYPE2 profiles[] = {
		NET_FW_PROFILE2_DOMAIN,
		NET_FW_PROFILE2_PRIVATE,
		NET_FW_PROFILE2_PUBLIC
	};

	bool anyDisabled = false;
	for (auto prof : profiles)
	{
		VARIANT_BOOL enabled = VARIANT_TRUE;
		if (SUCCEEDED(pPolicy->get_FirewallEnabled(prof, &enabled)))
		{
			if (enabled == VARIANT_FALSE)
			{
				anyDisabled = true;
				break;
			}
		}
	}

	pPolicy->Release();
	return anyDisabled;
}


bool WindowsFirewallManager::setProfilesEnabledCom(bool enable)
{
	ScopedCom com;
	if (!com.isOk())
	{
		return false;
	}

	INetFwPolicy2* pPolicy = getPolicy2();
	if (!pPolicy)
	{
		return false;
	}

	const NET_FW_PROFILE_TYPE2 profiles[] = {
		NET_FW_PROFILE2_DOMAIN,
		NET_FW_PROFILE2_PRIVATE,
		NET_FW_PROFILE2_PUBLIC
	};

	VARIANT_BOOL val = enable ? VARIANT_TRUE : VARIANT_FALSE;
	for (auto prof : profiles)
	{
		pPolicy->put_FirewallEnabled(prof, val);
	}

	pPolicy->Release();
	return true;
}


int WindowsFirewallManager::runNetsh(const QStringList& arguments)
{
	QProcess process;
	process.start(QStringLiteral("netsh"), arguments);
	process.waitForFinished(10000);

	const auto exitCode = process.exitCode();
	const auto output = QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError()).trimmed();
	InternetControlLog::write(QStringLiteral("netsh %1 -> exit code %2%3")
							  .arg(arguments.join(QLatin1Char(' ')))
							  .arg(exitCode)
							  .arg(output.isEmpty() ? QString() : QStringLiteral(" [") + output + QStringLiteral("]")));
	return exitCode;
}


void WindowsFirewallManager::ensureFirewallEnabled()
{
	bool anyDisabled = isAnyProfileDisabledCom();
	if (!anyDisabled)
	{
		// check registry fallback
		for (const auto& key : s_firewallProfileKeys)
		{
			QSettings profile(key, QSettings::NativeFormat);
			if (profile.value(QStringLiteral("EnableFirewall"), 1).toInt() == 0)
			{
				anyDisabled = true;
				break;
			}
		}
	}

	if (anyDisabled)
	{
		InternetControlLog::write(QStringLiteral("Windows Firewall was disabled - enabling it temporarily"));

		QSettings state(s_stateKey, QSettings::NativeFormat);
		state.setValue(QStringLiteral("FirewallEnabledByPlugin"), 1);
		state.sync();

		// make sure Veyon itself stays reachable while the firewall is on
		if (!addInboundAllowPortRuleCom(s_allowVeyonRuleName, QStringLiteral("11000-11999")))
		{
			runNetsh({ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
					   QStringLiteral("rule"), QStringLiteral("name=") + s_allowVeyonRuleName });
			runNetsh({ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
					   QStringLiteral("rule"), QStringLiteral("name=") + s_allowVeyonRuleName,
					   QStringLiteral("dir=in"), QStringLiteral("action=allow"), QStringLiteral("protocol=TCP"),
					   QStringLiteral("localport=11000-11999"), QStringLiteral("profile=any") });
		}

		if (!setProfilesEnabledCom(true))
		{
			runNetsh({ QStringLiteral("advfirewall"), QStringLiteral("set"), QStringLiteral("allprofiles"),
					   QStringLiteral("state"), QStringLiteral("on") });
		}
	}
}


void WindowsFirewallManager::restoreFirewallState()
{
	QSettings state(s_stateKey, QSettings::NativeFormat);
	if (state.value(QStringLiteral("FirewallEnabledByPlugin"), 0).toInt() == 1)
	{
		InternetControlLog::write(QStringLiteral("Restoring previous state: disabling Windows Firewall again"));

		if (!setProfilesEnabledCom(false))
		{
			runNetsh({ QStringLiteral("advfirewall"), QStringLiteral("set"), QStringLiteral("allprofiles"),
					   QStringLiteral("state"), QStringLiteral("off") });
		}

		if (!removeRuleCom(s_allowVeyonRuleName))
		{
			runNetsh({ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
					   QStringLiteral("rule"), QStringLiteral("name=") + s_allowVeyonRuleName });
		}

		state.remove(QStringLiteral("FirewallEnabledByPlugin"));
		state.sync();
	}
}


static bool isPublicIPv4(DWORD networkOrderAddress)
{
	const auto a = ntohl(networkOrderAddress);
	const auto b1 = (a >> 24) & 0xff;
	const auto b2 = (a >> 16) & 0xff;

	if (b1 == 0 || b1 == 10 || b1 == 127 || b1 >= 224) return false;
	if (b1 == 169 && b2 == 254) return false;
	if (b1 == 172 && b2 >= 16 && b2 <= 31) return false;
	if (b1 == 192 && b2 == 168) return false;
	return true;
}


int WindowsFirewallManager::killPublicTcpConnections()
{
	ULONG size = 0;
	GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
	if (size == 0)
	{
		return 0;
	}

	std::vector<char> buffer(size);
	auto table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
	if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR)
	{
		return 0;
	}

	int killed = 0;
	for (DWORD i = 0; i < table->dwNumEntries; ++i)
	{
		const auto& row = table->table[i];
		if (row.dwState == MIB_TCP_STATE_ESTAB && isPublicIPv4(row.dwRemoteAddr))
		{
			MIB_TCPROW tcpRow{};
			tcpRow.dwState = MIB_TCP_STATE_DELETE_TCB;
			tcpRow.dwLocalAddr = row.dwLocalAddr;
			tcpRow.dwLocalPort = row.dwLocalPort;
			tcpRow.dwRemoteAddr = row.dwRemoteAddr;
			tcpRow.dwRemotePort = row.dwRemotePort;
			if (SetTcpEntry(&tcpRow) == NO_ERROR)
			{
				++killed;
			}
		}
	}

	InternetControlLog::write(QStringLiteral("Reset %1 established TCP connections to the internet").arg(killed));
	return killed;
}


bool WindowsFirewallManager::restoreInternet()
{
	InternetControlLog::write(QStringLiteral("restoreInternet: removendo regras de firewall"));

	bool okV4 = removeRuleCom(s_blockRuleNameV4);
	bool okV6 = removeRuleCom(s_blockRuleNameV6);
	removeRuleCom(QStringLiteral("Veyon_Internet_Block"));

	// Fallback via netsh somente se a chamada COM falhou
	if (!okV4)
	{
		runNetsh({ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
				   QStringLiteral("rule"), QStringLiteral("name=") + s_blockRuleNameV4 });
	}
	if (!okV6)
	{
		runNetsh({ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
				   QStringLiteral("rule"), QStringLiteral("name=") + s_blockRuleNameV6 });
	}

	restoreFirewallState();
	InternetControlLog::write(QStringLiteral("restoreInternet: firewall liberado"));
	return true;
}


bool WindowsFirewallManager::blockInternet()
{
	ensureFirewallEnabled();

	bool okV4 = addOutboundBlockRuleCom(s_blockRuleNameV4, s_publicIPv4Addresses);
	bool okV6 = addOutboundBlockRuleCom(s_blockRuleNameV6, s_publicIPv6Addresses);

	// Fallback via netsh somente se COM falhou
	if (!okV4)
	{
		runNetsh({ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
				   QStringLiteral("rule"), QStringLiteral("name=") + s_blockRuleNameV4 });
		okV4 = (runNetsh({
			QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
			QStringLiteral("rule"), QStringLiteral("name=") + s_blockRuleNameV4,
			QStringLiteral("dir=out"), QStringLiteral("action=block"),
			QStringLiteral("protocol=any"),
			QStringLiteral("remoteip=") + s_publicIPv4Addresses,
			QStringLiteral("profile=any"), QStringLiteral("enable=yes")
		}) == 0);
	}

	if (!okV6)
	{
		runNetsh({ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
				   QStringLiteral("rule"), QStringLiteral("name=") + s_blockRuleNameV6 });
		okV6 = (runNetsh({
			QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
			QStringLiteral("rule"), QStringLiteral("name=") + s_blockRuleNameV6,
			QStringLiteral("dir=out"), QStringLiteral("action=block"),
			QStringLiteral("protocol=any"),
			QStringLiteral("remoteip=") + s_publicIPv6Addresses,
			QStringLiteral("profile=any"), QStringLiteral("enable=yes")
		}) == 0);
	}

	killPublicTcpConnections();

	return okV4 || okV6;
}
