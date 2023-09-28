#ifndef VIP_OPTIONS_H
#define VIP_OPTIONS_H

#include <qdialog.h>
#include <qtreewidget.h>
#include <qgroupbox.h>

#include "VipConfig.h"

class QTreeWidgetItem;
class QScrollArea;
/// Base class for pages inserted in the global #VipOptions widget
class VIP_GUI_EXPORT VipPageOption : public QWidget
{
	Q_OBJECT
public:
	VipPageOption(QWidget * parent = NULL)
		:QWidget(parent)
	{}

	static QGroupBox * createOptionGroup(const QString& label);

public Q_SLOTS:
	///Apply the settings as entered by the user
	virtual void applyPage() = 0;
	///Update this page based on the acutal settings
	virtual void updatePage() = 0;
protected:
	void  showEvent(QShowEvent *)
	{
		this->updatePage();
	}
};

class VipPageItems : public QTreeWidget
{
	Q_OBJECT
public:
	VipPageItems(QWidget * parent = NULL)
		:QTreeWidget(parent) {}
};

/// A dialog widget that displays general settings of Thermavip.
/// VipOptions should gather all settings editor widgets. It displays on the left side the different setting categories as a tree widget.
/// The settings editor is displayed on the right. Use VipOptions::addPage to ass a new settings editor with a specific category.
/// The settings widget might provide a apply() slot, which will be called inside VipOptions::ok().
class VIP_GUI_EXPORT VipOptions : public QDialog
{
	Q_OBJECT
public:
	VipOptions(QWidget * parent = NULL);
	~VipOptions();

	bool hasPage(VipPageOption * page) const;
	bool addPage(const QString & category, VipPageOption * page, const QIcon & icon = QIcon());
	void setCurrentPage(VipPageOption * page);
	QScrollArea * areaForPage(VipPageOption * page) const;
	void setTreeWidth(int w);

private Q_SLOTS:
	void ok();
	void itemClicked(QTreeWidgetItem * item, int column);

private:
	class PrivateData;
	PrivateData * m_data;
};


VIP_GUI_EXPORT VipOptions * vipGetOptions();


//
// A few standard settings pages
//


class AppearanceSettings : public VipPageOption
{
	Q_OBJECT

public:
	AppearanceSettings(QWidget * parent = NULL);
	~AppearanceSettings();

public Q_SLOTS:
	void applyPage();
	void updatePage();
private Q_SLOTS:
	void skinChanged();
private:
	QPixmap colorMapPixmap(int color_map, const QSize & size);
	QPixmap applyFactor(const QImage & img, int factor);
	class PrivateData;
	PrivateData * m_data;
};



class ProcessingSettings : public VipPageOption
{
	Q_OBJECT

public:
	ProcessingSettings(QWidget * parent = NULL);
	~ProcessingSettings();

public Q_SLOTS:
	void applyPage();
	void updatePage();

private:
	class PrivateData;
	PrivateData * m_data;
};


class EnvironmentSettings : public VipPageOption
{
	Q_OBJECT

public:
	EnvironmentSettings(QWidget * parent = NULL);
	~EnvironmentSettings();

public Q_SLOTS:
	void applyPage();
	void updatePage();

private Q_SLOTS:
	void openDataDirectory();
	void openLogDirectory();
	void openLogFile();
private:
	class PrivateData;
	PrivateData * m_data;
};



class RenderingSettings : public VipPageOption
{
	Q_OBJECT

public:
	RenderingSettings(QWidget* parent = NULL);
	~RenderingSettings();

public Q_SLOTS:
	void applyPage();
	void updatePage();

private:
	class PrivateData;
	PrivateData* m_data;
};


#endif
