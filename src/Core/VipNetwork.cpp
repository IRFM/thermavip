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

#include "VipNetwork.h"

#include <QCoreApplication>
#include <qdatetime.h>
#include <qprocess.h>

static void registerVipNetworkConnection()
{
	static QMutex mutex;
	QMutexLocker lock(&mutex);

	static bool reg = 0;
	if (!reg) {
		qRegisterMetaType<QAbstractSocket::PauseModes>("QAbstractSocket::PauseModes");
		qRegisterMetaType<QNetworkProxy>("QNetworkProxy");
		qRegisterMetaType<QAbstractSocket::SocketOption>("QAbstractSocket::SocketOption");
		qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
		qRegisterMetaType<QAbstractSocket::OpenMode>("QAbstractSocket::OpenMode");
		qRegisterMetaType<QAbstractSocket::BindMode>("QAbstractSocket::BindMode");
		qRegisterMetaType<QAbstractSocket::NetworkLayerProtocol>("QAbstractSocket::NetworkLayerProtocol");
		qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
		qRegisterMetaType<qintptr>("qintptr");
		reg = 1;
	}
}

bool vipPing(const QByteArray& host)
{
	QProcess proc;
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
	proc.setReadChannelMode(QProcess::MergedChannels);
#else
	proc.setProcessChannelMode(QProcess::MergedChannels);
#endif
#ifdef WIN32
	proc.start("ping",
		   QStringList() << host << "/n"
				 << "1" << "/w"<< "2",
		   QIODevice::ReadOnly);
#else
	proc.start("ping",
		   QStringList() << host << "-c"
				 << "1" << "-w" << "2",
		   QIODevice::ReadOnly);
#endif
	proc.waitForStarted();
	proc.waitForFinished();
	// QByteArray ar = proc.readAllStandardOutput();
	// printf("out: %s\n", ar.data());
	return proc.exitCode() == 0;
}

namespace detail
{

	VipNetworkConnectionPrivate::VipNetworkConnectionPrivate(VipNetworkConnection* con)
	  : socket(nullptr)
	  , connection(con)
	  , port(0)
	  , openMode(QAbstractSocket::NotOpen)
	  , protocol(QAbstractSocket::AnyIPProtocol)
	  , autoReconnection(false)
	{
	}

	bool VipNetworkConnectionPrivate::bind(const QHostAddress& address, quint16 _port, QAbstractSocket::BindMode mode)
	{
		QMutexLocker lock(&mutex);
		return socket->bind(address, _port, mode);
	}

	bool VipNetworkConnectionPrivate::waitForConnected(int msecs)
	{
		QMutexLocker lock(&mutex);
		return socket->waitForConnected(msecs);
	}

	bool VipNetworkConnectionPrivate::waitForDisconnected(int msecs)
	{
		QMutexLocker lock(&mutex);
		;
		return socket->waitForDisconnected(msecs);
	}

	bool VipNetworkConnectionPrivate::waitForReadyRead(int msecs)
	{
		QMutexLocker lock(&mutex);
		;
		return socket->waitForReadyRead(msecs);
	}

	bool VipNetworkConnectionPrivate::waitForBytesWritten(int msecs)
	{
		QMutexLocker lock(&mutex);
		;
		return socket->waitForBytesWritten(msecs);
	}

	void VipNetworkConnectionPrivate::resume()
	{
		QMutexLocker lock(&mutex);
		socket->resume();
	}
	void VipNetworkConnectionPrivate::setPauseMode(QAbstractSocket::PauseModes pauseMode)
	{
		QMutexLocker lock(&mutex);
		socket->setPauseMode(pauseMode);
	}

	void VipNetworkConnectionPrivate::setProxy(const QNetworkProxy& networkProxy)
	{
		QMutexLocker lock(&mutex);
		socket->setProxy(networkProxy);
	}

	void VipNetworkConnectionPrivate::setReadBufferSize(qint64 size)
	{
		QMutexLocker lock(&mutex);
		socket->setReadBufferSize(size);
	}

	void VipNetworkConnectionPrivate::setSocketOption(QAbstractSocket::SocketOption option, const QVariant& value)
	{
		QMutexLocker lock(&mutex);
		socket->setSocketOption(option, value);
	}

	qint64 VipNetworkConnectionPrivate::write(const QByteArray& ar)
	{
		QMutexLocker lock(&mutex);
		return socket->write(ar);
	}

	QByteArray VipNetworkConnectionPrivate::read(qint64 len)
	{
		QMutexLocker lock(&mutex);
		return socket->read(len);
	}

	QByteArray VipNetworkConnectionPrivate::readAll()
	{
		QMutexLocker lock(&mutex);
		return socket->readAll();
	}

	void VipNetworkConnectionPrivate::abort()
	{
		QMutexLocker lock(&mutex);
		socket->abort();
	}

	void VipNetworkConnectionPrivate::setSocketDescriptor(qintptr socketDescriptor, QAbstractSocket::SocketState socketState, QAbstractSocket::OpenMode _openMode)
	{
		QMutexLocker lock(&mutex);
		socket->setSocketDescriptor(socketDescriptor, socketState, _openMode);
	}

	void VipNetworkConnectionPrivate::connectToHost(const QString& hostName, quint16 _port, QAbstractSocket::OpenMode _openMode, QAbstractSocket::NetworkLayerProtocol _protocol)
	{
		QMutexLocker lock(&mutex);
		this->host = hostName;
		this->port = _port;
		this->openMode = _openMode;
		this->protocol = _protocol;
		socket->connectToHost(hostName, _port, _openMode, _protocol);
	}

	void VipNetworkConnectionPrivate::disconnectFromHost()
	{
		QMutexLocker lock(&mutex);
		socket->disconnectFromHost();
	}

	void VipNetworkConnectionPrivate::close()
	{
		QMutexLocker lock(&mutex);
		socket->close();
	}

	void VipNetworkConnectionPrivate::onReadyRead()
	{
		if (VipNetworkConnection* c = connection)
			c->onReadyRead();
	}

}

VipNetworkConnection::VipNetworkConnection(QObject* parent)
  : QThread(parent)
  , d_data(nullptr)
  , m_pending(0)
{
	registerVipNetworkConnection();

	start();

	// wait for socket to be created
	while (!d_data || !d_data->socket)
		QThread::msleep(1);
}

VipNetworkConnection::VipNetworkConnection(qintptr descriptor, QObject* parent)
  : QThread(parent)
  , d_data(nullptr)
  , m_pending(descriptor)
{
	registerVipNetworkConnection();

	start();

	// wait for socket to be created
	while (!d_data || !d_data->socket)
		QThread::msleep(1);
}

VipNetworkConnection::~VipNetworkConnection()
{
	setAutoReconnection(false);
	d_data->connection = nullptr;
	quit();
	wait();
}

QAbstractSocket* VipNetworkConnection::socket() const
{
	return d_data->socket;
}

void VipNetworkConnection::run()
{
	d_data = new detail::VipNetworkConnectionPrivate(this);
	d_data->socket = createSocket();
	if (m_pending) {
		d_data->socket->setSocketDescriptor(m_pending);
		d_data->socket->open(QIODevice::ReadWrite);
		m_pending = 0;
	}

	connect(d_data->socket, SIGNAL(readyRead()), d_data, SLOT(onReadyRead()), Qt::DirectConnection);
	connect(d_data->socket, SIGNAL(readyRead()), this, SLOT(_emit_readyRead()), Qt::DirectConnection);
	connect(d_data->socket, SIGNAL(connected()), this, SLOT(_emit_connected()), Qt::DirectConnection);
	connect(d_data->socket, SIGNAL(disconnected()), this, SLOT(_emit_disconnected()), Qt::DirectConnection);
	connect(d_data->socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SLOT(_emit_error(QAbstractSocket::SocketError)), Qt::DirectConnection);
	connect(d_data->socket, SIGNAL(hostFound()), this, SLOT(_emit_hostFound()), Qt::DirectConnection);
	connect(d_data->socket,
		SIGNAL(proxyAuthenticationRequired(const QNetworkProxy&, QAuthenticator*)),
		this,
		SLOT(_emit_proxyAuthenticationRequired(const QNetworkProxy&, QAuthenticator*)),
		Qt::DirectConnection);
	connect(d_data->socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(_emit_stateChanged(QAbstractSocket::SocketState)), Qt::DirectConnection);

	exec();

	d_data->socket->disconnectFromHost();
	if (d_data->socket->state() != QAbstractSocket::UnconnectedState)
		d_data->socket->waitForDisconnected();
	delete d_data->socket;
	delete d_data;
}

void VipNetworkConnection::resume()
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "resume", Qt::BlockingQueuedConnection);
	else
		d_data->resume();
}
void VipNetworkConnection::setPauseMode(QAbstractSocket::PauseModes pauseMode)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "setPauseMode", Qt::BlockingQueuedConnection, Q_ARG(QAbstractSocket::PauseModes, pauseMode));
	else
		d_data->setPauseMode(pauseMode);
}
void VipNetworkConnection::setProxy(const QNetworkProxy& networkProxy)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "setProxy", Qt::BlockingQueuedConnection, Q_ARG(QNetworkProxy, networkProxy));
	else
		d_data->setProxy(networkProxy);
}
void VipNetworkConnection::setReadBufferSize(qint64 size)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "setReadBufferSize", Qt::BlockingQueuedConnection, Q_ARG(qint64, size));
	else
		d_data->setReadBufferSize(size);
}
void VipNetworkConnection::setSocketOption(QAbstractSocket::SocketOption option, const QVariant& value)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "setSocketOption", Qt::BlockingQueuedConnection, Q_ARG(QAbstractSocket::SocketOption, option), Q_ARG(QVariant, value));
	else
		d_data->setSocketOption(option, value);
}

void VipNetworkConnection::setSocketDescriptor(qintptr socketDescriptor, QAbstractSocket::SocketState socketState, QAbstractSocket::OpenMode openMode)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data,
					  "setSocketDescriptor",
					  Qt::BlockingQueuedConnection,
					  Q_ARG(qintptr, socketDescriptor),
					  Q_ARG(QAbstractSocket::SocketState, socketState),
					  Q_ARG(QAbstractSocket::OpenMode, openMode));
	else
		d_data->setSocketDescriptor(socketDescriptor, socketState, openMode);
}

void VipNetworkConnection::connectToHost(const QString& hostName, quint16 port, QAbstractSocket::OpenMode openMode, QAbstractSocket::NetworkLayerProtocol protocol)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data,
					  "connectToHost",
					  Qt::BlockingQueuedConnection,
					  Q_ARG(QString, hostName),
					  Q_ARG(quint16, port),
					  Q_ARG(QAbstractSocket::OpenMode, openMode),
					  Q_ARG(QAbstractSocket::NetworkLayerProtocol, protocol));
	else
		d_data->connectToHost(hostName, port, openMode, protocol);
}

void VipNetworkConnection::reconnect()
{
	if (state() != QAbstractSocket::ConnectedState && !d_data->host.isEmpty() && d_data->port > 0) {
		abort();
		connectToHost(d_data->host, d_data->port, d_data->openMode, d_data->protocol);
	}
}

void VipNetworkConnection::disconnectFromHost()
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "disconnectFromHost", Qt::BlockingQueuedConnection);
	else
		d_data->disconnectFromHost();
}

void VipNetworkConnection::abort()
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "abort", Qt::BlockingQueuedConnection);
	else
		d_data->abort();
}

void VipNetworkConnection::close()
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "close", Qt::BlockingQueuedConnection);
	else
		d_data->close();
}

bool VipNetworkConnection::bind(const QHostAddress& address, quint16 port, QAbstractSocket::BindMode mode)
{
	bool res = false;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(
		  d_data, "bind", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(QHostAddress, address), Q_ARG(quint16, port), Q_ARG(QAbstractSocket::BindMode, mode));
	else
		res = d_data->bind(address, port, mode);
	return res;
}

qint64 VipNetworkConnection::write(const QByteArray& ar)
{
	qint64 res = 0;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "write", Qt::BlockingQueuedConnection, Q_RETURN_ARG(qint64, res), Q_ARG(QByteArray, ar));
	else
		res = d_data->write(ar);

	return res;
}

bool VipNetworkConnection::waitForConnected(int msecs)
{
	if (state() == QAbstractSocket::ConnectedState)
		return true;

	bool res = false;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "waitForConnected", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(int, msecs));
	else
		res = d_data->waitForConnected(msecs);

	return res;
}

bool VipNetworkConnection::waitForDisconnected(int msecs)
{
	if (state() == QAbstractSocket::UnconnectedState)
		return true;

	bool res = false;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "waitForDisconnected", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(int, msecs));
	else
		res = d_data->waitForDisconnected(msecs);

	return res;
}

bool VipNetworkConnection::waitForReadyRead(int msecs)
{
	bool res = false;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "waitForReadyRead", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(int, msecs));
	else
		res = d_data->waitForReadyRead(msecs);

	return res;
}

bool VipNetworkConnection::waitForBytesWritten(int msecs)
{
	bool res = false;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "waitForBytesWritten", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(int, msecs));
	else
		res = d_data->waitForBytesWritten(msecs);

	return res;
}

void VipNetworkConnection::setAutoReconnection(bool enable)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->autoReconnection = enable;

	QObject::disconnect(this, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(reconnect()));
	QObject::disconnect(this, SIGNAL(disconnected()), this, SLOT(reconnect()));
	if (enable) {
		QObject::connect(this, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(reconnect()), Qt::QueuedConnection);
		QObject::connect(this, SIGNAL(disconnected()), this, SLOT(reconnect()), Qt::QueuedConnection);
	}
}

bool VipNetworkConnection::autoReconnection() const
{
	return d_data->autoReconnection;
}

QByteArray VipNetworkConnection::read(qint64 size)
{
	QByteArray res;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "read", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QByteArray, res), Q_ARG(qint64, size));
	else
		res = d_data->read(size);

	return res;
}

QByteArray VipNetworkConnection::readAll()
{
	QByteArray res;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(d_data, "readAll", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QByteArray, res));
	else
		res = d_data->readAll();

	return res;
}

qint64 VipNetworkConnection::bytesAvailable() const
{
	QMutexLocker lock(&d_data->mutex);
	return socket()->bytesAvailable();
}

qint64 VipNetworkConnection::bytesToWrite() const
{
	QMutexLocker lock(&d_data->mutex);
	return socket()->bytesToWrite();
}

class VipTcpServer::PrivateData
{
public:
	QMutex mutex;
	QList<qintptr> connections;
};

VipTcpServer::VipTcpServer(QObject* parent)
  : QTcpServer(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

VipTcpServer::~VipTcpServer()
{
	// Close all connections
	for (qintptr con : d_data->connections) {
		QTcpSocket sock;
		sock.setSocketDescriptor(con);
	}
}

qintptr VipTcpServer::nextPendingConnectionDescriptor()
{
	QMutexLocker lock(&d_data->mutex);
	if (d_data->connections.isEmpty())
		return 0;

	return d_data->connections.takeFirst();
}

void VipTcpServer::incomingConnection(qintptr socketDescriptor)
{
	QMutexLocker lock(&d_data->mutex);
	if (d_data->connections.size() >= this->maxPendingConnections()) {
		// close connections
		while (d_data->connections.size() > this->maxPendingConnections()) {
			QTcpSocket sock;
			sock.setSocketDescriptor(d_data->connections.back());
			d_data->connections.pop_back();
		}
		QTcpSocket sock;
		sock.setSocketDescriptor(socketDescriptor);
		return;
	}

	d_data->connections.push_back(socketDescriptor);
}
