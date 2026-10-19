/*
 * InternetControlDialog.h - Teacher configuration dialog for Web Filtering
 *
 * Copyright (c) 2026 Veyon Community
 */

#pragma once

#include <QDialog>
#include <QStringList>

class QPlainTextEdit;

class InternetControlDialog : public QDialog
{
	Q_OBJECT
public:
	explicit InternetControlDialog(QWidget* parent = nullptr);

	QStringList allowedUrls() const;

	static QStringList loadAllowedUrls();
	static void saveAllowedUrls(const QStringList& urls);

private Q_SLOTS:
	void onSaveClicked();
	void onClearClicked();
	void onDefaultPresetClicked();

private:
	void setupUi();

	QPlainTextEdit* m_allowedUrlsEdit;
};
