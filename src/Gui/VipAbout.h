#ifndef VIP_ABOUT_H
#define VIP_ABOUT_H

#include "VipDisplayArea.h"
#include <qdialog.h>

/// General about dialog box
class VIP_GUI_EXPORT VipAboutDialog : public QDialog
{
	Q_OBJECT

public:
	VipAboutDialog();
	~VipAboutDialog();

	virtual bool eventFilter(QObject * watched, QEvent * evt);
private:
	class PrivateData;
	PrivateData * m_data;
};



#endif

