#ifndef VIP_WIDGET_RESIZER_H
#define VIP_WIDGET_RESIZER_H

#include <qwidget.h>

#include "VipConfig.h"
 
class VIP_GUI_EXPORT VipWidgetResizer : public QObject
{
	Q_OBJECT
	friend class VipWidgetResizerHandler;

public:
	VipWidgetResizer(QWidget* parent);
	~VipWidgetResizer();

	QWidget* parent() const;

	void setBounds(int inner_detect, int end_detect);
	int innerDetect() const;
	int outerDetect() const;

	void setEnabled(bool);
	bool isEnabled() const;

	void enableOutsideParent(bool);
	bool outsideParentEnabled() const;

protected:
	virtual bool isTopLevelWidget(const QPoint& screen_pos = QPoint()) const;
	bool filter(QObject* watched, QEvent* event);

private Q_SLOTS:
	void updateCursor();

private:
	void addCursor();
	void removeCursors();
	bool hasCustomCursor() const;
	QPoint validPosition(const QPoint& pt, bool* ok = NULL) const;
	QSize validSize(const QSize& s, bool* ok = NULL) const;
	class PrivateData;
	PrivateData* m_data;
};


#endif