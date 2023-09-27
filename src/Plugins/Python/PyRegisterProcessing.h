#pragma once

#include "VipProcessingObject.h"

#include <qwidget.h>

class PyRegisterProcessing
{
public:
	/**
	Save given processings in the Python custom processing XML file.
	*/
	static bool saveCustomProcessings(const QList<VipProcessingObject::Info> & infos);
	static bool saveCustomProcessings();

	static QList<VipProcessingObject::Info> customProcessing();

	/**
	Load the custom processing from the XML file and add them to the global VipProcessingObject system.
	If \a overwrite is true, this will overwrite already existing processing with the same name and category.
	*/
	static int loadCustomProcessings(bool overwrite);

	/**
	Open the processing manager
	*/
	static void openProcessingManager();
};


class QPushButton;
class QToolButton;
/**
A simple horizontal widget used by the different Python processing editors
*/
class PyApplyToolBar : public QWidget
{
	Q_OBJECT

public:
	PyApplyToolBar(QWidget * parent = NULL);
	~PyApplyToolBar();

	QPushButton *applyButton() const;
	QToolButton * registerButton() const;
	QToolButton * manageButton() const;

private:
	class PrivateData;
	PrivateData * m_data;
};


class QTreeWidgetItem;
/**
Manage registered PySignalFusionProcessing and PyProcessing custom processings
*/
class PySignalFusionProcessingManager : public QWidget
{
	Q_OBJECT

public:
	PySignalFusionProcessingManager(QWidget *parent = NULL);
	~PySignalFusionProcessingManager();

	QString name() const;
	QString category() const;
	QString description() const;

	void setName(const QString &);
	void setCategory(const QString &);
	void setDescription(const QString &);

	void setManagerVisible(bool);
	bool managerVisible() const;

	void setCreateNewVisible(bool);
	bool createNewVisible() const;

	public Q_SLOTS:
	void updateWidget();
	bool applyChanges();
	private Q_SLOTS:
	void removeSelection();
	void itemClicked(QTreeWidgetItem *, int);
	void itemDoubleClicked(QTreeWidgetItem *, int);
	void descriptionChanged();
	void selectionChanged();
	void showMenu(const QPoint &);
protected:
	virtual void keyPressEvent(QKeyEvent *);

	class PrivateData;
	PrivateData * m_data;
};