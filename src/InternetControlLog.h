/*
 * InternetControlLog.h - simple file logger for diagnosing the plugin on student computers
 *
 * Log file: C:\ProgramData\VeyonInternetControl\internetcontrol.log
 *
 * Copyright (c) 2026 Veyon Community
 */

#pragma once

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTextStream>

namespace InternetControlLog
{

inline QString logDirectory()
{
	return QStringLiteral("C:/ProgramData/VeyonInternetControl");
}

inline void write(const QString& message)
{
	QDir().mkpath(logDirectory());

	QFile file(logDirectory() + QStringLiteral("/internetcontrol.log"));

	// keep log file small
	if (file.size() > 1024 * 1024)
	{
		file.remove();
	}

	if (file.open(QFile::Append | QFile::Text))
	{
		QTextStream stream(&file);
		stream << QDateTime::currentDateTime().toString(Qt::ISODate) << "  " << message << "\n";
	}
}

}
