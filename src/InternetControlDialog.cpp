/*
 * InternetControlDialog.cpp - Implementation of Teacher configuration dialog
 *
 * Copyright (c) 2026 Veyon Community
 */

#include "InternetControlDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QDialogButtonBox>

static const QString SETTINGS_GROUP = QStringLiteral("InternetControlPlugin");
static const QString KEY_ALLOWED_URLS = QStringLiteral("AllowedUrls");

static const QStringList DEFAULT_ALLOWED_URLS = {
	QStringLiteral("wikipedia.org"),
	QStringLiteral("khanacademy.org"),
	QStringLiteral("moodle.org"),
	QStringLiteral("gov.br")
};

InternetControlDialog::InternetControlDialog(QWidget* parent) :
	QDialog(parent),
	m_allowedUrlsEdit(nullptr)
{
	setWindowTitle(tr("Configurar Sites Liberados - Veyon"));
	resize(560, 440);
	setupUi();
}

void InternetControlDialog::setupUi()
{
	auto* mainLayout = new QVBoxLayout(this);

	// Header instructions
	auto* headerLabel = new QLabel(
		tr("<h3>Controle de Bloqueio de Internet</h3>"
		   "<p>Defina abaixo os sites que os alunos <b>poderão acessar</b> quando o bloqueio de internet estiver ativado.<br>"
		   "Digite um site por linha (exemplo: <i>wikipedia.org</i> ou <i>moodle.minhaescola.edu.br</i>).</p>"
		   "<p style='color: #c0392b;'><b>Regra de Bloqueio:</b> Se esta lista estiver <u>vazia</u>, o bloqueio será <b>TOTAL (100% desconectado)</b>.</p>"),
		this);
	headerLabel->setWordWrap(true);
	mainLayout->addWidget(headerLabel);

	// Preset buttons
	auto* presetLayout = new QHBoxLayout();
	auto* clearBtn = new QPushButton(tr("🔴 Bloqueio Total (Limpar Lista)"), this);
	connect(clearBtn, &QPushButton::clicked, this, &InternetControlDialog::onClearClicked);
	presetLayout->addWidget(clearBtn);

	auto* presetBtn = new QPushButton(tr("🟢 Modelo de Prova (Wikipedia, Moodle, Gov.br)"), this);
	connect(presetBtn, &QPushButton::clicked, this, &InternetControlDialog::onDefaultPresetClicked);
	presetLayout->addWidget(presetBtn);

	mainLayout->addLayout(presetLayout);

	// Editor
	mainLayout->addWidget(new QLabel(tr("<b>Sites Liberados durante o Bloqueio:</b>"), this));
	m_allowedUrlsEdit = new QPlainTextEdit(this);
	m_allowedUrlsEdit->setPlaceholderText(
		tr("Deixe esta caixa em branco para Bloqueio Total\nOU informe os sites permitidos, um por linha:\nwikipedia.org\nkhanacademy.org\nmoodle.org\ngov.br"));

	const auto currentUrls = loadAllowedUrls();
	m_allowedUrlsEdit->setPlainText(currentUrls.join(QLatin1Char('\n')));
	mainLayout->addWidget(m_allowedUrlsEdit, 1);

	// Buttons
	auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &InternetControlDialog::onSaveClicked);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	mainLayout->addWidget(buttonBox);
}

QStringList InternetControlDialog::allowedUrls() const
{
	return m_allowedUrlsEdit->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

void InternetControlDialog::onSaveClicked()
{
	saveAllowedUrls(allowedUrls());
	accept();
}

void InternetControlDialog::onClearClicked()
{
	m_allowedUrlsEdit->clear();
}

void InternetControlDialog::onDefaultPresetClicked()
{
	m_allowedUrlsEdit->setPlainText(DEFAULT_ALLOWED_URLS.join(QLatin1Char('\n')));
}

QStringList InternetControlDialog::loadAllowedUrls()
{
	QSettings settings;
	settings.beginGroup(SETTINGS_GROUP);

	if (!settings.contains(KEY_ALLOWED_URLS))
	{
		settings.endGroup();
		return DEFAULT_ALLOWED_URLS;
	}

	auto list = settings.value(KEY_ALLOWED_URLS).toStringList();
	settings.endGroup();
	return list;
}

void InternetControlDialog::saveAllowedUrls(const QStringList& urls)
{
	QSettings settings;
	settings.beginGroup(SETTINGS_GROUP);
	settings.setValue(KEY_ALLOWED_URLS, urls);
	settings.endGroup();
}
