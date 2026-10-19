/*
 * InternetControlPlugin.cpp - Implementation of InternetControlPlugin
 *
 * Copyright (c) 2026 Veyon Community
 */

#include "InternetControlPlugin.h"
#include "InternetControlDialog.h"
#include "InternetControlLog.h"
#include "platform/WindowsFirewallManager.h"
#include "platform/WindowsPolicyManager.h"

#include "FeatureWorkerManager.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <QThreadPool>

static const QString s_stateKey = QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\VeyonInternetControl");

static bool isServerProcess()
{
	return QFileInfo(QCoreApplication::applicationFilePath()).fileName().compare(
			   QStringLiteral("veyon-server.exe"), Qt::CaseInsensitive) == 0;
}

InternetControlPlugin::InternetControlPlugin(QObject* parent) :
	QObject(parent),
	m_blockFeature(QStringLiteral("InternetBlock"),
				   Feature::Flag::Mode | Feature::Flag::AllComponents,
				   Feature::Uid("7b9c1d2e-3f4a-5b6c-7d8e-9f0a1b2c3d4e"),
				   Feature::Uid(),
				   tr("Bloquear Internet"), tr("Liberar Internet"),
				   tr("Bloqueia a internet dos alunos (bloqueio total se lista vazia, ou apenas sites permitidos)."),
				   QStringLiteral(":/internetcontrol/icons/internet-block.png")),
	m_configureFeature(QStringLiteral("InternetConfigure"),
					   Feature::Flag::Action | Feature::Flag::Master,
					   Feature::Uid("ae2f4a5b-6c7d-8e9f-0a1b-2c3d4e5f6a7b"),
					   Feature::Uid(),
					   tr("Configurar Sites Liberados"), {},
					   tr("Configura a lista de sites autorizados durante o bloqueio da internet."),
					   QStringLiteral(":/internetcontrol/icons/internet-configure.png")),
	m_features({ m_blockFeature, m_configureFeature })
{
	InternetControlLog::write(QStringLiteral("InternetControlPlugin initialized in process PID %1 (%2)")
							  .arg(QCoreApplication::applicationPid())
							  .arg(QCoreApplication::applicationFilePath()));

	// fail-safe: a block left over from a previous server instance (reboot, logoff, crash or a
	// lost "Liberar" command) is removed. If the teacher still has the block enabled, the Veyon
	// Master re-applies it as soon as it reconnects.
	if (isServerProcess())
	{
		if (persistedState() || WindowsPolicyManager::hasActivePolicies())
		{
			InternetControlLog::write(QStringLiteral("VeyonServer startup: stale block or policies found - restoring internet"));
			applyStop();
		}
	}
}

void InternetControlPlugin::runSerialized(const std::function<void()>& task)
{
	// single worker thread: commands are executed in order and veyon-server's event loop is never blocked
	static QThreadPool* pool = [] {
		auto* p = new QThreadPool;
		p->setMaxThreadCount(1);
		p->setExpiryTimeout(-1);
		return p;
	}();
	pool->start(task);
}

void InternetControlPlugin::persistState(bool blocked)
{
	QSettings state(s_stateKey, QSettings::NativeFormat);
	state.setValue(QStringLiteral("Blocked"), blocked ? 1 : 0);
	state.sync();
}

bool InternetControlPlugin::persistedState()
{
	QSettings state(s_stateKey, QSettings::NativeFormat);
	return state.value(QStringLiteral("Blocked"), 0).toInt() == 1;
}

void InternetControlPlugin::applyStart(const QStringList& allowedUrls)
{
	if (m_blocked && m_appliedUrls == allowedUrls)
	{
		InternetControlLog::write(QStringLiteral("StartBlock ignored - identical block already active"));
		return;
	}

	m_blocked = true;
	m_appliedUrls = allowedUrls;
	persistState(true);

	runSerialized([allowedUrls]() {
		if (allowedUrls.isEmpty())
		{
			InternetControlLog::write(QStringLiteral("Bloqueio Total (lista vazia) -> Firewall + Politicas dos Navegadores"));
			WindowsFirewallManager::blockInternet();
			WindowsPolicyManager::applyBlockAll();
		}
		else
		{
			InternetControlLog::write(QStringLiteral("Bloqueio com %1 sites liberados -> politicas dos navegadores").arg(allowedUrls.size()));
			WindowsFirewallManager::restoreInternet();
			WindowsPolicyManager::applyWhitelist(allowedUrls);
		}
		InternetControlLog::write(QStringLiteral("StartBlock finished"));
	});
}

void InternetControlPlugin::applyStop()
{
	const bool hadWhitelist = !m_appliedUrls.isEmpty();
	m_blocked = false;
	m_appliedUrls.clear();
	persistState(false);

	// 1. DESBLOQUEIO INSTANTÂNEO DO FIREWALL:
	// A chamada COM ao Windows Defender Firewall e direta em memoria e leva ~5-10ms.
	// Executando aqui imediatamente de forma sincrona, a internet e desobstruida no
	// exato milissegundo em que a ordem de liberar e recebida pelo aluno!
	WindowsFirewallManager::restoreInternet();
	InternetControlLog::write(QStringLiteral("applyStop: Firewall liberado instantaneamente"));

	// 2. Limpeza complementar em background (politicas, DNS e reinicio imediato dos navegadores abertos):
	runSerialized([hadWhitelist]() {
		Q_UNUSED(hadWhitelist)
		InternetControlLog::write(QStringLiteral("applyStop: executando limpeza de politicas e atualizacao de navegadores"));
		WindowsPolicyManager::clearPolicies(true);
		InternetControlLog::write(QStringLiteral("applyStop: StopBlock concluido"));
	});
}

bool InternetControlPlugin::startFeature(VeyonMasterInterface& master, const Feature& feature,
										 const ComputerControlInterfaceList& computerControlInterfaces)
{
	if (feature == m_configureFeature)
	{
		InternetControlDialog dialog(master.mainWindow());
		if (dialog.exec() == QDialog::Accepted)
		{
			const auto allowedUrls = InternetControlDialog::loadAllowedUrls();
			ComputerControlInterfaceList activeTargets;
			for (const auto& ci : master.allComputerControlInterfaces())
			{
				if (ci && (ci->activeFeatures().contains(m_blockFeature.uid()) ||
						   ci->designatedModeFeature() == m_blockFeature.uid()))
				{
					activeTargets.append(ci);
				}
			}

			if (!activeTargets.isEmpty())
			{
				InternetControlLog::write(QStringLiteral("Master: updating active InternetBlock on %1 computers with %2 allowed URLs")
										  .arg(activeTargets.size())
										  .arg(allowedUrls.size()));
				QVariantMap args;
				args.insert(argToString(Argument::AllowedUrls), allowedUrls);
				controlFeature(m_blockFeature.uid(), Operation::Start, args, activeTargets);
			}
		}
		return true;
	}

	if (feature == m_blockFeature)
	{
		const auto targetInterfaces = computerControlInterfaces.isEmpty()
			? master.filteredComputerControlInterfaces()
			: computerControlInterfaces;

		const auto allowedUrls = InternetControlDialog::loadAllowedUrls();

		InternetControlLog::write(QStringLiteral("Master: startFeature Bloquear Internet for %1 computers with %2 allowed URLs")
								  .arg(targetInterfaces.size())
								  .arg(allowedUrls.size()));

		QVariantMap args;
		args.insert(argToString(Argument::AllowedUrls), allowedUrls);
		return controlFeature(feature.uid(), Operation::Start, args, targetInterfaces);
	}

	return false;
}

bool InternetControlPlugin::stopFeature(VeyonMasterInterface& master, const Feature& feature,
										const ComputerControlInterfaceList& computerControlInterfaces)
{
	if (feature == m_blockFeature)
	{
		const auto targetInterfaces = computerControlInterfaces.isEmpty()
			? master.filteredComputerControlInterfaces()
			: computerControlInterfaces;

		InternetControlLog::write(QStringLiteral("Master: stopFeature Liberar Internet for %1 computers")
								  .arg(targetInterfaces.size()));

		return controlFeature(feature.uid(), Operation::Stop, {}, targetInterfaces);
	}

	return false;
}

bool InternetControlPlugin::controlFeature(Feature::Uid featureUid, Operation operation,
										   const QVariantMap& arguments,
										   const ComputerControlInterfaceList& computerControlInterfaces)
{
	if (hasFeature(featureUid) == false)
	{
		return false;
	}

	QStringList targets;
	for (const auto& ci : computerControlInterfaces)
	{
		if (ci)
		{
			const bool isConnected = (ci->state() == ComputerControlInterface::State::Connected);
			targets.append(QStringLiteral("%1(%2)").arg(ci->computer().hostName()).arg(isConnected ? QStringLiteral("conn") : QStringLiteral("disc")));
		}
	}

	InternetControlLog::write(QStringLiteral("Master: controlFeature %1 op=%2 sending to %3 targets: [%4]")
							  .arg(featureUid.toString())
							  .arg(operation == Operation::Start ? QStringLiteral("Start") : QStringLiteral("Stop"))
							  .arg(targets.size())
							  .arg(targets.join(QStringLiteral(", "))));

	if (featureUid == m_blockFeature.uid())
	{
		if (operation == Operation::Start)
		{
			const auto allowedUrls = arguments.value(argToString(Argument::AllowedUrls)).toStringList();
			sendFeatureMessage(FeatureMessage{featureUid, Command::StartBlock}
							   .addArgument(Argument::AllowedUrls, allowedUrls),
							   computerControlInterfaces);
		}
		else
		{
			sendFeatureMessage(FeatureMessage{featureUid, Command::StopBlock}, computerControlInterfaces);
		}
		return true;
	}

	return false;
}

bool InternetControlPlugin::handleFeatureMessage(VeyonServerInterface& server,
												 const MessageContext& messageContext,
												 const FeatureMessage& message)
{
	Q_UNUSED(server)
	Q_UNUSED(messageContext)

	if (hasFeature(message.featureUid()) == false)
	{
		return false;
	}

	InternetControlLog::write(QStringLiteral("VeyonServer: Received feature message for %1, command=%2")
							  .arg(message.featureUid().toString())
							  .arg(static_cast<int>(message.command<Command>())));

	if (message.featureUid() == m_blockFeature.uid())
	{
		switch (message.command<Command>())
		{
		case Command::StartBlock:
			applyStart(message.argument(Argument::AllowedUrls).toStringList());
			return true;

		case Command::StopBlock:
			applyStop();
			return true;

		default:
			break;
		}
	}

	return false;
}

bool InternetControlPlugin::handleFeatureMessage(VeyonWorkerInterface& worker, const FeatureMessage& message)
{
	Q_UNUSED(worker)
	Q_UNUSED(message)

	// all work is done by veyon-server (SYSTEM privileges); the worker runs as the logged-on user
	return false;
}
bool InternetControlPlugin::isFeatureActive(VeyonServerInterface& server, Feature::Uid featureUid) const
{
	Q_UNUSED(server)

	if (featureUid == m_blockFeature.uid())
	{
		return m_blocked;
	}

	return false;
}
