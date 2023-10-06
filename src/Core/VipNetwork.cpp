#include "VipNetwork.h"

#include <qdatetime.h>
#include <QCoreApplication>





static void registerVipNetworkConnection()
{
	static QMutex mutex;
	QMutexLocker lock(&mutex);

	static bool reg = 0;
	if (!reg)
	{
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



namespace detail
{

	VipNetworkConnectionPrivate::VipNetworkConnectionPrivate(VipNetworkConnection* con)
	  : mutex(QMutex::Recursive)
	  , socket(nullptr)
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





VipNetworkConnection::VipNetworkConnection(QObject * parent)
  : QThread(parent)
  , m_data(nullptr)
  , m_pending(0)
{
	registerVipNetworkConnection();


	start();

	//wait for socket to be created
	while (!m_data || !m_data->socket)
		QThread::msleep(1);
}

VipNetworkConnection::VipNetworkConnection(qintptr descriptor, QObject* parent)
  : QThread(parent)
  , m_data(nullptr)
  , m_pending(descriptor)
{
	registerVipNetworkConnection();

	start();

	// wait for socket to be created
	while (!m_data || !m_data->socket)
		QThread::msleep(1);
}

VipNetworkConnection::~VipNetworkConnection()
{
	setAutoReconnection(false);
	m_data->connection = nullptr;
	quit();
	wait();
}

QAbstractSocket * VipNetworkConnection::socket() const
{
	return m_data->socket;
}

void VipNetworkConnection::run()
{
	m_data = new detail::VipNetworkConnectionPrivate(this);
	m_data->socket = createSocket();
	if (m_pending) {
		m_data->socket->setSocketDescriptor(m_pending);
		m_data->socket->open(QIODevice::ReadWrite);
		m_pending = 0;
	}
	
	connect(m_data->socket, SIGNAL(readyRead()), m_data, SLOT(onReadyRead()), Qt::DirectConnection);
	connect(m_data->socket, SIGNAL(readyRead()), this, SLOT(_emit_readyRead()), Qt::DirectConnection);
	connect(m_data->socket, SIGNAL(connected()), this, SLOT(_emit_connected()), Qt::DirectConnection);
	connect(m_data->socket, SIGNAL(disconnected()), this, SLOT(_emit_disconnected()), Qt::DirectConnection);
	connect(m_data->socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(_emit_error(QAbstractSocket::SocketError)), Qt::DirectConnection);
	connect(m_data->socket, SIGNAL(hostFound()), this, SLOT(_emit_hostFound()), Qt::DirectConnection);
	connect(m_data->socket, SIGNAL(proxyAuthenticationRequired(const QNetworkProxy &, QAuthenticator *)), this, SLOT(_emit_proxyAuthenticationRequired(const QNetworkProxy &, QAuthenticator *)), Qt::DirectConnection);
	connect(m_data->socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(_emit_stateChanged(QAbstractSocket::SocketState)), Qt::DirectConnection);

	exec();

	m_data->socket->disconnectFromHost();
	if(m_data->socket->state() != QAbstractSocket::UnconnectedState)
		m_data->socket->waitForDisconnected();
	delete m_data->socket;
	delete m_data;
}



void	VipNetworkConnection::resume()
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "resume", Qt::BlockingQueuedConnection);
	else
		m_data->resume();
}
void	VipNetworkConnection::setPauseMode(QAbstractSocket::PauseModes pauseMode)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "setPauseMode", Qt::BlockingQueuedConnection, Q_ARG(QAbstractSocket::PauseModes, pauseMode));
	else
		m_data->setPauseMode(pauseMode);
}
void	VipNetworkConnection::setProxy(const QNetworkProxy &networkProxy)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "setProxy", Qt::BlockingQueuedConnection, Q_ARG(QNetworkProxy, networkProxy));
	else
		m_data->setProxy(networkProxy);
}
void	VipNetworkConnection::setReadBufferSize(qint64 size)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "setReadBufferSize", Qt::BlockingQueuedConnection, Q_ARG(qint64, size));
	else
		m_data->setReadBufferSize(size);
}
void	VipNetworkConnection::setSocketOption(QAbstractSocket::SocketOption option, const QVariant &value)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "setSocketOption", Qt::BlockingQueuedConnection, Q_ARG(QAbstractSocket::SocketOption, option), Q_ARG(QVariant, value));
	else
		m_data->setSocketOption(option, value);
}

void	VipNetworkConnection::setSocketDescriptor(qintptr socketDescriptor, QAbstractSocket::SocketState socketState, QAbstractSocket::OpenMode openMode)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "setSocketDescriptor", Qt::BlockingQueuedConnection, Q_ARG(qintptr, socketDescriptor), Q_ARG(QAbstractSocket::SocketState, socketState), Q_ARG(QAbstractSocket::OpenMode, openMode));
	else
		m_data->setSocketDescriptor(socketDescriptor, socketState, openMode);
}

void	VipNetworkConnection::connectToHost(const QString &hostName, quint16 port, QAbstractSocket::OpenMode openMode, QAbstractSocket::NetworkLayerProtocol protocol)
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "connectToHost", Qt::BlockingQueuedConnection, Q_ARG(QString, hostName), Q_ARG(quint16, port), Q_ARG(QAbstractSocket::OpenMode, openMode), Q_ARG(QAbstractSocket::NetworkLayerProtocol, protocol));
	else
		m_data->connectToHost(hostName, port, openMode, protocol);
}

void	VipNetworkConnection::reconnect()
{
	if (state() != QAbstractSocket::ConnectedState && !m_data->host.isEmpty() && m_data->port > 0)
	{
		abort();
		connectToHost(m_data->host, m_data->port, m_data->openMode, m_data->protocol);
	}
}

void	VipNetworkConnection::disconnectFromHost()
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "disconnectFromHost", Qt::BlockingQueuedConnection);
	else
		m_data->disconnectFromHost();
}

void	VipNetworkConnection::abort()
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "abort", Qt::BlockingQueuedConnection);
	else
		m_data->abort();
}

void	VipNetworkConnection::close()
{
	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "close", Qt::BlockingQueuedConnection);
	else
		m_data->close();
}

bool	VipNetworkConnection::bind(const QHostAddress &address, quint16 port, QAbstractSocket::BindMode mode)
{
	bool res = false;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "bind", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(QHostAddress, address), Q_ARG(quint16, port), Q_ARG(QAbstractSocket::BindMode, mode));
	else
		res = m_data->bind(address, port, mode);
	return res;
}


qint64	VipNetworkConnection::write(const QByteArray & ar)
{
	qint64 res = 0;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "write", Qt::BlockingQueuedConnection, Q_RETURN_ARG(qint64, res), Q_ARG(QByteArray, ar));
	else
		res = m_data->write(ar);

	return res;
}


bool VipNetworkConnection::waitForConnected(int msecs)
{
	if (state() == QAbstractSocket::ConnectedState)
		return true;

	bool res = false;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "waitForConnected", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(int, msecs));
	else
		res = m_data->waitForConnected(msecs);

	return res;
}

bool VipNetworkConnection::waitForDisconnected(int msecs)
{
	if (state() == QAbstractSocket::UnconnectedState)
		return true;

	bool res = false;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "waitForDisconnected", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(int, msecs));
	else
		res = m_data->waitForDisconnected(msecs);

	return res;
}

bool VipNetworkConnection::waitForReadyRead(int msecs )
{
	bool res = false;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "waitForReadyRead", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(int, msecs));
	else
		res = m_data->waitForReadyRead(msecs);

	return res;
}

bool VipNetworkConnection::waitForBytesWritten(int msecs )
{
	bool res = false;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "waitForBytesWritten", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(int, msecs));
	else
		res = m_data->waitForBytesWritten(msecs);

	return res;
}

void VipNetworkConnection::setAutoReconnection(bool enable)
{
	QMutexLocker lock(&m_data->mutex);
	m_data->autoReconnection = enable;

	QObject::disconnect(this, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(reconnect()));
	QObject::disconnect(this, SIGNAL(disconnected()), this, SLOT(reconnect()));
	if (enable)
	{
		QObject::connect(this, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(reconnect()),Qt::QueuedConnection);
		QObject::connect(this, SIGNAL(disconnected()), this, SLOT(reconnect()), Qt::QueuedConnection);
	}
}

bool VipNetworkConnection::autoReconnection() const
{
	return m_data->autoReconnection;
}

QByteArray VipNetworkConnection::read(qint64 size)
{
	QByteArray res;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "read", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QByteArray, res), Q_ARG(qint64, size));
	else
		res = m_data->read(size);

	return res;
}

QByteArray VipNetworkConnection::readAll()
{
	QByteArray res;

	if (QThread::currentThread() != socket()->thread())
		QMetaObject::invokeMethod(m_data, "readAll", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QByteArray, res));
	else
		res = m_data->readAll();

	return res;
}

qint64	VipNetworkConnection::bytesAvailable() const
{
	QMutexLocker lock(&m_data->mutex);
	return socket()->bytesAvailable();
}

qint64	VipNetworkConnection::bytesToWrite() const
{
	QMutexLocker lock(&m_data->mutex);
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
	m_data = new PrivateData();
}
	
VipTcpServer::~VipTcpServer() 
{
	// Close all connections
	for (qintptr con : m_data->connections) {
		QTcpSocket sock;
		sock.setSocketDescriptor(con);
	}
	delete m_data;
}

qintptr VipTcpServer::nextPendingConnectionDescriptor() 
{
	QMutexLocker lock(&m_data->mutex);
	if (m_data->connections.isEmpty())
		return 0;

	return m_data->connections.takeFirst();
}


void VipTcpServer::incomingConnection(qintptr socketDescriptor) 
{
	QMutexLocker lock(&m_data->mutex);
	if (m_data->connections.size() >= this->maxPendingConnections()) {
		// close connections
		while (m_data->connections.size() > this->maxPendingConnections()) {
			QTcpSocket sock;
			sock.setSocketDescriptor(m_data->connections.back());
			m_data->connections.pop_back();
		}
		QTcpSocket sock;
		sock.setSocketDescriptor(socketDescriptor);
		return;
	}

	m_data->connections.push_back(socketDescriptor);
}