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


#ifndef VIP_SQL_OPTIONS_H
#define VIP_SQL_OPTIONS_H

#include "VipSqlQuery.h"
#include "VipOptions.h"

/// @brief Small widget providing features to edit a SQL DB connection
class VIP_ANNOTATION_EXPORT VipSQLDataBaseOptionsWidget : public QWidget
{
	Q_OBJECT
public:
	VipSQLDataBaseOptionsWidget(QWidget* parent = nullptr);
	~VipSQLDataBaseOptionsWidget();

	void setHostname(const QString&);
	QString hostname() const;

	void setPort(int);
	int port() const;

	void setDataBaseName(const QString&);
	QString dataBaseName() const;

	void setUserName(const QString&);
	QString userName() const;

	void setPassword(const QString&);
	QString password() const;

public Q_SLOTS:
	void reconnect();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Small widget used to edit thermal event DB options
class VIP_ANNOTATION_EXPORT VipThermalEventDBOptionsWidget : public QWidget
{
	Q_OBJECT
public:
	VipThermalEventDBOptionsWidget(QWidget* parent = nullptr);
	~VipThermalEventDBOptionsWidget();

	void setMinimalEventSize(const QSize& size);
	QSize minimalEventSize() const;

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);

};



class VIP_ANNOTATION_EXPORT VipSQLThermalEventOptions : public VipPageOption
{
	Q_OBJECT

public:
	VipSQLThermalEventOptions(QWidget* parent = nullptr);
	~VipSQLThermalEventOptions();

	VipSQLDataBaseOptionsWidget* sqlOptions() const;
	VipThermalEventDBOptionsWidget* thermaEventDBOptions() const;

	/// Apply the settings as entered by the user
	virtual void applyPage();
	/// Update this page based on the acutal settings
	virtual void updatePage();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif

