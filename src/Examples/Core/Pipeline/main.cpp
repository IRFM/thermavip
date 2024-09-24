#include "pipeline.h"

#include "VipNetwork.h"


#include <qdatetime.h>
#include <qapplication.h>

#include <iostream>


// no inline
#if defined(_MSC_VER) && !defined(__clang__)
#define SEQ_NOINLINE(...) __declspec(noinline) __VA_ARGS__
#else
#define SEQ_NOINLINE(...) __VA_ARGS__ __attribute__((noinline))
#endif

SEQ_NOINLINE(void) add(double& sum, int i, double factor)
{
	sum += i * factor;
}


struct Senderthread : public QThread
{
	virtual void run() 
	{ 
		// create tcp server and wai for a connection
		VipTcpServer server;
		server.listen(QHostAddress("127.0.0.1"), 10703);
		server.waitForNewConnection();
		printf("server received a connected\n");

		// create VipNetworkConnection from the new connection
		VipNetworkConnection con(server.nextPendingConnectionDescriptor());

		for(int i=0; i < 5; ++i) {
			// write 5 times 'hello x'
			con.write("hello " + QByteArray::number(i));
			QThread::msleep(1000);
		}
	}
};
struct Receiverthread : public QThread
{
	virtual void run()
	{
		// create a VipNetworkConnection and connect it
		VipNetworkConnection con;
		con.connectToHost("127.0.0.1", 10703);
		if (con.waitForConnected())
			printf("client connected\n");

		while (true) {

			// the server write every second, therefore stop if nothing is received after 2s
			if (!con.waitForReadyRead(2000))
				break;

			QByteArray ar = con.readAll();
			printf("received '%s'\n", ar.data());
		}
	}
};

int main(int argc, char** argv)
{
	QApplication app(argc,argv);
	{
		Senderthread th1;
		th1.start();
		Receiverthread th2;
		th2.start();
		th1.wait();
	}
	
	int count = 1000000;
	double factor = QDateTime::currentMSecsSinceEpoch();
	MultiplyNumericalValue mult;
	mult.propertyAt(0)->setData(factor);
	qint64 st, el;

	st = QDateTime::currentMSecsSinceEpoch();

	double sum = 0;
	for (int i = 0; i < count; ++i) {
		add(sum, i, factor);
	}

	el = QDateTime::currentMSecsSinceEpoch() - st;

	std::cout << "Raw multiply/add: " << el << " ms (result: " << sum << ")" << std::endl;

	


	mult.inputAt(0)->setListType(VipDataList::LastAvailable);
	mult.setComputeTimeStatistics(false);

	st = QDateTime::currentMSecsSinceEpoch();

	for (int i = 0; i < count; ++i) {
		mult.inputAt(0)->setData(i);
		mult.update();
		
	}

	el = QDateTime::currentMSecsSinceEpoch() - st;

	std::cout << "Synchronous multiply/add: " << el << " ms (result: " << mult.sum << ")" << std::endl;
	

	mult.setScheduleStrategy(VipProcessingObject::Asynchronous);
	mult.inputAt(0)->setListType(VipDataList::FIFO, VipDataList::Number);
	mult.sum = 0;

	st = QDateTime::currentMSecsSinceEpoch();

	VipInput* in = mult.inputAt(0);
	for (int i = 0; i < count; ++i) {
		in->setData(i);
	}
	qint64 el1 = QDateTime::currentMSecsSinceEpoch() - st;
	mult.wait();

	el = QDateTime::currentMSecsSinceEpoch() - st;

	std::cout << "Asynchronous multiply/add schedule: " << el1 << " ms" << std::endl;
	std::cout << "Asynchronous multiply/add: " << el << " ms (result: " << mult.sum << ")" << std::endl;
}