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

#include <QEventLoop>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QPdfWriter>
#include <QPrinter>
#include <qapplication.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <qdesktopwidget.h>
#endif
#include <qfileinfo.h>
#include <qpagesize.h>
#include <qpicture.h>
#include <qprocess.h>
#include <qscreen.h>
#include <qsvggenerator.h>
#include <qwidget.h>

#include "VipLogging.h"
#include "VipRenderObject.h"
#include "VipShapeDevice.h"

static VipRenderObject* getRenderObject(QObject* item)
{
	if (!item)
		return nullptr;
	QVariant data = item->property("VipRenderObject");
	if (data.userType() == qMetaTypeId<VipRenderObject*>())
		return data.value<VipRenderObject*>();
	else
		return nullptr;
}

VipRenderObject::VipRenderObject(QObject* this_object)
  : m_object(this_object)
{
	m_object->setProperty("VipRenderObject", QVariant::fromValue(this));
}

VipRenderObject::~VipRenderObject()
{
	if (m_object)
		m_object->setProperty("VipRenderObject", QVariant());
}

bool VipRenderObject::renderObject(QPainter*, const QPointF&, bool)
{
	// default behavior for QWidget: use QWidget::render
	if (QWidget* w = qobject_cast<QWidget*>(m_object))
		if (w->isVisible()) {
			// TEST: uncomment this line
			// w->render(p, pos.toPoint(), QRegion(), draw_background ? QWidget::DrawWindowBackground : QWidget::RenderFlags(0));
			return true;
		}
	return false;
}

void VipRenderObject::startRender(QGraphicsScene* scene, VipRenderState& state)
{
	if (VipRenderObject* render = getRenderObject(scene))
		render->startRender(state);

	QList<QGraphicsItem*> items = scene->items();
	for (QList<QGraphicsItem*>::iterator it = items.begin(); it != items.end(); ++it) {
		QGraphicsItem* item = *it;
		if (VipRenderObject* obj = getRenderObject(item->toGraphicsObject()))
			obj->startRender(state);
	}
}

void VipRenderObject::endRender(QGraphicsScene* scene, VipRenderState& state)
{
	if (VipRenderObject* render = getRenderObject(scene))
		render->endRender(state);

	QList<QGraphicsItem*> items = scene->items();
	for (QList<QGraphicsItem*>::iterator it = items.begin(); it != items.end(); ++it) {
		QGraphicsItem* item = *it;
		if (VipRenderObject* obj = getRenderObject(item->toGraphicsObject()))
			obj->endRender(state);
	}
}

void VipRenderObject::startRender(QObject* obj, VipRenderState& state)
{
	if (VipRenderObject* render = getRenderObject(obj)) {
		render->startRender(state);
	}

	QList<QObject*> items = obj->findChildren<QObject*>();
	for (QList<QObject*>::iterator it = items.begin(); it != items.end(); ++it) {
		if (VipRenderObject* render = getRenderObject(*it)) {
			render->startRender(state);
		}
	}
}

void VipRenderObject::endRender(QObject* obj, VipRenderState& state)
{
	if (VipRenderObject* render = getRenderObject(obj)) {
		render->endRender(state);
	}

	QList<QObject*> items = obj->findChildren<QObject*>();
	for (QList<QObject*>::iterator it = items.begin(); it != items.end(); ++it) {
		if (VipRenderObject* render = getRenderObject(*it)) {
			render->endRender(state);
		}
	}
}

void VipRenderObject::renderObject(VipRenderObject* obj, QPainter* p, const QPoint& pos, bool draw_children, bool draw_background)
{
	QWidget* w = qobject_cast<QWidget*>(obj->m_object);
	if (w && w->isHidden())
		return;

	if (!obj->renderObject(p, pos, draw_background))
		return;

	if (draw_children) {
		if (w) {
			QList<QWidget*> children = w->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
			for (int i = 0; i < children.size(); ++i) {
				QWidget* child = children[i];

				p->save();

				QPoint offset = child->pos();
				// p->setTransform(QTransform().translate(offset.x(), offset.y()), true);

				if (VipRenderObject* ren = getRenderObject(child))
					renderObject(ren, p, pos + offset, draw_children, draw_background);
				else {
					VipRenderObject r(child);
					renderObject(&r, p, pos + offset, draw_children, draw_background);
				}

				p->restore();
			}
		}
		else {
			QList<QObject*> items = obj->m_object->findChildren<QObject*>();
			for (QList<QObject*>::iterator it = items.begin(); it != items.end(); ++it) {
				if (VipRenderObject* ren = getRenderObject(*it))
					ren->renderObject(p, pos, draw_background);
			}
		}
	}
}

QRectF VipRenderObject::renderRect(VipRenderObject* obj)
{
	if (QWidget* w = qobject_cast<QWidget*>(obj->m_object))
		return QRectF(0, 0, w->width(), w->height());

	VipShapeDevice dev;
	dev.setExtractBoundingRectOnly(true);
	QPainter p(&dev);

	VipRenderObject::renderObject(obj, &p, QPoint(0, 0), true, false);

	return dev.shape().boundingRect();
}

static bool hasPdfTops()
{
	static bool checked = false;
	static bool ok = false;
	if (checked)
		return ok;
	else {
		checked = true;
		QProcess p;
		p.start(QString("pdftops"), QStringList());
		if (p.waitForStarted()) {
			if (p.waitForFinished())
				ok = (p.readAllStandardOutput() + p.readAllStandardError()).size() > 0;
			return ok;
		}
		return ok = false;
	}
}

int VipRenderObject::supportedVectorFormats()
{
	int res = PDF | SVG;
	if (hasPdfTops())
		res |= PS | EPS;
	return res;
}

bool VipRenderObject::saveAsPs(VipRenderObject* render, const QString& filename)
{
	if (!hasPdfTops())
		return false;

	QSizeF point_size;
	if (!saveAsPdf(render, filename + ".pdf", &point_size))
		return false;

	QProcess p;
	/* QString option;
	if (QFileInfo(filename).suffix().compare("eps", Qt::CaseInsensitive) == 0)
		option = " -eps ";
	option += "-paperw " + QString::number(std::ceil(point_size.width())) + " -paperh " + QString::number(std::ceil(point_size.height())) + " ";
	QString command = "pdftops " + option + filename + ".pdf " + filename;
	p.start(command);*/
	QStringList option;
	if (QFileInfo(filename).suffix().compare("eps", Qt::CaseInsensitive) == 0)
		option << "-eps";
	option << "-paperw" << QString::number(std::ceil(point_size.width())) << "-paperh" << QString::number(std::ceil(point_size.height())) << (filename + ".pdf") << filename;
	p.start("pdftops", option);

	bool res = false;
	if (p.waitForStarted())
		if (p.waitForFinished())
			res = true;

	QFile::remove(filename + ".pdf");
	return res;
}

bool VipRenderObject::saveAsSvg(VipRenderObject* render, const QString& filename, const QString& title, const QString& description)
{
	VipRenderState state;
	VipRenderObject::startRender(render->thisObject(), state);

	QCoreApplication::processEvents();

	QRectF bounding = VipRenderObject::renderRect(render);
	bounding.setTopLeft(QPointF());

	QSvgGenerator generator;
	generator.setFileName(filename);
	generator.setSize(bounding.size().toSize());
	generator.setViewBox(bounding.toRect());
	generator.setTitle(title);
	generator.setDescription(description);
	QPainter painter;
	painter.begin(&generator);
	VipRenderObject::renderObject(render, &painter, QPoint(0, 0), true, false);
	painter.end();

	VipRenderObject::endRender(render->thisObject(), state);
	return true;
}

bool VipRenderObject::saveAsPdf(VipRenderObject* render, const QString& filename, QSizeF* point_size)
{
	QPrinter printer(QPrinter::HighResolution);
	printer.setOutputFileName(filename);
	printer.setFontEmbeddingEnabled(true);

	QFileInfo info(filename);
	if (info.suffix().compare("pdf", Qt::CaseInsensitive) == 0) {
		printer.setOutputFormat(QPrinter::PdfFormat);
#if (QT_VERSION_MAJOR >= 5 && QT_VERSION_MINOR > 9)
		printer.setPdfVersion(QPrinter::PdfVersion_1_6);
#endif
	}
	else
		return false;

	QPainter painter;
	VipRenderState state;
	VipRenderObject::startRender(render->thisObject(), state);

	QCoreApplication::processEvents();

	QPicture pic;
	QWidget* w = qobject_cast<QWidget*>(render->thisObject());
	QRect bounding;

	if (!w) {
		// first render in QPicture
		{
			QPainter p;
			p.begin(&pic); // paint in picture
			VipRenderObject::renderObject(render, &p, QPoint(0, 0), true, false);
			p.end();
		}
		bounding = pic.boundingRect();
	}
	else {
		bounding = QRect(QPoint(0, 0), w->size());
	}

	// get bounding rect in millimeters
	QScreen* screen = qApp->primaryScreen();
	if (w) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		int thisScreen = QApplication::desktop()->screenNumber(w);
		if (thisScreen >= 0)
			screen = qApp->screens()[thisScreen];
#else
		if (w->screen())
			screen = w->screen();
#endif
	}
	QSizeF screen_psize = screen->physicalSize();
	QSize screen_size = screen->size();
	qreal mm_per_pixel_x = screen_psize.width() / screen_size.width();
	qreal mm_per_pixel_y = screen_psize.height() / screen_size.height();
	QSizeF paper_size(bounding.width() * mm_per_pixel_x, bounding.height() * mm_per_pixel_y);

	if (point_size)
		*point_size = QSizeF(paper_size.width() * 2.834645669291, paper_size.height() * 2.834645669291);

	// printer.setFullPage(true);
	QPageSize page(paper_size, QPageSize::Millimeter);
	printer.setPageSize(page);
	// printer.setPageSize(QPrinter::Custom);
	// printer.setPaperSize(paper_size, QPrinter::Millimeter);
	// printer.setPageMargins(0, 0, 0, 0, QPageSize::Millimeter);
	printer.setResolution(600);

	if (!painter.begin(&printer)) { // failed to open file
		VIP_LOG_WARNING("Failed to open file %s, is it writable?", filename.toLatin1().data());
		VipRenderObject::endRender(render->thisObject(), state);
		return false;
	}

	// QRect window = printer.pageRect(QPrinter::Unit::Millimeter).toRect();
	// painter.setWindow(window);

	QTransform tr;
	qreal sx = (bounding.width() / paper_size.width());
	qreal sy = (bounding.height() / paper_size.height());
	double scalex = printer.width() / paper_size.width();
	double scaley = printer.height() / paper_size.height();
	tr.scale(scalex / sx, scaley / sy); // printer.width() / paper_size.width(), printer.height() / paper_size.height());
	// QTransform transform = QTransform::fromScale(painter.device()->physicalDpiX() / 1, painter.device()->physicalDpiY() / 1);
	painter.setTransform(tr);

	if (!w) {
		painter.drawPicture(QPoint(0, 0), pic);
	}
	else {
		VipRenderObject::renderObject(render, &painter, QPoint(0, 0), true, false);
	}
	painter.end();

	VipRenderObject::endRender(render->thisObject(), state);
	return true;
}

bool VipRenderObject::saveAsImage(VipRenderObject* render, const QString& filename, QColor* background)
{
	VipRenderState state;
	VipRenderObject::startRender(render->thisObject(), state);

	QCoreApplication::processEvents();

	QRect bounding = VipRenderObject::renderRect(render).toRect();
	bounding.setTopLeft(QPoint(0, 0));

	QPixmap pix(bounding.size());
	if (background)
		pix.fill(*background);
	QPainter p(&pix);

	VipRenderObject::renderObject(render, &p, QPoint(0, 0), true, background == nullptr);
	p.end();

	VipRenderObject::endRender(render->thisObject(), state);

	return pix.save(filename);
}