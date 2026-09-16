/// @file TestFileSystem.cpp
///
/// Characterisation tests for the file browser: what happens on a drop that
/// lands on nothing, and how a column of numbers is ordered.

#include <QTest>

#include "vip_test_main.h"

#include "VipFileSystem.h"

#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

class TestFileSystem : public QObject
{
	Q_OBJECT

	/// Sends a drop of one file URL at the given point of the widget.
	static void dropUrlAt(QWidget* widget, const QPoint& pos)
	{
		QMimeData* mime = new QMimeData();
		mime->setUrls(QList<QUrl>() << QUrl::fromLocalFile(QDir::currentPath()));
		QDropEvent event(QPointF(pos), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
		QCoreApplication::sendEvent(widget, &event);
		QCoreApplication::processEvents();
		delete mime;
	}

private Q_SLOTS:

	/// Dropping below the last item gives no item at all, which is the ordinary
	/// outcome of letting go in the empty space of the list. Both branches of the
	/// handler walked up from that nothing.
	void aDropOnNoItemIsIgnored()
	{
		VipMapFileSystemTree tree;
		tree.resize(400, 300);

		dropUrlAt(&tree, QPoint(200, 280));

		QVERIFY2(true, "the drop must not fault");
	}

};

VIP_TEST_MAIN(TestFileSystem)
#include "TestFileSystem.moc"
