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

#include "VipUpdatePlayers.h"
#include "VipImageProcessing.h"

#include "VipDisplayArea.h"
#include "VipPlotMarker.h"
#include "VipPlotShape.h"
#include "VipSymbol.h"

#include <QApplication>
#include <QGraphicsSceneMouseEvent>

VipDrawCropArea::VipDrawCropArea(VipAbstractPlotArea* area)
{
	qApp->installEventFilter(this);
	area->installFilter(this);
	cursor = area->cursor();
	area->setCursor(Qt::CrossCursor);
}

VipDrawCropArea::~VipDrawCropArea()
{
	qApp->removeEventFilter(this);
	if (area())
		area()->setCursor(cursor);
}

QRectF VipDrawCropArea::boundingRect() const
{
	return QRectF(area()->scaleToPosition(begin), area()->scaleToPosition(end));
}

QPainterPath VipDrawCropArea::shape() const
{
	QPainterPath path;
	path.addRect(boundingRect());
	return path;
}

void VipDrawCropArea::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
	painter->setPen(QPen(Qt::white, 1, Qt::DashLine));
	QColor c = Qt::white;
	c.setAlpha(50);
	painter->setBrush(c);
	painter->drawRect(boundingRect());
}

bool VipDrawCropArea::sceneEvent(QEvent* event)
{
	if (!area())
		return false;

	if (event->type() == QEvent::GraphicsSceneMousePress) {
		QGraphicsSceneMouseEvent* evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		begin = vipRound(area()->positionToScale(evt->pos()));
		end = begin;
		return true;
	}
	else if (event->type() == QEvent::GraphicsSceneMouseMove) {
		QGraphicsSceneMouseEvent* evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		end = vipRound(area()->positionToScale(evt->pos()));
		this->prepareGeometryChange();
		return true;
	}
	else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
		if (begin != end) {
			Q_EMIT cropCreated(begin, end);
			this->deleteLater();
		}
		return true;
	}

	return false;
}

bool VipDrawCropArea::eventFilter(QObject* /*watched*/, QEvent* event)
{
	if (!area())
		return false;

	if (event->type() == QEvent::MouseButtonPress) {
		if (VipImageArea2D* a = qobject_cast<VipImageArea2D*>(area())) {
			QPoint pt = QCursor::pos();
			QRect view_rect = area()->view()->mapFromScene(a->visualizedSceneRect()).boundingRect();
			view_rect = QRect(area()->view()->mapToGlobal(view_rect.topLeft()), area()->view()->mapToGlobal(view_rect.bottomRight()));
			if (!view_rect.contains(pt))
				this->deleteLater();
		}
	}

	return false;
}

VipUpdateVideoPlayer::VipUpdateVideoPlayer(VipVideoPlayer* player)
  : QObject(player)
  , m_player(player)
  , m_displayMarkerPos(false)
{
	player->setProperty("NoImageProcessing", true);

	m_toolBar = new QToolBar();
	m_toolBar->setIconSize(QSize(18, 18));

	m_toolBar->addSeparator();
	connect(m_toolBar->addAction(vipIcon("rotate_left.png"), "Rotate 90 degrees left"), SIGNAL(triggered(bool)), this, SLOT(rotate90Left()));
	connect(m_toolBar->addAction(vipIcon("rotate_right.png"), "Rotate 90 degrees right"), SIGNAL(triggered(bool)), this, SLOT(rotate90Right()));
	connect(m_toolBar->addAction(vipIcon("rotate_180.png"), "Rotate 180"), SIGNAL(triggered(bool)), this, SLOT(rotate180()));
	connect(m_toolBar->addAction(vipIcon("vreflection.png"), "Vertical reflection"), SIGNAL(triggered(bool)), this, SLOT(mirrorVertical()));
	connect(m_toolBar->addAction(vipIcon("hreflection.png"), "Horizontal reflection"), SIGNAL(triggered(bool)), this, SLOT(mirrorHorizontal()));

	// make sure the tool bar has a fixed size and does not show a drop-down indicator
	m_toolBar->setMinimumWidth(m_toolBar->sizeHint().width());

	m_crop = new QToolButton();
	m_crop->setAutoRaise(true);
	m_crop->setToolTip("Define a new crop area");
	m_crop->setIcon(vipIcon("crop.png"));
	m_crop->setCheckable(true);
	connect(m_crop, SIGNAL(clicked(bool)), this, SLOT(cropStarted()));

	QMenu* menu = new QMenu(m_crop);
	connect(menu->addAction("Remove last crop"), SIGNAL(triggered(bool)), this, SLOT(removeLastCrop()));
	menu->addSeparator();
	connect(menu->addAction("Remove all crops"), SIGNAL(triggered(bool)), this, SLOT(removeAllCrops()));
	m_crop->setMenu(menu);
	m_crop->setPopupMode(QToolButton::MenuButtonPopup);

	player->toolBar()->addWidget(m_crop);

	player->toolBar()->addSeparator();
	m_local_minmax = new QToolButton();
	m_local_minmax->setAutoRaise(true);
	m_local_minmax->setToolTip("Display local minimum and maximum");
	m_local_minmax->setIcon(vipIcon("local_minmax.png"));
	m_local_minmax->setCheckable(true);
	player->toolBar()->addWidget(m_local_minmax);
	connect(m_local_minmax, SIGNAL(clicked(bool)), this, SLOT(setMarkersEnabled(bool)));

	QMenu* display_pos = new QMenu(m_local_minmax);
	m_minmaxPos = display_pos->addAction("Display minimum/maximum positions");
	m_minmaxPos->setCheckable(true);
	display_pos->addSeparator();
	QAction* save_infos = display_pos->addAction("Save selected ROI infos (Tmax, PosX, PosY)");
	connect(save_infos, SIGNAL(triggered(bool)), this, SLOT(saveROIInfos()));
	connect(m_minmaxPos, SIGNAL(triggered(bool)), this, SLOT(setDisplayMarkerPos(bool)));
	m_local_minmax->setMenu(display_pos);
	m_local_minmax->setPopupMode(QToolButton::MenuButtonPopup);

	m_toolBar->hide();
	m_toolBarAction = player->toolBar()->addWidget(m_toolBar);

	// show the tool bar when the player displays a valid image.
	connect(player, SIGNAL(displayImageChanged()), this, SLOT(newPlayerImage()), Qt::DirectConnection);

	// read properties and restore state
	setMarkersEnabled(player->property("_vip_customMarkersEnabled").toBool());
	setDisplayMarkerPos(player->property("_vip_customDisplayMarkerPos").toBool());
}

void VipUpdateVideoPlayer::rotate90Left()
{
	if (VipVideoPlayer* p = m_player)
		if (VipProcessingList* lst = p->sourceProcessingList()) {
			// if there are already 3 rotate left, add ing a 4 one is a null operation, remove all of them
			QList<VipRotate90Left*> rotate_left = vipListCast<VipRotate90Left*>(lst->processings());
			if (rotate_left.size() == 3) {
				for (int i = 0; i < rotate_left.size(); ++i)
					lst->remove(rotate_left[i]);
			}
			else {
				// try to remove a VipRotate90Right
				QList<VipRotate90Right*> rotate_right = vipListCast<VipRotate90Right*>(lst->processings());
				if (rotate_right.size())
					lst->remove(rotate_right.first());
				else
					lst->append(new VipRotate90Left());
			}
			lst->reload();
		}
}

void VipUpdateVideoPlayer::rotate90Right()
{
	if (VipVideoPlayer* p = m_player)
		if (VipProcessingList* lst = p->sourceProcessingList()) {
			// if there are already 3 rotate right, add ing a 4 one is a null operation, remove all of them
			QList<VipRotate90Right*> rotate_right = vipListCast<VipRotate90Right*>(lst->processings());
			if (rotate_right.size() == 3) {
				for (int i = 0; i < rotate_right.size(); ++i)
					lst->remove(rotate_right[i]);
			}
			else {
				// try to remove a VipRotate90Left
				QList<VipRotate90Left*> rotate_left = vipListCast<VipRotate90Left*>(lst->processings());
				if (rotate_left.size())
					lst->remove(rotate_left.first());
				else
					lst->append(new VipRotate90Right());
			}
			lst->reload();
		}
}

void VipUpdateVideoPlayer::rotate180()
{
	if (VipVideoPlayer* p = m_player)
		if (VipProcessingList* lst = p->sourceProcessingList()) {
			// try to remove a 2 VipRotate90Right or 2 VipRotate90Left or 1 VipRotate180
			QList<VipRotate90Right*> rotate_right = vipListCast<VipRotate90Right*>(lst->processings());
			if (rotate_right.size() > 1) {
				lst->remove(rotate_right[0]);
				lst->remove(rotate_right[1]);
				lst->reload();
				return;
			}
			QList<VipRotate90Left*> rotate_left = vipListCast<VipRotate90Left*>(lst->processings());
			if (rotate_left.size() > 1) {
				lst->remove(rotate_left[0]);
				lst->remove(rotate_left[1]);
				lst->reload();
				return;
			}
			QList<VipRotate180*> rotate_180 = vipListCast<VipRotate180*>(lst->processings());
			if (rotate_180.size()) {
				lst->remove(rotate_180[0]);
				lst->reload();
				return;
			}

			lst->append(new VipRotate180());
			lst->reload();
		}
}

void VipUpdateVideoPlayer::mirrorVertical()
{
	if (VipVideoPlayer* p = m_player)
		if (VipProcessingList* lst = p->sourceProcessingList()) {
			// try to remove a VipMirrorV
			QList<VipMirrorV*> mirror_v = vipListCast<VipMirrorV*>(lst->processings());
			if (mirror_v.size())
				lst->remove(mirror_v.first());
			else
				lst->append(new VipMirrorV());
			lst->reload();
		}
}

void VipUpdateVideoPlayer::mirrorHorizontal()
{
	if (VipVideoPlayer* p = m_player)
		if (VipProcessingList* lst = p->sourceProcessingList()) {
			// try to remove a VipMirrorH
			QList<VipMirrorH*> mirror_h = vipListCast<VipMirrorH*>(lst->processings());
			if (mirror_h.size())
				lst->remove(mirror_h.first());
			else
				lst->append(new VipMirrorH());
			lst->reload();
		}
}

void VipUpdateVideoPlayer::removeLastCrop()
{
	if (VipVideoPlayer* p = m_player)
		if (VipProcessingList* lst = p->sourceProcessingList()) {
			// remove the last VipImageCrop object in the processing list
			QList<VipImageCrop*> crops = vipListCast<VipImageCrop*>(lst->processings());
			if (crops.size()) {
				lst->remove(crops.last());
				lst->reload();
			}
		}
}

void VipUpdateVideoPlayer::removeAllCrops()
{
	if (VipVideoPlayer* p = m_player)
		if (VipProcessingList* lst = p->sourceProcessingList()) {
			// remove all VipImageCrop objects in the processing list
			QList<VipImageCrop*> crops = vipListCast<VipImageCrop*>(lst->processings());
			if (crops.size()) {
				lst->blockSignals(true);
				for (int i = 0; i < crops.size() - 1; ++i)
					lst->remove(crops[i]);
				lst->blockSignals(false);
				lst->remove(crops.last());
				lst->reload();
			}
		}
}

void VipUpdateVideoPlayer::cropStarted()
{
	if (VipVideoPlayer* p = m_player) {
		if (m_crop->isChecked()) {
			VipDrawCropArea* draw = new VipDrawCropArea(p->plotWidget2D()->area());
			connect(draw, SIGNAL(destroyed(QObject*)), this, SLOT(cropEnded()));
			connect(draw, SIGNAL(cropCreated(const QPointF&, const QPointF&)), this, SLOT(cropAdded(const QPointF&, const QPointF&)));
		}
	}
}

void VipUpdateVideoPlayer::cropEnded()
{
	m_crop->blockSignals(true);
	m_crop->setChecked(false);
	m_crop->blockSignals(false);
}

void VipUpdateVideoPlayer::cropAdded(const QPointF& start, const QPointF& end)
{
	if (VipVideoPlayer* p = m_player)
		if (VipProcessingList* lst = p->sourceProcessingList()) {
			QPointF st(qMin(start.x(), end.x()), qMin(start.y(), end.y()));
			QPointF en(qMax(start.x(), end.x()), qMax(start.y(), end.y()));

			VipImageCrop* crop = new VipImageCrop();
			crop->setStartPosition(vipVector((qsizetype)st.y(), (qsizetype)st.x()));
			crop->setEndPosition(vipVector((qsizetype)en.y(), (qsizetype)en.x()));
			lst->append(crop);
			lst->reload();
		}
}

void VipUpdateVideoPlayer::newPlayerImage()
{
	if (VipVideoPlayer* p = m_player) {
		// display the tool bar if we have a valid image
		if (!p->spectrogram()->rawData().isEmpty()) {
			// ok, the player displays a valid image, avoid going through this function anymore
			m_toolBarAction->setVisible(true);
			disconnect(p, SIGNAL(displayImageChanged()), this, SLOT(newPlayerImage()));
		}
	}
}

void VipUpdateVideoPlayer::setDisplayMarkerPos(bool enable)
{
	m_minmaxPos->blockSignals(true);
	m_minmaxPos->setChecked(enable);
	m_minmaxPos->blockSignals(false);
	if (VipVideoPlayer* p = m_player) {
		p->setProperty("_vip_customDisplayMarkerPos", enable);
		m_displayMarkerPos = enable;
		if (m_maxMarkers.size() && m_maxMarkers.first()->isVisible())
			updateMarkers();
	}
}

void VipUpdateVideoPlayer::setMarkersEnabled(bool enable)
{
	m_local_minmax->blockSignals(true);
	m_local_minmax->setChecked(enable);
	m_local_minmax->blockSignals(false);
	if (VipVideoPlayer* p = m_player) {
		p->setProperty("_vip_customMarkersEnabled", enable);
		if (enable) {
			VipPlotSpectrogram* spectro = p->spectrogram();
			connect(spectro, SIGNAL(dataChanged()), this, SLOT(updateMarkers()));
			connect(p->spectrogram()->scene(), SIGNAL(selectionChanged()), this, SLOT(updateMarkers()));
			connect(p->plotSceneModel()->sceneModel().shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(updateMarkers()));
			updateMarkers();
		}
		else {
			VipPlotSpectrogram* spectro = p->spectrogram();
			disconnect(spectro, SIGNAL(dataChanged()), this, SLOT(updateMarkers()));
			disconnect(p->spectrogram()->scene(), SIGNAL(selectionChanged()), this, SLOT(updateMarkers()));
			disconnect(p->plotSceneModel()->sceneModel().shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(updateMarkers()));

			for (int i = 0; i < m_minMarkers.size(); ++i)
				m_minMarkers[i]->setVisible(false);
			for (int i = 0; i < m_maxMarkers.size(); ++i)
				m_maxMarkers[i]->setVisible(false);
		}
	}
}

#include "VipPlotGrid.h"

void VipUpdateVideoPlayer::saveROIInfos()
{
	if (VipVideoPlayer* p = m_player)
		if (VipPlotSceneModel* plot_scene = p->plotSceneModel()) {
			VipSceneModel model = plot_scene->sceneModel();
			QList<VipPlotShape*> plot_shapes = p->viewer()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
			if (plot_shapes.size() == 0)
				return;

			VipNDArray image = p->array(); // data.extract(data.boundingRect());
			if (image.isEmpty() || !image.canConvert<double>())
				return;

			QPoint offset(0, 0); // = data.boundingRect().topLeft().toPoint();
			QRect rect(QPoint(0, 0), QSize(image.shape(1), image.shape(0)));

			QByteArray content;
			for (int i = 0; i < plot_shapes.size(); ++i) {
				const VipShape sh = plot_shapes[i]->rawData();
				QString name = sh.name();
				if (name.isEmpty())
					name = sh.group() + " " + QString::number(sh.id());
				VipShapeStatistics stats = sh.statistics(image, offset, &m_buffer, VipShapeStatistics::Maximum);
				content += name.toLatin1() + ": " + QByteArray::number(stats.max) + " " + QByteArray::number(stats.maxPoint.x()) + " " + QByteArray::number(stats.maxPoint.y()) + "\n";
			}

			QString filename = VipFileDialog::getSaveFileName(vipGetMainWindow(), "Save ROI infos", "Text file (*.txt)");
			if (filename.isEmpty())
				return;

			QFile out(filename);
			if (!out.open(QFile::WriteOnly | QFile::Text))
				return;
			out.write(content);
			out.close();
		}
}

void VipUpdateVideoPlayer::updateMarkers()
{
	if (VipVideoPlayer* p = m_player)
		if (VipPlotSceneModel* plot_scene = p->plotSceneModel()) {

			VipSceneModel model = plot_scene->sceneModel();

			QList<VipPlotShape*> plot_shapes = p->viewer()->area()->findItems<VipPlotShape*>(QString(), 2, 1);
			VipShapeList shapes;
			double level = -1;
			for (int i = 0; i < plot_shapes.size(); ++i) {
				// if (plot_shapes[i]->rawData().type() != VipShape::Point)
				{
					shapes << plot_shapes[i]->rawData();
					if (level < 0)
						level = plot_shapes[i]->zValue() + 100;
				}
			}
			if (level < 0)
				level = p->spectrogram()->zValue() + 100;

			// get the current image
			// VipRasterData data = p->spectrogram()->rawData();
			VipNDArray image = p->array(); // data.extract(data.boundingRect());
			if (image.isEmpty() || !image.canConvert<double>())
				return;

			QPoint offset(0, 0); // = data.boundingRect().topLeft().toPoint();
			QRect rect(QPoint(0, 0), QSize(image.shape(1), image.shape(0)));

			VipShape full_image(rect);

			// create the full list of shapes
			if (!shapes.size())
				shapes.append(full_image);

			QRectF canvas = p->viewer()->area()->canvas()->boundingRect();

			// update the markers
			for (int i = 0; i < shapes.size(); ++i) {
				if (i >= m_maxMarkers.size()) {
					VipPlotMarker* m = new VipPlotMarker();
					m->setIgnoreStyleSheet(true);
					m->setLineStyle(VipPlotMarker::NoLine);
					m->setItemAttribute(VipPlotMarker::ClipToScaleRect, false);
					m->setItemAttribute(VipPlotMarker::HasToolTip, false);
					m->setItemAttribute(VipPlotMarker::IgnoreMouseEvents, true);
					VipSymbol* s = new VipSymbol();
					s->setSize(10, 10);
					s->setStyle(VipSymbol::Triangle);
					m->setSymbol(s);
					m->setSymbolVisible(true);
					s->setPen(Qt::white);
					s->setBrush(Qt::red);
					m->setAxes(p->spectrogram()->axes(), p->spectrogram()->coordinateSystemType());
					m->setRenderHints(QPainter::Antialiasing);
					m->setLabelAlignment(Qt::AlignRight);
					m->setProperty("_vip_no_serialize", true); // avoid serialization of VipPlotMarker
					m_maxMarkers.append(m);
				}
				if (i >= m_minMarkers.size()) {
					VipPlotMarker* m = new VipPlotMarker();
					m->setIgnoreStyleSheet(true);
					m->setLineStyle(VipPlotMarker::NoLine);
					m->setItemAttribute(VipPlotMarker::HasToolTip, false);
					m->setItemAttribute(VipPlotMarker::ClipToScaleRect, false);
					m->setItemAttribute(VipPlotMarker::IgnoreMouseEvents, true);
					VipSymbol* s = new VipSymbol();
					s->setSize(10, 10);
					s->setStyle(VipSymbol::Triangle);
					s->setPen(Qt::white);
					s->setBrush(Qt::blue);
					m->setSymbol(s);
					m->setSymbolVisible(true);
					m->setAxes(p->spectrogram()->axes(), p->spectrogram()->coordinateSystemType());
					m->setRenderHints(QPainter::Antialiasing);
					m->setLabelAlignment(Qt::AlignRight);
					m->setProperty("_vip_no_serialize", true);
					m_minMarkers.append(m);
				}

				VipShapeStatistics statistics = shapes[i].statistics(image, offset, &m_buffer, VipShapeStatistics::Maximum | VipShapeStatistics::Minimum);
				VipShapeStatistics stats = statistics;

				// get the size of a pixel
				QPointF pix_size = p->plotWidget2D()->area()->scaleToPosition(QPointF(1, 1)) - p->plotWidget2D()->area()->scaleToPosition(QPointF(0, 0));
				// set the points
				m_minMarkers[i]->setRawData(QPointF(stats.minPoint) + QPointF(0.5, 0.5));
				m_maxMarkers[i]->setRawData(QPointF(stats.maxPoint) + QPointF(0.5, 0.5));

				// set the texts
				VipText min_t;
				if (m_displayMarkerPos)
					min_t.setText("<b>" + QString::number(stats.min) + "</b><br>(x:" + QString::number(stats.minPoint.x()) + " , y:" + QString::number(stats.minPoint.y()) + ")");
				else
					min_t.setText("<b>" + QString::number(stats.min) + "</b>");
				min_t.setTextPen(QColor(Qt::blue));
				min_t.setBackgroundBrush(QBrush(QColor(255, 255, 255, 160)));

				VipText max_t;
				if (m_displayMarkerPos)
					max_t.setText("<b>" + QString::number(stats.max) + "</b><br>(x:" + QString::number(stats.maxPoint.x()) + " , y:" + QString::number(stats.maxPoint.y()) + ")");
				else
					max_t.setText("<b>" + QString::number(stats.max) + "</b>");
				max_t.setTextPen(QColor(Qt::red));
				max_t.setBackgroundBrush(QBrush(QColor(255, 255, 255, 160)));

				QPointF min_pos = m_minMarkers[i]->sceneMap()->transform(m_minMarkers[i]->rawData());
				QPointF max_pos = m_maxMarkers[i]->sceneMap()->transform(m_maxMarkers[i]->rawData());
				Qt::Alignment min_align = Qt::AlignRight;
				if (canvas.right() - min_pos.x() < 30)
					min_align = Qt::AlignLeft;
				if (canvas.bottom() - min_pos.y() < 30)
					min_align = Qt::AlignTop | Qt::AlignHCenter;
				else if (min_pos.y() - canvas.top() < 30)
					min_align |= Qt::AlignBottom | Qt::AlignHCenter;
				;

				Qt::Alignment max_align = Qt::AlignRight;
				if (canvas.right() - max_pos.x() < 30)
					max_align = Qt::AlignLeft;
				if (canvas.bottom() - max_pos.y() < 30)
					max_align |= Qt::AlignTop | Qt::AlignHCenter;
				else if (max_pos.y() - canvas.top() < 30)
					max_align |= Qt::AlignBottom | Qt::AlignHCenter;
				;

				m_minMarkers[i]->setLabelAlignment(min_align);
				m_maxMarkers[i]->setLabelAlignment(max_align);
				m_minMarkers[i]->setLabel(min_t);
				m_maxMarkers[i]->setLabel(max_t);

				// set the transform
				QTransform min_tr;
				min_tr.translate(0, pix_size.y() / 2 + m_minMarkers[i]->symbol()->size().height() / 2);

				m_minMarkers[i]->setTransform(min_tr);
				m_maxMarkers[i]->setTransform(min_tr);

				if (shapes[i].type() == VipShape::Point)
					m_minMarkers[i]->setVisible(false);
				else if (!m_minMarkers[i]->isVisible())
					m_minMarkers[i]->setVisible(true);

				if (!m_maxMarkers[i]->isVisible())
					m_maxMarkers[i]->setVisible(true);

				if (level > 0) {
					m_minMarkers[i]->setZValue(level);
					m_maxMarkers[i]->setZValue(level + 1);
				}
			}

			for (int i = shapes.size(); i < m_minMarkers.size(); ++i)
				m_minMarkers[i]->setVisible(false);
			for (int i = shapes.size(); i < m_maxMarkers.size(); ++i)
				m_maxMarkers[i]->setVisible(false);
		}
}

static void cropOnShape(VipPlotShape* shape, VipVideoPlayer* player)
{
	if (VipProcessingList* lst = player->sourceProcessingList()) {
		QRectF r = shape->rawData().boundingRect().normalized();

		QPoint st(r.topLeft().toPoint());
		QPoint en(r.bottomRight().toPoint());

		VipImageCrop* crop = new VipImageCrop();
		crop->setStartPosition(vipVector(st.y(), st.x()));
		crop->setEndPosition(vipVector(en.y(), en.x()));
		lst->append(crop);
		lst->reload();
	}
}

static void removeLastCrop(VipVideoPlayer* p)
{
	if (VipProcessingList* lst = p->sourceProcessingList()) {
		// remove the last VipImageCrop object in the processing list
		QList<VipImageCrop*> crops = vipListCast<VipImageCrop*>(lst->processings());
		if (crops.size()) {
			lst->remove(crops.last());
			lst->reload();
		}
	}
}

struct Separator : QAction
{
	Separator(QObject* parent)
	  : QAction(parent)
	{
	}

	~Separator() {}
};

static QList<QAction*> videoPlayerActions(VipPlotItem* item, VipVideoPlayer* player)
{
	QList<QAction*> actions;
	if (VipPlotShape* shape = qobject_cast<VipPlotShape*>(item)) {
		if ((shape->rawData().type() == VipShape::Path || shape->rawData().type() == VipShape::Polygon) && !vipIsImageArray(player->viewer()->area()->array())) {
			QAction* crop = new QAction("Crop image on shape bounding rect", nullptr);
			QObject::connect(crop, &QAction::triggered, std::bind(cropOnShape, shape, player));
			actions << crop;
		}
	}

	if (VipProcessingList* lst = player->sourceProcessingList()) {
		if (lst->processings<VipImageCrop*>().size()) {
			QAction* remove = new QAction("Remove last crop", nullptr);
			QObject::connect(remove, &QAction::triggered, std::bind(removeLastCrop, player));
			actions << remove;
		}
	}

	if (actions.size()) {
		Separator* sep = new Separator(nullptr);
		sep->setSeparator(true);
		actions.insert(actions.begin(), sep);
	}
	return actions;
}

static void updateVideoPlayer(VipVideoPlayer* player)
{
	if (player && !player->property("NoImageProcessing").toBool() && player->spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>())
		new VipUpdateVideoPlayer(player);
}

int VipUpdateVideoPlayer::registerClass()
{
	// register the VipUpdateVideoPlayer class
	VipFDItemRightClick().append<QList<QAction*>(VipPlotItem*, VipVideoPlayer*)>(videoPlayerActions);
	vipFDPlayerCreated().append<void(VipVideoPlayer*)>(updateVideoPlayer);
	return 0;
}

VipDrawDistance2Points::VipDrawDistance2Points(VipAbstractPlotArea* area)
  : startItem(nullptr)
  , endItem(nullptr)
  , hover(nullptr)
{
	qApp->installEventFilter(this);
	area->installFilter(this);
	cursor = area->cursor();
	area->setCursor(Qt::CrossCursor);
}
VipDrawDistance2Points::~VipDrawDistance2Points()
{
	qApp->removeEventFilter(this);
	if (area())
		area()->setCursor(cursor);
}

QRectF VipDrawDistance2Points::boundingRect() const
{
	QLineF l = computeLine();
	return QRectF(l.p1(), l.p2()).normalized();
}

QPainterPath VipDrawDistance2Points::shape() const
{
	QPainterPath path;
	path.addRect(boundingRect());
	return path;
}
#include "VipQuiver.h"

QLineF VipDrawDistance2Points::computeLine() const
{
	if (!startItem) {
		// no click for now, highlight closest point
		if (hover) {
			return QLineF(hoverPt, hoverPt);
		}
		else
			return QLineF();
	}
	else {
		if (hover) {
			return QLineF(startItem->sceneMap()->transform(begin), hoverPt);
		}
		else {
			return QLineF(startItem->sceneMap()->transform(begin), this->mapFromScene(end));
		}
	}
}

void VipDrawDistance2Points::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	VipQuiverPath p;
	p.setPen(QPen(Qt::red, 1, Qt::DashLine));
	p.setExtremityPen(VipQuiverPath::Start, QPen(Qt::red));
	p.setExtremityPen(VipQuiverPath::End, QPen(Qt::red));
	p.setStyle(VipQuiverPath::StartCircle | VipQuiverPath::EndCircle);
	p.setLength(VipQuiverPath::Start, 9);
	p.setLength(VipQuiverPath::End, 9);
	painter->setRenderHints(QPainter::Antialiasing);

	if (!startItem) {
		// no click for now, highlight closest point
		if (hover) {
			p.draw(painter, QLineF(hoverPt, hoverPt));
		}
	}
	else {
		if (hover) {
			p.draw(painter, QLineF(startItem->sceneMap()->transform(begin), hoverPt));
		}
		else {
			p.setStyle(VipQuiverPath::StartCircle);
			p.draw(painter, QLineF(startItem->sceneMap()->transform(begin), this->mapFromScene(end)));
		}
	}
}

static VipPlotItem* findItemPoint(VipAbstractPlotArea* area, const QPointF& scene_pos, QPointF& item_point)
{
	QList<VipPointVector> points;
	VipBoxStyleList styles;
	QList<int> legends;
	PlotItemList lst = area->plotItems(scene_pos, -1, VIP_PLOTTING_STICK_DISTANCE, points, styles, legends);
	double d = std::numeric_limits<double>::max();
	VipPlotItem* item = nullptr;

	// take the closest item
	for (int i = 0; i < points.size(); ++i) {
		if (points[i].isEmpty())
			continue;
		QPointF pt = lst[i]->mapFromScene(points[i].first());
		double dist = (pt - scene_pos).manhattanLength();
		if (dist < d) {
			d = dist;
			item = lst[i];
			item_point = points[i].first();
		}
	}
	return item;
}

bool VipDrawDistance2Points::sceneEvent(QEvent* event)
{
	if (!area())
		return false;

	if (event->type() == QEvent::GraphicsSceneMousePress) {
		QGraphicsSceneMouseEvent* evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		if (evt->button() != Qt::LeftButton)
			return false;

		if (hover) {
			if (begin == VipPoint()) {
				// first point
				startItem = hover;
				endItem = nullptr;
				begin = end = startItem->sceneMap()->invTransform(hoverPt);
			}
		}
		return true;
	}
	else if (event->type() == QEvent::GraphicsSceneMouseMove || event->type() == QEvent::GraphicsSceneHoverMove) {
		QPointF scene_pos =
		  event->type() == QEvent::GraphicsSceneMouseMove ? static_cast<QGraphicsSceneMouseEvent*>(event)->scenePos() : static_cast<QGraphicsSceneHoverEvent*>(event)->scenePos();
		scene_pos = area()->mapFromScene(scene_pos);

		if ((hover = findItemPoint(area(), scene_pos, hoverPt)))
			end = hover->sceneMap()->invTransform(hoverPt);
		else
			end = area()->mapToScene(scene_pos);
		this->prepareGeometryChange();
		return false;
	}
	else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
		QGraphicsSceneMouseEvent* evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		if (evt->button() != Qt::LeftButton)
			return false;

		if (begin != end) {
			if (hover) {

				end = hover->sceneMap()->invTransform(hoverPt);
				Q_EMIT distanceCreated(begin, end);
				this->deleteLater();
			}
			else {
				this->deleteLater();
			}
		}
		return false;
	}

	return false;
}

bool VipDrawDistance2Points::eventFilter(QObject*, QEvent* event)
{
	if (!area())
		return false;

	if (event->type() == QEvent::MouseButtonPress) {
		QPoint pt = QCursor::pos();
		QRect view_rect = area()->view()->mapFromScene(area()->boundingRect()).boundingRect();
		view_rect = QRect(area()->view()->mapToGlobal(view_rect.topLeft()), area()->view()->mapToGlobal(view_rect.bottomRight()));
		if (!view_rect.contains(pt)) {
			this->deleteLater();
			return true;
		}
	}

	return false;
}

VipUpdatePlotPlayer::VipUpdatePlotPlayer(VipPlotPlayer* player)
  : QObject(player)
  , m_player(player)
{
	player->setProperty("VipUpdatePlotPlayer", true);
	m_drawDist = m_player->advancedTools()->menu()->addAction(vipIcon("distance_points.png"), "Distance between points");
	m_drawDist->setCheckable(true);
	m_drawDist->setToolTip("<b>Compute distance between 2 points</b><br>"
			       "Click successively on 2 curve points to get the x and y difference, and the Euclidean distance.");
	connect(m_drawDist, SIGNAL(triggered(bool)), this, SLOT(startDistance(bool)));

	player->toolBar()->addSeparator();
	m_local_minmax = new QToolButton();
	m_local_minmax->setAutoRaise(true);
	m_local_minmax->setToolTip("<b>Display curves minimum and maximum</b><br>Min/Max Y values are computed on the current visible abscissa interval.");
	m_local_minmax->setIcon(vipIcon("local_minmax.png"));
	m_local_minmax->setCheckable(true);
	player->toolBar()->addWidget(m_local_minmax);
	connect(m_local_minmax, SIGNAL(clicked(bool)), this, SLOT(setMarkersEnabled(bool)));

	QMenu* display_pos = new QMenu(m_local_minmax);
	m_minmaxPos = display_pos->addAction("Display minimum/maximum X positions");
	m_minmaxPos->setCheckable(true);
	connect(m_minmaxPos, SIGNAL(triggered(bool)), this, SLOT(setDisplayMarkerPos(bool)));
	m_local_minmax->setMenu(display_pos);
	m_local_minmax->setPopupMode(QToolButton::MenuButtonPopup);

	m_updateTimer.setInterval(1000);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateMarkers()));

	connect(VipPlayerLifeTime::instance(), SIGNAL(destroyed(VipAbstractPlayer*)), this, SLOT(stopMarkers(VipAbstractPlayer*)));

	// read properties and restore state
	setMarkersEnabled(player->property("_vip_customMarkersEnabled").toBool());
	setDisplayMarkerPos(player->property("_vip_customDisplayMarkerPos").toBool());
}

VipUpdatePlotPlayer::~VipUpdatePlotPlayer()
{
	disconnect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateMarkers()));
	m_updateTimer.stop();
}

void VipUpdatePlotPlayer::startDistance(bool)
{
	if (VipPlotPlayer* p = m_player) {
		if (m_drawDist->isChecked()) {
			p->selectionZoomArea(false);
			VipDrawDistance2Points* draw = new VipDrawDistance2Points(p->plotWidget2D()->area());
			connect(draw, SIGNAL(destroyed(QObject*)), this, SLOT(endDistance()));
			connect(draw, SIGNAL(distanceCreated(const VipPoint&, const VipPoint&)), this, SLOT(distanceCreated(const VipPoint&, const VipPoint&)), Qt::QueuedConnection);
		}
		else {
		}
	}
}

void VipUpdatePlotPlayer::endDistance()
{
	m_drawDist->blockSignals(true);
	m_drawDist->setChecked(false);
	m_drawDist->blockSignals(false);
}

#include <qmessagebox.h>
void VipUpdatePlotPlayer::distanceCreated(const VipPoint& start, const VipPoint& end)
{
	QString xunit;
	vip_double factor = 1;
	if (m_player) {
		xunit = m_player->timeUnit();
		factor = m_player->timeFactor();
	}
	QString text = "<b>Diff x (second - first): </b>" + QString::number((double)((end.x() - start.x()) / factor)) + " " + xunit + "<br>";
	text += "<b>Diff y (second - first): </b>" + QString::number((double)(end.y() - start.y())) + "<br>";

	vip_double x2 = ((end.x() - start.x()) / factor) * ((end.x() - start.x()) / factor);
	vip_double y2 = (end.y() - start.y()) * (end.y() - start.y());

	text += "<b>Euclidean distance: " + (xunit.isEmpty() ? QString() : ("(time in " + xunit + ")")) + ": </b>" + QString::number(qSqrt((double)(x2 + y2)));

	QMessageBox mb(QMessageBox::Information, "Distance between points", text, QMessageBox::Ok, m_player);
	mb.setTextInteractionFlags(Qt::TextSelectableByMouse);
	/*int dialogResult =*/mb.exec();
}

void VipUpdatePlotPlayer::setDisplayMarkerPos(bool enable)
{
	m_minmaxPos->blockSignals(true);
	m_minmaxPos->setChecked(enable);
	m_minmaxPos->blockSignals(false);
	if (VipPlotPlayer* p = m_player) {
		p->setProperty("_vip_customDisplayMarkerPos", enable);
	}
}

void VipUpdatePlotPlayer::setMarkersEnabled(bool enable)
{
	m_local_minmax->blockSignals(true);
	m_local_minmax->setChecked(enable);
	m_local_minmax->blockSignals(false);
	if (VipPlotPlayer* p = m_player) {
		p->setProperty("_vip_customMarkersEnabled", enable);
		if (enable) {
			VipAbstractPlotArea* area = p->viewer()->area();
			connect(area, SIGNAL(itemDataChanged(VipPlotItem*)), this, SLOT(updateMarkers()));
			connect(area, SIGNAL(itemAdded(VipPlotItem*)), this, SLOT(updateMarkers()));
			connect(area, SIGNAL(itemRemoved(VipPlotItem*)), this, SLOT(updateMarkers()));
			connect(p->defaultXAxis(), SIGNAL(scaleDivChanged(bool)), this, SLOT(updateMarkers()));
			connect(p->verticalWindow()->rawData().shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(updateMarkers()));
			connect(p->verticalWindow(), SIGNAL(visibilityChanged(VipPlotItem*)), this, SLOT(updateMarkers()));
			updateMarkers();
			m_updateTimer.start();
		}
		else {
			VipAbstractPlotArea* area = p->viewer()->area();
			disconnect(area, SIGNAL(itemDataChanged(VipPlotItem*)), this, SLOT(updateMarkers()));
			disconnect(area, SIGNAL(itemAdded(VipPlotItem*)), this, SLOT(updateMarkers()));
			disconnect(area, SIGNAL(itemRemoved(VipPlotItem*)), this, SLOT(updateMarkers()));
			disconnect(p->defaultXAxis(), SIGNAL(scaleDivChanged(bool)), this, SLOT(updateMarkers()));
			disconnect(p->verticalWindow()->rawData().shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(updateMarkers()));
			disconnect(p->verticalWindow(), SIGNAL(visibilityChanged(VipPlotItem*)), this, SLOT(updateMarkers()));

			for (QMap<VipPlotItem*, QList<VipPlotMarker*>>::iterator it = m_minMarkers.begin(); it != m_minMarkers.end(); ++it) {
				for (int i = 0; i < it.value().size(); ++i)
					it.value()[i]->setVisible(false);
			}
			for (QMap<VipPlotItem*, QList<VipPlotMarker*>>::iterator it = m_maxMarkers.begin(); it != m_maxMarkers.end(); ++it) {
				for (int i = 0; i < it.value().size(); ++i)
					it.value()[i]->setVisible(false);
			}
			m_updateTimer.stop();
		}
	}
}

void VipUpdatePlotPlayer::stopMarkers(VipAbstractPlayer* pl)
{
	if (pl == m_player) {
		disconnect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateMarkers()));
		m_updateTimer.stop();
	}
}

void VipUpdatePlotPlayer::updateMarkers()
{

	if (VipPlotPlayer* p = m_player) {
		// TODO: crash when closing player
		// Set markers color to curve color, add to Text 'Max:' and 'Min:'
		VipAbstractPlotArea* area = p->viewer()->area();
		disconnect(area, SIGNAL(itemDataChanged(VipPlotItem*)), this, SLOT(updateMarkers()));
		disconnect(area, SIGNAL(itemAdded(VipPlotItem*)), this, SLOT(updateMarkers()));
		disconnect(area, SIGNAL(itemRemoved(VipPlotItem*)), this, SLOT(updateMarkers()));

		VipInterval bounds = p->defaultXAxis()->scaleDiv().bounds();
		if (p->displayVerticalWindow()) {
			QRectF r = p->verticalWindow()->rawData().polygon().boundingRect();
			VipInterval inter(r.left(), r.right());
			VipInterval intersect = inter.intersect(bounds);
			if (intersect.isValid())
				bounds = intersect;
		}

		QList<VipPlotItem*> items = p->viewer()->area()->findItems<VipPlotItem*>();
		QList<VipPlotCurve*> curves;
		// compute curves and curves max z value
		double level = -DBL_MAX;
		for (int i = 0; i < items.size(); ++i)
			if (VipPlotCurve* c = qobject_cast<VipPlotCurve*>(items[i])) {
				if (c->zValue() > level)
					level = c->zValue();
				curves.append(c);
			}

		QRectF canvas = p->viewer()->area()->canvas()->boundingRect();

		for (int i = 0; i < curves.size(); ++i) {
			VipPlotCurve* curve = curves[i];

			QList<VipPlotMarker*>& maxs = m_maxMarkers[curve];
			QList<VipPlotMarker*>& mins = m_minMarkers[curve];

			// create missing markers, hide additional ones
			if (maxs.size() != curve->vectors().size()) {
				for (int j = maxs.size(); j < curve->vectors().size(); ++j) {

					VipPlotMarker* max = new VipPlotMarker();
					max->setIgnoreStyleSheet(true);
					max->setLineStyle(VipPlotMarker::NoLine);
					max->setItemAttribute(VipPlotMarker::ClipToScaleRect, false);
					max->setItemAttribute(VipPlotMarker::HasToolTip, false);
					max->setItemAttribute(VipPlotMarker::AutoScale, false);
					max->setItemAttribute(VipPlotMarker::IgnoreMouseEvents, true);
					{
						VipSymbol* s = new VipSymbol();
						s->setSize(10, 10);
						s->setStyle(VipSymbol::DTriangle);
						max->setSymbol(s);
						max->setSymbolVisible(true);
						s->setPen(Qt::white);
						s->setBrush(Qt::red);
					}
					max->setAxes(curve->axes(), curve->coordinateSystemType());
					max->setRenderHints(QPainter::Antialiasing);
					max->setLabelAlignment(Qt::AlignRight);
					max->setProperty("_vip_no_serialize", true); // avoid serialization of VipPlotMarker
					maxs.append(max);

					VipPlotMarker* min = new VipPlotMarker();
					min->setIgnoreStyleSheet(true);
					min->setLineStyle(VipPlotMarker::NoLine);
					min->setItemAttribute(VipPlotMarker::HasToolTip, false);
					min->setItemAttribute(VipPlotMarker::ClipToScaleRect, false);
					min->setItemAttribute(VipPlotMarker::AutoScale, false);
					min->setItemAttribute(VipPlotMarker::IgnoreMouseEvents, true);
					VipSymbol* s = new VipSymbol();
					s->setSize(10, 10);
					s->setStyle(VipSymbol::Triangle);
					s->setPen(Qt::white);
					s->setBrush(Qt::blue);
					min->setSymbol(s);
					min->setSymbolVisible(true);
					min->setAxes(curve->axes(), curve->coordinateSystemType());
					min->setRenderHints(QPainter::Antialiasing);
					min->setLabelAlignment(Qt::AlignRight);
					min->setProperty("_vip_no_serialize", true);
					mins.append(min);
				}
			}

			// compute min/max
			const QList<VipPointVector> vectors = curve->vectors();

			// hide additional ones
			for (int j = vectors.size(); j < maxs.size(); ++j) {
				maxs[j]->setVisible(false);
				mins[j]->setVisible(false);
			}
			if (!curve->isVisible()) {
				for (int j = 0; j < vectors.size(); ++j) {
					maxs[j]->setVisible(false);
					mins[j]->setVisible(false);
				}
				continue;
			}

			for (int j = 0; j < vectors.size(); ++j) {
				const VipPointVector& vec = vectors[j];
				if (vec.size()) {
					int max = -1;
					int min = -1;
					bool inside = false;
					for (int index = 0; index < vec.size(); ++index) {
						if (bounds.contains(vec[index].x())) {
							inside = true;
							if (max == -1) {
								max = min = index;
							}
							else {
								if (vec[index].y() > vec[max].y())
									max = index;
								if (vec[index].y() < vec[min].y())
									min = index;
							}
						}
						else if (inside)
							break;
					}
					if (max == -1) {
						maxs[j]->setVisible(false);
						mins[j]->setVisible(false);
						continue;
					}

					maxs[j]->blockSignals(true);
					mins[j]->blockSignals(true);

					maxs[j]->setVisible(true);
					maxs[j]->setZValue(level + 1);
					maxs[j]->setRawData(vec[max]);
					mins[j]->setVisible(true);
					mins[j]->setZValue(level);
					mins[j]->setRawData(vec[min]);
					maxs[j]->symbol()->setBrush(curve->pen().color());
					mins[j]->symbol()->setBrush(curve->pen().color());

					// set the texts
					VipText min_t;
					if (m_minmaxPos->isChecked())
						min_t.setText("<b>Min: " + QString::number(vec[min].y()) +
							      "</b><br>(x:" + curve->axes()[0]->scaleDraw()->label(vec[min].x(), VipScaleDiv::MajorTick).text() + ")");
					else
						min_t.setText("<b>Min: " + QString::number(vec[min].y()) + "</b>");
					min_t.setTextPen(curve->pen().color());
					min_t.setBackgroundBrush(QBrush(QColor(255, 255, 255, 160)));

					VipText max_t;
					if (m_minmaxPos->isChecked())
						max_t.setText("<b>Max: " + QString::number(vec[max].y()) +
							      "</b><br>(x:" + curve->axes()[0]->scaleDraw()->label(vec[max].x(), VipScaleDiv::MajorTick).text() + ")");
					else
						max_t.setText("<b>Max: " + QString::number(vec[max].y()) + "</b>");
					max_t.setTextPen(curve->pen().color());
					max_t.setBackgroundBrush(QBrush(QColor(255, 255, 255, 160)));

					QPointF min_pos = mins[j]->sceneMap()->transform(mins[j]->rawData());
					QPointF max_pos = maxs[j]->sceneMap()->transform(maxs[j]->rawData());
					Qt::Alignment min_align = Qt::AlignRight;
					if (canvas.right() - min_pos.x() < 30)
						min_align = Qt::AlignLeft;
					if (canvas.bottom() - min_pos.y() < 30)
						min_align = Qt::AlignTop | Qt::AlignHCenter;
					else if (min_pos.y() - canvas.top() < 30)
						min_align |= Qt::AlignBottom | Qt::AlignHCenter;
					;

					Qt::Alignment max_align = Qt::AlignRight;
					if (canvas.right() - max_pos.x() < 30)
						max_align = Qt::AlignLeft;
					if (canvas.bottom() - max_pos.y() < 30)
						max_align |= Qt::AlignTop | Qt::AlignHCenter;
					else if (max_pos.y() - canvas.top() < 30)
						max_align |= Qt::AlignBottom | Qt::AlignHCenter;
					;

					mins[j]->setLabelAlignment(min_align);
					maxs[j]->setLabelAlignment(max_align);
					mins[j]->setLabel(min_t);
					maxs[j]->setLabel(max_t);

					// set the transform
					QTransform min_tr;
					min_tr.translate(0, mins[j]->symbol()->size().height() / 2);
					QTransform max_tr;
					max_tr.translate(0, -maxs[j]->symbol()->size().height() / 2);
					mins[j]->setTransform(min_tr);
					maxs[j]->setTransform(max_tr);

					maxs[j]->blockSignals(false);
					mins[j]->blockSignals(false);
				}
				else {
					maxs[j]->setVisible(false);
					mins[j]->setVisible(false);
				}
			}
		}

		connect(area, SIGNAL(itemDataChanged(VipPlotItem*)), this, SLOT(updateMarkers()));
		connect(area, SIGNAL(itemAdded(VipPlotItem*)), this, SLOT(updateMarkers()));
		connect(area, SIGNAL(itemRemoved(VipPlotItem*)), this, SLOT(updateMarkers()));
	}
}

static void updatePlotPlayer(VipPlotPlayer* pl)
{
	if (pl && !pl->property("VipUpdatePlotPlayer").toBool())
		new VipUpdatePlotPlayer(pl);
	else if (pl) {
		if (VipUpdatePlotPlayer* u = pl->findChild<VipUpdatePlotPlayer*>()) {
			u->setMarkersEnabled(pl->property("_vip_customMarkersEnabled").toBool());
			u->setDisplayMarkerPos(pl->property("_vip_customDisplayMarkerPos").toBool());
		}
	}
}
int VipUpdatePlotPlayer::registerClass()
{
	// register the VipUpdateVideoPlayer class
	vipFDPlayerCreated().append<void(VipPlotPlayer*)>(updatePlotPlayer);
	return 0;
}

static bool initVideo = vipAddInitializationFunction(VipUpdateVideoPlayer::registerClass);
static bool initPlot = vipAddInitializationFunction(VipUpdatePlotPlayer::registerClass);
