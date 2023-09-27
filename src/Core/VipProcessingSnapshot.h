#ifndef VIP_PROCESSING_SNAPSHOT_H
#define VIP_PROCESSING_SNAPSHOT_H


#include "VipIODevice.h"


class VipArchive;

/// VipProcessingSnapshot is a fake VipProcessingObject that represents the state of an actual VipProcessingObject:
/// <ul>
/// <li>Classname, object name, description,...</li>
/// <li>Number of inputs/outputs/properties,</li>
/// <li>Last processing time,</li>
/// <li>Last errors.</li>
/// </ul>
/// It is mainly used to load back a processing pool snapshot through #vipLoadProcessingPoolSnapshot, and can also be used to represent graphically a processing pool into a #VipGraphicsProcessingScene.
///
/// When loading a processing pool snapshot with #vipLoadProcessingPoolSnapshot, this will fill the processing pool with VipProcessingSnapshot objects.
class VIP_CORE_EXPORT VipProcessingSnapshot : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipMultiInput inputs)
	VIP_IO(VipMultiProperty properties)
	VIP_IO(VipMultiOutput outputs)
	friend VIP_CORE_EXPORT bool vipLoadProcessingPoolSnapshot(VipProcessingPool * pool, VipArchive & arch);
public:
	VipProcessingSnapshot(QObject * parent = NULL)
		:VipProcessingObject(parent), m_processingTime(0), m_lastProcessingTime(0) {}
	virtual Info info() const { return m_info; }
	virtual QString inputDescription(const QString & input) const { return m_inputDescriptions[input]; }
	virtual QString outputDescription(const QString & output) const { return m_outputDescriptions[output]; }
	virtual QString propertyDescription(const QString & property) const { return m_propertyDescriptions[property]; }
	virtual qint64 processingTime() const { return m_processingTime; }
	virtual qint64 lastProcessingTime() const { return m_lastProcessingTime; }
private:
	Info m_info;
	QMap<QString, QString> m_inputDescriptions;
	QMap<QString, QString> m_propertyDescriptions;
	QMap<QString, QString> m_outputDescriptions;
	qint64 m_processingTime;
	qint64 m_lastProcessingTime;
};

/// Save a snapshot of a #VipProcessingPool into a #VipArchive.
///
/// A snapshot just describes the different #VipProcessingObject present in the processing pool, with their general information (classname, object name, errors,
/// processing time,...) and their connections.
///
/// Creating a snapshot is usefull to retrieve in a fast way the current state of a processing pool and, for instance, display it in a #VipGraphicsProcessingScene.
/// Since creating a snapshot is very fast, it can be done in real time and, for instance, sent throught the network to another Thermavip that will display it.
///
/// To load back a sbapshot, use #vipLoadProcessingPoolSnapshot.
VIP_CORE_EXPORT bool vipSaveProcessingPoolSnapshot(VipProcessingPool * pool, VipArchive & arch);

/// Load a snapshot previously saved with #vipSaveProcessingPoolSnapshot.
///
/// This function read all #VipProcessingObject in the snapshot and, for each of them:
/// <ul>
/// <li>Look in the processing pool if an instance of #VipProcessingSnapshot with the right object name already exists,</li>
/// <li>If not, create a new one and add it to the processing pool,</li>
/// <li>Retrieve all inputs/outputs/properties from the snapshot and update the #VipProcessingSnapshot accordingly</li>
/// </ul>
///
/// This function is very fast and is designed to be call in real time to display the current state of a distant processing pool.
/// Once the snapshot is loaded into the processing pool, it can be displayed in a #VipGraphicsProcessingScene.
VIP_CORE_EXPORT bool vipLoadProcessingPoolSnapshot(VipProcessingPool * pool, VipArchive & arch);

///Create a binary snapshot of a processing pool.
/// Uses #vipSaveProcessingPoolSnapshot.
VIP_CORE_EXPORT QByteArray vipSaveBinarySnapshot(VipProcessingPool * pool);
///Create a XML snapshot of a processing pool.
/// Uses #vipSaveProcessingPoolSnapshot.
VIP_CORE_EXPORT QString vipSaveXMLSnapshot(VipProcessingPool * pool);
///Load a binary snapshot into a processing pool.
/// Uses #vipLoadProcessingPoolSnapshot.
VIP_CORE_EXPORT bool vipLoadBinarySnapshot(VipProcessingPool * pool, const QByteArray & ar);
///Load a XML snapshot into a processing pool.
/// Uses #vipLoadProcessingPoolSnapshot.
VIP_CORE_EXPORT bool vipLoadXMLSnapshot(VipProcessingPool * pool, const QString & text);


#endif
