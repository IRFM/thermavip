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

#ifndef VIP_SQL_QUERY_H
#define VIP_SQL_QUERY_H

#include "VipCore.h"
#include "VipPolygon.h"
#include "VipProgress.h"
#include "VipSceneModel.h"

#include <QSqlDatabase>

/// @brief Maximum number of points to describe a polygon in the database
#define VIP_DB_MAX_FRAME_POLYGON_POINTS 32

/// @brief Experiment id type
using Vip_experiment_id = qint64;

//////////////////////////////////////////////
// SQL database functions
//////////////////////////////////////////////

/// @brief Wrap a SQL query condition
struct VipRequestCondition
{
	enum Separator
	{
		AND,
		OR
	};

	VipRequestCondition()
	  : sep(AND)
	{
	}
	QString varname;
	QVariant min, max;
	QString equal;
	QStringList enums;
	Separator sep;
};

struct VipThermalEventDBOptions
{
	QSize minimumSize{ QSize(0, 0) };
};

VIP_ANNOTATION_EXPORT QSqlDatabase vipGetGlobalSQLConnection();
VIP_ANNOTATION_EXPORT bool vipCreateSQLConnection(const QString& hostname, int port, const QString& db_name, const QString& user_name, const QString& password);

VIP_ANNOTATION_EXPORT void vipSetThermalEventDBOptions(const VipThermalEventDBOptions&);
VIP_ANNOTATION_EXPORT const VipThermalEventDBOptions& vipGetThermalEventDBOptions() noexcept;

/// @brief Build a SQL query condition based on a column name, a min and max value and a separator
///
/// Example:
/// \code{.cpp}
/// // print: '(max_temperature_C > 400 AND max_temperature_C < 450)'
/// std::cout<< vipFormatRequestCondition(vipRequestCondition("max_temperature_C", 400,450,VipRequestCondition::AND)) << std::endl;
/// \endcode
VIP_ANNOTATION_EXPORT VipRequestCondition vipRequestCondition(const QString& varname, const QVariant& min, const QVariant& max, VipRequestCondition::Separator sep);

/// @brief Build a SQL query condition based on a column name, an enumeration list
///
/// Example:
/// \code{.cpp}
/// // print: '(user = "Max" OR user = "Stan" OR user = "Ben")'
/// std::cout<< vipFormatRequestCondition(vipRequestCondition("user", QStringList()<<"Max"<<"Stan"<<"Ben")) << std::endl;
/// \endcode
VIP_ANNOTATION_EXPORT VipRequestCondition vipRequestCondition(const QString& varname, const QStringList& one_of_enum);

/// @brief Build a SQL query condition based on a column name and a value
///
/// Example:
/// \code{.cpp}
/// // print: '(user = "Max")'
/// std::cout<< vipFormatRequestCondition(vipRequestCondition("user", "Max")) << std::endl;
/// \endcode
VIP_ANNOTATION_EXPORT VipRequestCondition vipRequestCondition(const QString& varname, const QString& equal);

/// @brief Format a VipRequestCondition to string
VIP_ANNOTATION_EXPORT QString vipFormatRequestCondition(const VipRequestCondition& c);

/// @brief Represents a dataset as read from the DB
struct VipDataset
{
	int id;			 // dataset id
	QString creation_date;	 // dataset creation date
	QString annotation_type; // dataset type of annotations
	QString description;	 // dataset short description
};

/// @brief Returns the list of possible cameras from the DB based on the 'lines_of_sight' table
VIP_ANNOTATION_EXPORT QStringList vipCamerasDB();
/// @brief Returns the list of allowed users from the DB based on the 'users' table
VIP_ANNOTATION_EXPORT QStringList vipUsersDB();
/// @brief Returns the list of possible analysis status from the DB based on the 'analysis_status' table
VIP_ANNOTATION_EXPORT QStringList vipAnalysisStatusDB();
/// @brief Returns the list of possible devices from the DB based on the 'devices' table
VIP_ANNOTATION_EXPORT QStringList vipDevicesDB();
/// @brief Returns the possible datasets from the DB based on the 'datasets' table
VIP_ANNOTATION_EXPORT QMap<qsizetype, VipDataset> vipDatasetsDB();
/// @brief Returns the possible annotation methods from the DB based on the 'methods' table
VIP_ANNOTATION_EXPORT QStringList vipMethodsDB();
/// @brief Returns the list of event types from the DB based on the 'thermal_event_categories' table
VIP_ANNOTATION_EXPORT QStringList vipEventTypesDB();
/// @brief Returns the list of event types from the DB based on the 'thermal_event_categories' table and valid for given line of sight
VIP_ANNOTATION_EXPORT QStringList vipEventTypesDB(const QString& line_of_sight);
/// @brief Returns the local folder containing movies as defined in the .env file (field LOCAL_MOVIE_FOLDER)
VIP_ANNOTATION_EXPORT QString vipLocalMovieFolderDB();
/// @brief Returns the movie files suffix from the local folder containing movies as defined in the .env file (field LOCAL_MOVIE_SUFFIX)
VIP_ANNOTATION_EXPORT QString vipLocalMovieSuffix();

VIP_ANNOTATION_EXPORT bool vipHasReadRightsDB();
VIP_ANNOTATION_EXPORT bool vipHasWriteRightsDB();

/// @brief Default list of events type.
/// Stores a map of event_ID -> list of timestamped shapes
typedef QMap<qint64, VipShapeList> Vip_event_list;

/// @brief Returns a copy of input events
VIP_ANNOTATION_EXPORT Vip_event_list vipCopyEvents(const Vip_event_list& events);

/// @brief Remove event from DB based on their ids in the 'thermal_events' table
VIP_ANNOTATION_EXPORT bool vipRemoveFromDB(const QList<qint64>& ids, VipProgress* p = nullptr);

/// @brief Set new value to given column for selected events only
VIP_ANNOTATION_EXPORT bool vipChangeColumnInfoDB(const QList<qint64>& ids, const QString& column, const QString& value, VipProgress* p = nullptr);

/// @brief Send events to DB
/// @param userName Current user name (use vipUserName())
/// @param camera line of sight name
/// @param device device name
/// @param pulse experiment id
/// @param shapes events to record
/// @param p optional progress bar
/// @return list of created ids in the 'thermal_events' table
VIP_ANNOTATION_EXPORT QList<qint64> vipSendToDB(const QString& userName, const QString& camera, const QString& device, Vip_experiment_id pulse, const Vip_event_list& shapes, VipProgress* p = nullptr);

/// @brief Gather information to query the 'thermal_events' table using vipQueryDB()
struct VipEventQuery
{
	QList<qint64> eventIds;	     // list of event ids
	QStringList cameras;	     // possible cameras (all if empty)
	QStringList devices;	     // possible devices (all if empty)
	Vip_experiment_id min_pulse; // minimum pulse (no minimum if -1)
	Vip_experiment_id max_pulse; // maximum pulse (no maximum if -1)
	// int id_thermaleventinfo;
	QString in_comment;	 // sub string to find in comment
	QString in_name;	 // sub string to find in name
	QString method;		 // method if not empty
	QString dataset;	 // dataset name if not empty
	QStringList users;	 // possible users (all if empty)
	qint64 min_duration;	 // minimum duration if not -1
	qint64 max_duration;	 // maximum duration if not -1
	double min_temperature;	 // min maximum temperature if not -1
	double max_temperature;	 // max maximum temperature if not -1
	int automatic;		 // automatic or manual detection if not -1
	double min_confidence;	 // minimum confidence value if not -1
	double max_confidence;	 // maximum confidence value if not -1
	QStringList event_types; // possible event types (all if empty)

	VipEventQuery()
	  : min_pulse(-1)
	  , max_pulse(-1)
	  /*, id_thermaleventinfo(-1)*/
	  , min_duration(-1)
	  , max_duration(-1)
	  , min_temperature(-1)
	  , max_temperature(-1)
	  , automatic(-1)
	  , min_confidence(-1)
	  , max_confidence(-1)
	{
	}
};

/// @brief Unique event result  from a query
struct VipEventQueryResult
{
	VipEventQueryResult()
	  : eventId(-1)
	  , experiment_id(-1)
	  , initialTimestamp(0)
	  , lastTimestamp(0)
	  , duration(0)
	  , dateValidation(0)
	  , automatic(true)
	  , maximum(0)
	  , confidence(0)
	{
	}
	QString comment;
	QString name;
	QString eventName;
	QString camera;
	QString device;
	qint64 eventId;
	Vip_experiment_id experiment_id;
	qint64 initialTimestamp;
	qint64 lastTimestamp;
	qint64 duration;
	qint64 dateValidation;
	bool automatic;
	double maximum;
	double confidence;
	QString analysis_status;
	QString user;
	QString method;
	QString dataset;
	QString error;
	VipShapeList shapes;

	bool isValid() const { return error.isEmpty(); }
};

/// @brief Result of a query using vipQueryDB(const VipEventQuery& query)
/// Stores a map of event id -> VipEventQueryResult
struct VipEventQueryResults
{
	QString error;
	QMap<qint64, VipEventQueryResult> events;

	bool isValid() const { return error.isEmpty(); }
};

/// @brief Query the DB based on a VipEventQuery
///
/// The query is performed only on the 'thermal_events' table.
/// Therfore, the VipEventQueryResult::shapes field is not filled.
/// Returns a VipEventQueryResults storing the map of event_id -> VipEventQueryResult.
///
/// The VipEventQueryResults can be used in vipFullQueryDB() to read actual thermal event instances
/// (which is a lot heavier)
VIP_ANNOTATION_EXPORT VipEventQueryResults vipQueryDB(const VipEventQuery& query, VipProgress* p = nullptr);

struct VipCameraResult
{
	QString cameraName;
	QString device;
	VipEventQueryResults events;
};
struct VipPulseResult
{
	Vip_experiment_id experiment_id;
	QMap<QString, VipCameraResult> cameras;
};
/// @brief Result of a full query using vipFullQueryDB().
///
/// Stores event sorted by experiment id and camera name.
struct VipFullQueryResult
{
	QMap<Vip_experiment_id, VipPulseResult> result;
	QString error;
	int totalEvents; // total number of VipShape
	VipFullQueryResult()
	  : totalEvents(0)
	{
	}
	bool isValid() const { return error.isEmpty(); }
};

/// @brief Performs a full event query on the DB based on a VipEventQueryResults
VIP_ANNOTATION_EXPORT VipFullQueryResult vipFullQueryDB(const VipEventQueryResults& evtres, VipProgress* p = nullptr);

//////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////

/// @brief Extract all events from a VipFullQueryResult struct
VIP_ANNOTATION_EXPORT Vip_event_list vipExtractEvents(const VipFullQueryResult& res);

/// @brief Simplify input polygon in order to have at most max_points.
/// Internally uses vipRDPSimplifyPolygon().
VIP_ANNOTATION_EXPORT QPolygonF vipSimplifyPolygonDB(const QPolygonF& poly, qsizetype max_points);

/// @brief Convert input events to JSON format
VIP_ANNOTATION_EXPORT QByteArray vipEventsToJson(const Vip_event_list& evts, VipProgress* progress = nullptr);
/// @brief Convert input events to JSON format and write to file
VIP_ANNOTATION_EXPORT bool vipEventsToJsonFile(const QString& out_file, const Vip_event_list& evts, VipProgress* progress = nullptr);
/// @brief Read thermal event from a JSON file
VIP_ANNOTATION_EXPORT Vip_event_list vipEventsFromJson(const QByteArray& content);

//////////////////////////////////////////////
// Per device handling
//////////////////////////////////////////////

/// @brief Base virtual class defining how to handle movies for a device
struct VipBaseDeviceParameters
{
	virtual ~VipBaseDeviceParameters() {}
	/// @brief Returns a path based on experiment id and camera name.
	/// This path must be suitable to be opened inside Thermavip.
	virtual QString createDevicePath(Vip_experiment_id experiment_id, const QString& camera) const = 0;
	/// @brief Default video, empty QSize if unknown
	virtual QSize defaultVideoSize() const { return QSize(); }
	/// @brief Returns a newly constructed pulse editor widget.
	/// Usually returns a new VipPulseSpinBox
	virtual QWidget* pulseEditor() const = 0;
};

/// @brief Register a DeviceParameters for given device name
/// @param name device name (like 'WEST', 'W7X'...)
/// @param  DeviceParameters object created with operator new
/// @return true on success
VIP_ANNOTATION_EXPORT bool vipRegisterDeviceParameters(const QString& name, VipBaseDeviceParameters* param);
/// @brief Returns the DeviceParameters associated to given device name.
/// Returns the default DeviceParameters if provided name was not found.
VIP_ANNOTATION_EXPORT const VipBaseDeviceParameters* vipFindDeviceParameters(const QString& name);

//////////////////////////////////////////////
// Widgets
//////////////////////////////////////////////

#include <qlineedit.h>
#include <qmenu.h>
#include <qspinbox.h>
#include <qtoolbutton.h>
#include <qwidget.h>

/// @brief Spinbox working with 64 bits integer
class VIP_ANNOTATION_EXPORT VipLongLongSpinBox : public QAbstractSpinBox
{
	Q_OBJECT

	Q_PROPERTY(qint64 minimum READ minimum WRITE setMinimum)
	Q_PROPERTY(qint64 maximum READ maximum WRITE setMaximum)
	Q_PROPERTY(qint64 value READ value WRITE setValue NOTIFY valueChanged USER true)

	qint64 m_minimum;
	qint64 m_maximum;
	qint64 m_value;

public:
	explicit VipLongLongSpinBox(QWidget* parent = nullptr)
	  : QAbstractSpinBox(parent)
	{
		connect(lineEdit(), SIGNAL(textEdited(const QString&)), this, SLOT(onEditFinished()));
	}
	~VipLongLongSpinBox(){};

	qint64 value() const { return m_value; };

	qint64 minimum() const { return m_minimum; };

	void setMinimum(qint64 min) { m_minimum = min; }

	qint64 maximum() const { return m_maximum; };

	void setMaximum(qint64 max) { m_maximum = max; }

	void setRange(qint64 min, qint64 max)
	{
		setMinimum(min);
		setMaximum(max);
	}

	virtual void stepBy(int steps)
	{
		auto new_value = m_value;
		if (steps < 0 && new_value + steps > new_value) {
			new_value = std::numeric_limits<qint64>::min();
		}
		else if (steps > 0 && new_value + steps < new_value) {
			new_value = std::numeric_limits<qint64>::max();
		}
		else {
			new_value += steps;
		}

		lineEdit()->setText(textFromValue(new_value));
		setValue(new_value);
	}

	QLineEdit* lineEdit() const { return QAbstractSpinBox::lineEdit(); }

protected:
	// bool event(QEvent *event);
	virtual QValidator::State validate(QString& input, int&) const
	{
		bool ok;
		qint64 val = input.toLongLong(&ok);
		if (!ok)
			return QValidator::Invalid;

		if (val < m_minimum || val > m_maximum)
			return QValidator::Invalid;

		return QValidator::Acceptable;
	}

	virtual qint64 valueFromText(const QString& text) const { return text.toLongLong(); }

	virtual QString textFromValue(qint64 val) const { return QString::number(val); }
	// virtual void fixup(QString &str) const;

	virtual QAbstractSpinBox::StepEnabled stepEnabled() const { return StepUpEnabled | StepDownEnabled; }

public Q_SLOTS:
	void setValue(qint64 val)
	{
		if (m_value != val) {
			lineEdit()->setText(textFromValue(val));
			m_value = val;
		}
	}

	void onEditFinished()
	{
		QString input = lineEdit()->text();
		int pos = 0;
		if (QValidator::Acceptable == validate(input, pos))
			setValue(valueFromText(input));
		else
			lineEdit()->setText(textFromValue(m_value));
	}

Q_SIGNALS:
	void valueChanged(qint64 v);
};

/// @brief Integer experiment id editor, used for WEST and W7-X
class VIP_ANNOTATION_EXPORT VipPulseSpinBox : public VipLongLongSpinBox
{
	Q_OBJECT

public:
	VipPulseSpinBox(QWidget* parent = nullptr)
	  : VipLongLongSpinBox(parent)
	{
		setMaximumWidth(150);
		setRange(0, std::numeric_limits<qint64>::max());
		setValue(0);
		setToolTip(tr("Select experiment id"));
		setLocale(QLocale(QLocale::C));
	}
};

/// @brief Tool button used to select a dataset
class VIP_ANNOTATION_EXPORT VipDatasetButton : public QToolButton
{
	Q_OBJECT

public:
	VipDatasetButton(QWidget* parent = nullptr);
	~VipDatasetButton();

	QString dataset() const;

	int datasetCount() const;
	QString datasetName(int index) const;
	bool datasetChecked(int index) const;
	void setChecked(int index, bool checked);

public Q_SLOTS:
	void setDataset(const QString& str);
	void checkAll(bool);

	void aboutToShow();

private Q_SLOTS:
	void emitChanged() { Q_EMIT changed(); }
Q_SIGNALS:
	void changed();

protected:
	virtual void showEvent(QShowEvent* evt);

private:
	void init();
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Vertical widget used to query DB for events
class VIP_ANNOTATION_EXPORT VipQueryDBWidget : public QWidget
{
	Q_OBJECT

public:
	VipQueryDBWidget(const QString& device, QWidget* widget = nullptr);
	~VipQueryDBWidget();

	void enablePulseRange(bool);
	bool pulseRangeEnabled() const;

	void enableAllCameras(bool);
	bool isAllCamerasEnabled() const;

	void enableAllDevices(bool enable);
	bool isAllDevicesEnabled() const;

	void setRemovePreviousVisible(bool);
	bool isRemovepreviousVisible() const;

	void setRemovePrevious(bool);
	bool removePrevious() const;

	void setPulseRange(const QPair<Vip_experiment_id, Vip_experiment_id>& range);
	void setPulse(Vip_experiment_id p) { setPulseRange(QPair<Vip_experiment_id, Vip_experiment_id>(p, p)); }
	QPair<Vip_experiment_id, Vip_experiment_id> pulseRange() const;

	void setIDThermalEventInfo(int);
	int idThermalEventInfo() const;

	void setUserName(const QString& name);
	QString userName() const;

	void setCamera(const QString& camera);
	QString camera() const;

	void setDevice(const QString& device);
	QString device() const;

	void setDataset(const QString& dataset);
	QString dataset() const;

	void setInComment(const QString& comment);
	QString inComment() const;

	void setInName(const QString& name);
	QString inName() const;

	void setMethod(const QString& method);
	QString method() const;

	void setDurationRange(const QPair<qint64, qint64>& range);
	QPair<qint64, qint64> durationRange() const;

	void setMaxTemperatureRange(const QPair<double, double>& range);
	QPair<double, double> maxTemperatureRange() const;

	void setAutomatic(int);
	int automatic() const;

	void setMinConfidence(double);
	double minConfidence() const;

	void setMaxConfidence(double);
	double maxConfidence() const;

	void setThermalEvent(const QString&);
	QString thermalEvent() const;

private Q_SLOTS:
	void pulseChanged(Vip_experiment_id);
	void deviceChanged();

public:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
