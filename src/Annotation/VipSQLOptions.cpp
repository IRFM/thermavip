/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipSQLOptions.h"
#include "VipStandardWidgets.h"

#include <QGridLayout>

class VipSQLDataBaseOptionsWidget::PrivateData
{
public:
	QLineEdit hostname;
	QSpinBox port;
	QLineEdit db_name;
	QLineEdit user_name;
	QLineEdit password;
	QToolButton reconnect;
};

VipSQLDataBaseOptionsWidget::VipSQLDataBaseOptionsWidget(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	QGridLayout* lay = new QGridLayout();
	int row = 0;

	d_data->hostname.setToolTip("SQL database hostname");
	d_data->hostname.setMaximumWidth(100);
	lay->addWidget(new QLabel("Hostname"), row, 0);
	lay->addWidget(&d_data->hostname, row++, 1);

	d_data->port.setToolTip("SQL database port");
	d_data->port.setMaximumWidth(100);
	d_data->port.setRange(0, 65535);
	lay->addWidget(new QLabel("Port"), row, 0);
	lay->addWidget(&d_data->port, row++, 1);

	d_data->db_name.setToolTip("SQL database name");
	d_data->db_name.setMaximumWidth(100);
	lay->addWidget(new QLabel("DB name"), row, 0);
	lay->addWidget(&d_data->db_name, row++, 1);

	d_data->user_name.setToolTip("SQL database user name");
	d_data->user_name.setMaximumWidth(100);
	lay->addWidget(new QLabel("Username"), row, 0);
	lay->addWidget(&d_data->user_name, row++, 1);

	d_data->password.setToolTip("SQL database password");
	d_data->password.setMaximumWidth(100);
	d_data->password.setEchoMode(QLineEdit::Password);
	lay->addWidget(new QLabel("Password"), row, 0);
	lay->addWidget(&d_data->password, row++, 1);

	d_data->reconnect.setText("Reconnect");
	d_data->reconnect.setIcon(vipIcon("db.png"));
	d_data->reconnect.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	lay->addWidget(&d_data->reconnect, row++, 0, 1, 2);

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay->addLayout(lay);
	hlay->addStretch(1);

	setLayout(hlay);

	connect(&d_data->reconnect, SIGNAL(clicked(bool)), this, SLOT(reconnect()));
}

VipSQLDataBaseOptionsWidget::~VipSQLDataBaseOptionsWidget() {}

void VipSQLDataBaseOptionsWidget::setHostname(const QString& name)
{
	d_data->hostname.setText(name);
}
QString VipSQLDataBaseOptionsWidget::hostname() const
{
	return d_data->hostname.text();
}

void VipSQLDataBaseOptionsWidget::setPort(int p)
{
	d_data->port.setValue(p);
}
int VipSQLDataBaseOptionsWidget::port() const
{
	return d_data->port.value();
}

void VipSQLDataBaseOptionsWidget::setDataBaseName(const QString& name)
{
	d_data->db_name.setText(name);
}
QString VipSQLDataBaseOptionsWidget::dataBaseName() const
{
	return d_data->db_name.text();
}

void VipSQLDataBaseOptionsWidget::setUserName(const QString& name)
{
	d_data->user_name.setText(name);
}
QString VipSQLDataBaseOptionsWidget::userName() const
{
	return d_data->user_name.text();
}

void VipSQLDataBaseOptionsWidget::setPassword(const QString& p)
{
	d_data->password.setText(p);
}
QString VipSQLDataBaseOptionsWidget::password() const
{
	return d_data->password.text();
}

void VipSQLDataBaseOptionsWidget::reconnect()
{
	vipCreateSQLConnection(hostname(), port(), dataBaseName(), userName(), password());
}






class VipThermalEventDBOptionsWidget::PrivateData
{
public:
	QSpinBox minWidth;
	QSpinBox minHeight;
};

VipThermalEventDBOptionsWidget::VipThermalEventDBOptionsWidget(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	QGridLayout* lay = new QGridLayout();

	d_data->minWidth.setRange(0, 100);
	d_data->minHeight.setRange(0, 100);

	lay->addWidget(new QLabel("BBox minimum size"), 0, 0);
	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay->addWidget(&d_data->minWidth);
	hlay->addWidget(new QLabel("x"));
	hlay->addWidget(&d_data->minHeight);
	hlay->addStretch(1);
	lay->addLayout(hlay, 0, 1);

	setLayout(lay);
}
VipThermalEventDBOptionsWidget::~VipThermalEventDBOptionsWidget() {}

void VipThermalEventDBOptionsWidget::setMinimalEventSize(const QSize& size)
{
	d_data->minWidth.setValue(size.width());
	d_data->minHeight.setValue(size.height());
}
QSize VipThermalEventDBOptionsWidget::minimalEventSize() const
{
	return QSize(d_data->minWidth.value(), d_data->minHeight.value());
}




class VipSQLThermalEventOptions::PrivateData
{
public:
	VipSQLDataBaseOptionsWidget dbOptions;
	VipThermalEventDBOptionsWidget thOptions;
};


VipSQLThermalEventOptions::VipSQLThermalEventOptions(QWidget* parent)
  : VipPageOption(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	QVBoxLayout* lay = new QVBoxLayout();

	lay->addWidget(createOptionGroup("SQL database connection options"));
	lay->addWidget(&d_data->dbOptions);

	lay->addWidget(createOptionGroup("Thermal event database options"));
	lay->addWidget(&d_data->thOptions);
	lay->addStretch(1);

	setLayout(lay);
}

VipSQLThermalEventOptions::~VipSQLThermalEventOptions() {}

VipSQLDataBaseOptionsWidget* VipSQLThermalEventOptions::sqlOptions() const
{
	return &d_data->dbOptions;
}
VipThermalEventDBOptionsWidget* VipSQLThermalEventOptions::thermaEventDBOptions() const
{
	return &d_data->thOptions;
}

/// Apply the settings as entered by the user
void VipSQLThermalEventOptions::applyPage() 
{
	auto opt = vipGetThermalEventDBOptions();
	opt.minimumSize = d_data->thOptions.minimalEventSize();
	vipSetThermalEventDBOptions(opt);
}
/// Update this page based on the acutal settings
void VipSQLThermalEventOptions::updatePage()
{
	QSqlDatabase db = vipGetGlobalSQLConnection();

	d_data->dbOptions.setDataBaseName(db.databaseName());
	d_data->dbOptions.setHostname(db.hostName());
	d_data->dbOptions.setPort(db.port());
	d_data->dbOptions.setUserName(db.userName());
	d_data->dbOptions.setPassword(db.password());

	d_data->thOptions.setMinimalEventSize(vipGetThermalEventDBOptions().minimumSize);
}

static int registerOptions()
{
	vipGetOptions()->addPage("Thermal Event DB", new VipSQLThermalEventOptions(),vipIcon("DB.png"));
	return 0;
}

static int _register = vipAddGuiInitializationFunction(registerOptions);