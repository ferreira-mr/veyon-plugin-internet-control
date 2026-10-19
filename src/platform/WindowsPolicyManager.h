/*
 * WindowsPolicyManager.h - Manages Edge, Chrome, Brave, Opera and Firefox policies for URL filtering
 *
 * Copyright (c) 2026 Veyon Community
 */

#pragma once

#include <QStringList>

class WindowsPolicyManager
{
public:
	static void applyBlockAll();
	static void applyWhitelist(const QStringList& allowedUrls);
	static void clearPolicies(bool restartBrowsers = true);
	static bool hasActivePolicies();
	static void flushDnsFast();
	static void restartOpenBrowsers();

	// converts user input like "*youtube.com*", "https://*.wikipedia.org/*" or
	// "www.site.com/page" into a plain host (+ optional path), e.g. "wikipedia.org"
	static QString normalizeEntry(const QString& input);

private:
	static void applyCommonBrowserLockdown();
	static void removeAllPolicyKeys();
	static void clearHosts();
};
