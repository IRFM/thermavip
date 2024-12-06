/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#ifndef VIP_NETWORK_H
#define VIP_NETWORK_H

#include "VipConfig.h"

#include <qhostaddress.h>
#include <qmutex.h>
#include <qnetworkproxy.h>
#include <qtcpserver.h>
#include <qtcpsocket.h>
#include <qthread.h>

class VipNetworkConnection;

namespace detail
{

	/// \internal
	/// Private class used by VipNetworkConnection
	class VIP_CORE_EXPORT VipNetworkConnectionPrivate : public QObject
	{
		Q_OBJECT

	public:
		QRecursiveMutex mutex;
		QAbstractSocket* socket;
		VipNetworkConnection* connection;
		QString host;
		quint16 port;
		QAbstractSocket::OpenMode openMode;
		QAbstractSocket::NetworkLayerProtocol protocol;
		bool autoReconnection;
		VipNetworkConnectionPrivate(VipNetworkConnection* con);

	public Q_SLOTS:
		void resume();
		void setPauseMode(QAbstractSocket::PauseModes pauseMode);
		void setProxy(const QNetworkProxy& networkProxy);
		void setReadBufferSize(qint64 size);
		void setSocketOption(QAbstractSocket::SocketOption option, const QVariant& value);
		qint64 write(const QByteArray& ar);
		void abort();
		void setSocketDescriptor(qintptr socketDescriptor,
					 QAbstractSocket::SocketState socketState = QAbstractSocket::ConnectedState,
					 QAbstractSocket::OpenMode openMode = QAbstractSocket::ReadWrite);
		void connectToHost(const QString& hostName,
				   quint16 port,
				   QAbstractSocket::OpenMode openMode = QAbstractSocket::ReadWrite,
				   QAbstractSocket::NetworkLayerProtocol protocol = QAbstractSocket::AnyIPProtocol);
		bool bind(const QHostAddress& address, quint16 port, QAbstractSocket::BindMode mode);
		void disconnectFromHost();
		void close();
		QByteArray read(qint64);
		QByteArray readAll();
		bool waitForConnected(int msecs = 30000);
		bool waitForDisconnected(int msecs = 30000);
		bool waitForReadyRead(int msecs = 30000);
		bool waitForBytesWritten(int msecs = 30000);
		void onReadyRead();
	};

}

VIP_CORE_EXPORT bool vipPing(const QByteArray& host);

/// VipNetworkConnection represents a thread safe socket connection that does not use the main event loop.
///
/// It might be complicated in Qt to use sockets in a multi-threaded environment. Indeed, calling on of the QAbstractSocket methods in a different thread than the socket's thread
/// might lead to unexpected crashes. Furthermore, QAbstractSocket is not thread safe and manipulating a socket from different threads might lead to concurrent resource access.
/// Another issue with QAbstractSocket is that it is heavily reliant on the main event loop for I/O operations. In few situations, when a lot of reading/writing operations
/// are involved, the responsiveness of the GUI is decreased.
///
/// VipNetworkConnection is meant to cope with these issues. It uses an internal QAbstractSocket that leaves in its own thread, and all calls to this socket's methods
/// are performed in the socket thread. All members are protected from concurrent access and can be safely called from different threads.
///
/// VipNetworkConnection uses its own event loop to perform I/O operations and can be used outside of the main event loop.
///
/// By default, VipNetworkConnection uses a QTcpSocket. You can change the socket type by overloading the member VipNetworkConnection::createSocket().
///
/// VipNetworkConnection has a similar interface as QAbstractSocket. To handle incoming data, you can use one of the following way:
/// -	Use VipNetworkConnection::readyRead signal,
/// -	Overload the VipNetworkConnection::onReadyRead member,
/// -	Directly call VipNetworkConnection::read() or VipNetworkConnection::readAll() from any thread.
///
/// To build a TCP server using VipNetworkConnection, you should use VipTcpServer class.
///
class VIP_CORE_EXPORT VipNetworkConnection : public QThread
{
	Q_OBJECT
	friend class detail::VipNetworkConnectionPrivate;

public:
	VipNetworkConnection(QObject* parent = nullptr);
	VipNetworkConnection(qintptr descriptor, QObject* parent = nullptr);
	~VipNetworkConnection();

	QAbstractSocket* socket() const;

	QAbstractSocket::SocketError error() const { return socket()->error(); }
	bool isValid() const { return socket()->isValid(); }
	QHostAddress localAddress() const { return socket()->localAddress(); }
	quint16 localPort() const { return socket()->localPort(); }
	QAbstractSocket::PauseModes pauseMode() const { return socket()->pauseMode(); }
	QHostAddress peerAddress() const { return socket()->peerAddress(); }
	QString peerName() const { return socket()->peerName(); }
	quint16 peerPort() const { return socket()->peerPort(); }
	QNetworkProxy proxy() const { return socket()->proxy(); }
	qint64 readBufferSize() const { return socket()->readBufferSize(); }
	qintptr socketDescriptor() const { return socket()->socketDescriptor(); }
	QVariant socketOption(QAbstractSocket::SocketOption option) { return socket()->socketOption(option); }
	QAbstractSocket::SocketType socketType() const { return socket()->socketType(); }
	QAbstractSocket::SocketState state() const { return socket()->state(); }

	bool waitForConnected(int msecs = 30000);
	bool waitForDisconnected(int msecs = 30000);
	bool waitForReadyRead(int msecs = 30000);
	bool waitForBytesWritten(int msecs = 30000);

	void setAutoReconnection(bool enable);
	bool autoReconnection() const;

	void setPauseMode(QAbstractSocket::PauseModes pauseMode);
	void setProxy(const QNetworkProxy& networkProxy);
	void setReadBufferSize(qint64 size);
	void setSocketOption(QAbstractSocket::SocketOption option, const QVariant& value);
	qint64 write(const QByteArray& ar);
	void setSocketDescriptor(qintptr socketDescriptor, QAbstractSocket::SocketState socketState = QAbstractSocket::ConnectedState, QAbstractSocket::OpenMode openMode = QAbstractSocket::ReadWrite);
	void connectToHost(const QString& hostName,
			   quint16 port,
			   QAbstractSocket::OpenMode openMode = QAbstractSocket::ReadWrite,
			   QAbstractSocket::NetworkLayerProtocol protocol = QAbstractSocket::AnyIPProtocol);
	bool bind(const QHostAddress& address, quint16 port = 0, QAbstractSocket::BindMode mode = QAbstractSocket::DefaultForPlatform);

	QByteArray read(qint64);
	QByteArray readAll();
	qint64 bytesAvailable() const;
	qint64 bytesToWrite() const;

public Q_SLOTS:
	void resume();
	void disconnectFromHost();
	void abort();
	void close();
	void reconnect();

protected:
	virtual QAbstractSocket* createSocket(QObject* parent = nullptr) { return new QTcpSocket(parent); }

protected Q_SLOTS:
	virtual void onReadyRead() {}

Q_SIGNALS:
	void readyRead();
	void connected();
	void disconnected();
	void error(QAbstractSocket::SocketError socketError);
	void hostFound();
	void proxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator);
	void stateChanged(QAbstractSocket::SocketState socketState);

	// private interface

private Q_SLOTS:
	virtual void run();

	void _emit_readyRead() { Q_EMIT readyRead(); }
	void _emit_connected() { Q_EMIT connected(); }
	void _emit_disconnected() { Q_EMIT disconnected(); }
	void _emit_error(QAbstractSocket::SocketError socketError) { Q_EMIT error(socketError); }
	void _emit_hostFound() { Q_EMIT hostFound(); }
	void _emit_proxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator) { Q_EMIT proxyAuthenticationRequired(proxy, authenticator); }
	void _emit_stateChanged(QAbstractSocket::SocketState socketState) { Q_EMIT stateChanged(socketState); }

private:
	detail::VipNetworkConnectionPrivate* d_data;
	qintptr m_pending;
};

/// @brief TCP server meant to be used with VipNetworkConnection
///
/// VipTcpServer inherits QTcpServer and uses most of its features,
/// except that it does not create QTcpSocket objects but instead
/// returns raw socket descriptors that can be used by VipNetworkConnection.
///
/// Therefore, you should use VipTcpServer::nextPendingConnectionDescriptor()
/// instead of QTcpServer::nextPendingConnection().
/// Note that QTcpServer::nextPendingConnection() will always return
/// a null socket.
///
/// VipTcpServer uses QTcpServer::maxPendingConnections() to discard
/// connections when the internal list of descriptor is full.
///
///
/// Usage:
///
/// \code
///
/// #include "VipNetwork.h"
/// #include <qapplication.h>
///
///
/// struct Senderthread : public QThread
/// {
/// 	virtual void run()
/// 	{
/// 		// create tcp server and wai for a connection
/// 		VipTcpServer server;
/// 		server.listen(QHostAddress("127.0.0.1"), 10703);
/// 		server.waitForNewConnection();
/// 		printf("server received a connected\n");
///
/// 		// create VipNetworkConnection from the new connection
/// 		VipNetworkConnection con(server.nextPendingConnectionDescriptor());
///
/// 		for (int i = 0; i < 5; ++i) {
/// 			// write 5 times 'hello x'
/// 			con.write("hello " + QByteArray::number(i));
/// 			QThread::msleep(1000);
/// 		}
/// 	}
/// };
///
/// struct Receiverthread : public QThread
/// {
/// 	virtual void run()
/// 	{
/// 		// create a VipNetworkConnection and connect it
/// 		VipNetworkConnection con;
/// 		con.connectToHost("127.0.0.1", 10703);
/// 		if (con.waitForConnected())
/// 			printf("client connected\n");
///
/// 		while (true) {
///
/// 			// the server write every second, therefore stop if nothing is received after 2s
/// 			if (!con.waitForReadyRead(2000))
/// 				break;
///
/// 			QByteArray ar = con.readAll();
/// 			printf("received '%s'\n", ar.data());
/// 		}
/// 	}
/// };
///
/// int main(int argc, char** argv)
/// {
/// 	QApplication app(argc, argv);
///
/// 	Senderthread server;
/// 	server.start();
///
/// 	Receiverthread client;
/// 	client.start();
///
/// 	server.wait();
///
///
///		return 0;
/// }
///
/// \endcode
///
class VIP_CORE_EXPORT VipTcpServer : public QTcpServer
{
	Q_OBJECT
public:
	VipTcpServer(QObject* parent = nullptr);
	~VipTcpServer();

	qintptr nextPendingConnectionDescriptor();

protected:
	virtual void incomingConnection(qintptr socketDescriptor);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_METATYPE(QAbstractSocket::PauseModes)
Q_DECLARE_METATYPE(QAbstractSocket::SocketOption)
Q_DECLARE_METATYPE(QAbstractSocket::OpenMode)
Q_DECLARE_METATYPE(QAbstractSocket::NetworkLayerProtocol)
Q_DECLARE_METATYPE(QAbstractSocket::BindMode)
Q_DECLARE_METATYPE(qintptr)

#endif