/*
 * WindowsFirewallManager.h - Manages Windows Defender Firewall rules for Veyon Internet Control
 *
 * Copyright (c) 2026 Veyon Community
 */

#pragma once

#include <QString>
#include <QStringList>

class WindowsFirewallManager
{
public:
	static bool blockInternet();
	static bool restoreInternet();
	static int killPublicTcpConnections();

private:
	static int runNetsh(const QStringList& arguments);
	static void ensureFirewallEnabled();
	static void restoreFirewallState();

	// Native Windows Defender Firewall COM methods (instant, zero process creation)
	static bool removeRuleCom(const QString& ruleName);
	static bool addOutboundBlockRuleCom(const QString& ruleName, const QString& remoteAddresses);
	static bool addInboundAllowPortRuleCom(const QString& ruleName, const QString& localPorts);
	static bool isAnyProfileDisabledCom();
	static bool setProfilesEnabledCom(bool enable);

	static const QString s_blockRuleNameV4;
	static const QString s_blockRuleNameV6;
	static const QString s_allowVeyonRuleName;
};
