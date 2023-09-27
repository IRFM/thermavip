#pragma once

#include "VipPlayer.h"

/**
Customize a plot player or a video player.
This class adds a new tool bar button to start scripts stored in #vipGetPythonScriptsPlayerDirectory().
*/
class CustomizePlayer : public QObject
{
	Q_OBJECT

public:
	CustomizePlayer(VipAbstractPlayer * player);
	~CustomizePlayer();

	VipAbstractPlayer * player() const;
	VipPlayer2D * player2D() const { return qobject_cast<VipPlayer2D*>(player()); }
	VipVideoPlayer * videoPlayer() const { return qobject_cast<VipVideoPlayer*>(player()); }
	VipPlotPlayer * plotPlayer() const { return qobject_cast<VipPlotPlayer*>(player()); }

private Q_SLOTS:
	void scriptSelected(QAction *);

private:
	void buildScriptsMenu(QMenu *, bool * found);

	class PrivateData;
	PrivateData * m_data;
};

void customizePlayer(VipAbstractPlayer * player);
