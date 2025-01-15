/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VIP_OPTIONS_H
#define VIP_OPTIONS_H

#include <qdialog.h>
#include <qgroupbox.h>
#include <qtreewidget.h>

#include "VipConfig.h"

class QTreeWidgetItem;
class QScrollArea;
/// Base class for pages inserted in the global #VipOptions widget
class VIP_GUI_EXPORT VipPageOption : public QWidget
{
	Q_OBJECT
public:
	VipPageOption(QWidget* parent = nullptr)
	  : QWidget(parent)
	{
	}

	static QGroupBox* createOptionGroup(const QString& label);

public Q_SLOTS:
	/// Apply the settings as entered by the user
	virtual void applyPage() = 0;
	/// Update this page based on the acutal settings
	virtual void updatePage() = 0;

protected:
	void showEvent(QShowEvent*) { this->updatePage(); }
};

class VipPageItems : public QTreeWidget
{
	Q_OBJECT
public:
	VipPageItems(QWidget* parent = nullptr);
};

/// A dialog widget that displays general settings of Thermavip.
/// VipOptions should gather all settings editor widgets. It displays on the left side the different setting categories as a tree widget.
/// The settings editor is displayed on the right. Use VipOptions::addPage to ass a new settings editor with a specific category.
/// The settings widget might provide a apply() slot, which will be called inside VipOptions::ok().
class VIP_GUI_EXPORT VipOptions : public QDialog
{
	Q_OBJECT
public:
	VipOptions(QWidget* parent = nullptr);
	~VipOptions();

	bool hasPage(VipPageOption* page) const;
	bool addPage(const QString& category, VipPageOption* page, const QIcon& icon = QIcon());
	void setCurrentPage(VipPageOption* page);
	QScrollArea* areaForPage(VipPageOption* page) const;
	void setTreeWidth(int w);

private Q_SLOTS:
	void ok();
	void itemClicked(QTreeWidgetItem* item, int column);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_GUI_EXPORT VipOptions* vipGetOptions();

//
// A few standard settings pages
//

class AppearanceSettings : public VipPageOption
{
	Q_OBJECT

public:
	AppearanceSettings(QWidget* parent = nullptr);
	~AppearanceSettings();

public Q_SLOTS:
	void applyPage();
	void updatePage();
private Q_SLOTS:
	void skinChanged();

private:
	QPixmap colorMapPixmap(int color_map, const QSize& size);
	QPixmap applyFactor(const QImage& img, int factor);
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class ProcessingSettings : public VipPageOption
{
	Q_OBJECT

public:
	ProcessingSettings(QWidget* parent = nullptr);
	~ProcessingSettings();

public Q_SLOTS:
	void applyPage();
	void updatePage();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class EnvironmentSettings : public VipPageOption
{
	Q_OBJECT

public:
	EnvironmentSettings(QWidget* parent = nullptr);
	~EnvironmentSettings();

public Q_SLOTS:
	void applyPage();
	void updatePage();

private Q_SLOTS:
	void openDataDirectory();
	void openLogDirectory();
	void openLogFile();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class RenderingSettings : public VipPageOption
{
	Q_OBJECT

public:
	RenderingSettings(QWidget* parent = nullptr);
	~RenderingSettings();

public Q_SLOTS:
	void applyPage();
	void updatePage();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
