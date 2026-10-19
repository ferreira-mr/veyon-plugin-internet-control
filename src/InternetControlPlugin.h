/*
 * InternetControlPlugin.h - Declaration of InternetControlPlugin class
 *
 * Copyright (c) 2026 Veyon Community
 */

#pragma once

#include "FeatureProviderInterface.h"
#include "PluginInterface.h"

#include <functional>

class InternetControlPlugin : public QObject, public FeatureProviderInterface, public PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.InternetControl")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)

public:
	enum class Command
	{
		StartBlock,
		StopBlock
	};
	Q_ENUM(Command)

	enum class Argument
	{
		AllowedUrls
	};
	Q_ENUM(Argument)

	explicit InternetControlPlugin(QObject* parent = nullptr);
	~InternetControlPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("1f9c3d4a-5b6c-7e8f-9a0b-1c2d3e4f5a6b") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber(1, 0);
	}

	QString name() const override
	{
		return QStringLiteral("InternetControl");
	}

	QString description() const override
	{
		return tr("Controle e Bloqueio de Internet para Alunos");
	}

	QString vendor() const override
	{
		return QStringLiteral("Veyon Community");
	}

	QString copyright() const override
	{
		return QStringLiteral("Copyright (c) 2026");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature(Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						const ComputerControlInterfaceList& computerControlInterfaces) override;

	bool startFeature(VeyonMasterInterface& master, const Feature& feature,
					  const ComputerControlInterfaceList& computerControlInterfaces) override;

	bool stopFeature(VeyonMasterInterface& master, const Feature& feature,
					 const ComputerControlInterfaceList& computerControlInterfaces) override;

	bool handleFeatureMessage(VeyonServerInterface& server, const MessageContext& messageContext,
							  const FeatureMessage& message) override;

	bool handleFeatureMessage(VeyonWorkerInterface& worker, const FeatureMessage& message) override;

	bool isFeatureActive(VeyonServerInterface& server, Feature::Uid featureUid) const override;

private:
	void applyStart(const QStringList& allowedUrls);
	void applyStop();
	static void runSerialized(const std::function<void()>& task);
	static void persistState(bool blocked);
	static bool persistedState();

	mutable bool m_blocked{false};
	QStringList m_appliedUrls;

	const Feature m_blockFeature;
	const Feature m_configureFeature;
	const FeatureList m_features;
};
