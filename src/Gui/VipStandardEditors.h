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

#ifndef VIP_STANDARD_EDITORS_H
#define VIP_STANDARD_EDITORS_H

#include "VipAbstractScale.h"
#include "VipPlotCurve.h"
#include "VipPlotGrid.h"
#include "VipPlotHistogram.h"
#include "VipPlotWidget2D.h"
#include "VipStandardWidgets.h"
#include "VipSymbol.h"

#include <QCheckBox>
#include <QPointer>
#include <qgroupbox.h>
/// \addtogroup Gui
/// @{

// helper functions
/// Return all possible desynchronize scales for this item with a non empty title or object name
VIP_GUI_EXPORT QList<VipAbstractScale*> vipAllScales(VipPlotItem*);
VIP_GUI_EXPORT QStringList vipScaleNames(const QList<VipAbstractScale*>& scales);
/// Return the index (or -1 if not found) of the scale with given title or object name
VIP_GUI_EXPORT int vipIndexOfScale(const QList<VipAbstractScale*>& scales, const QString& name);

// helper function, return the title or object name of a VipPlotItem or VipAbstractScale
VIP_GUI_EXPORT QString vipItemName(QGraphicsObject* obj);

/// Widget used to edit a VipSymbol object.
class VIP_GUI_EXPORT VipSymbolWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(VipSymbol value READ getSymbol WRITE setSymbol)

public:
	VipSymbolWidget(QWidget* parent = nullptr);

	void setSymbol(const VipSymbol&);
	VipSymbol getSymbol() const;

	// void setColorOptionsVisible(bool);
	// bool colorOptionsVisible() const;

	VipPenButton* penEditor() const { return const_cast<VipPenButton*>(&m_pen_color); }
	VipPenButton* brushEditor() const { return const_cast<VipPenButton*>(&m_brush_color); }

private Q_SLOTS:
	void emitSymbolChanged();
	void redraw();

Q_SIGNALS:
	void symbolChanged(const VipSymbol&);

private:
	VipSymbol m_symbol;
	QComboBox m_style;
	QSpinBox m_size;
	VipPenButton m_pen_color;
	VipPenButton m_brush_color;
};

class VIP_GUI_EXPORT VipPlotItemWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(VipPlotItem* value READ getPlotItem WRITE setPlotItem)
public:
	VipPlotItemWidget(QWidget* parent = nullptr);
	~VipPlotItemWidget();

	void setPlotItem(VipPlotItem*);
	VipPlotItem* getPlotItem() const;
	void updatePlotItem(VipPlotItem*);

	void setTitleVisible(bool);
	bool titleVisible() const;

	VipLineEdit* title() const { return const_cast<VipLineEdit*>(&m_title); }

Q_SIGNALS:
	void plotItemChanged(VipPlotItem*);

private Q_SLOTS:
	void emitPlotItemChanged();

private:
	QPointer<VipPlotItem> m_item;
	QList<VipAbstractScale*> m_scales;

	QLabel m_titleLabel;
	// VipTextWidget
	VipLineEdit m_title;
	QCheckBox m_visible;
	QCheckBox m_antialiazed;
	QCheckBox m_drawText;
	QLabel m_xAxisLabel, m_yAxisLabel;
	QComboBox m_xAxis, m_yAxis;
};

/// Widget used to edit a VipPlotGrid object.
class VIP_GUI_EXPORT VipPlotGridWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(VipPlotGrid* value READ getGrid WRITE setGrid)

public:
	VipPlotGridWidget(QWidget* parent = nullptr);

	void setGrid(VipPlotGrid* grid);
	VipPlotGrid* getGrid() const;
	void updateGrid(VipPlotGrid* grid);

Q_SIGNALS:
	void gridChanged(VipPlotGrid*);

private Q_SLOTS:
	void emitGridChanged();

private:
	VipPlotItemWidget m_item;
	QCheckBox m_enableX, m_enableXMin;
	QCheckBox m_enableY, m_enableYMin;
	VipPenButton m_maj_pen;
	VipPenButton m_min_pen;
	QPointer<VipPlotGrid> m_grid;
};

/// Widget used to edit a VipPlotCanvas object.
class VIP_GUI_EXPORT VipPlotCanvasWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(VipPlotCanvas* value READ getCanvas WRITE setCanvas)

public:
	VipPlotCanvasWidget(QWidget* parent = nullptr);

	void setCanvas(VipPlotCanvas*);
	VipPlotCanvas* getCanvas() const;
	void updateCanvas(VipPlotCanvas*);

Q_SIGNALS:
	void canvasChanged(VipPlotCanvas*);

private Q_SLOTS:
	void emitCanvasChanged();

private:
	VipBoxStyleWidget m_inner;
	VipBoxStyleWidget m_outer;
	QPointer<VipPlotCanvas> m_canvas;
	// QPointer<VipPlotItemArea> m_area;
};

class VIP_GUI_EXPORT VipPlotCurveWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(VipPlotCurve* value READ getCurve WRITE setCurve)

public:
	VipPlotCurveWidget(QWidget* parent = nullptr);

	void setCurve(VipPlotCurve*);
	VipPlotCurve* getCurve() const;
	void updateCurve(VipPlotCurve*);

	VipPlotItemWidget* baseItemEditor() const { return const_cast<VipPlotItemWidget*>(&m_item); }
	VipBoxStyleWidget* styleEditor() const { return const_cast<VipBoxStyleWidget*>(&m_style); }
	VipSymbolWidget* symbolEditor() const { return const_cast<VipSymbolWidget*>(&m_symbol); }

Q_SIGNALS:
	void curveChanged(VipPlotCurve*);

private Q_SLOTS:
	void emitCurveChanged();

private:
	QGroupBox m_draw_line, m_draw_symbol;
	QComboBox m_line_style;
	VipDoubleEdit m_baseline;
	VipPlotItemWidget m_item;
	VipBoxStyleWidget m_style;
	VipSymbolWidget m_symbol;
	QPointer<VipPlotCurve> m_curve;
	// keep track of previous colors for style sheet
	QColor m_line;
	QColor m_back;
	QColor m_symbolPen;
	QColor m_symbolBack;
};

class VIP_GUI_EXPORT VipPlotHistogramWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(VipPlotHistogram* value READ getHistogram WRITE setHistogram)

public:
	VipPlotHistogramWidget(QWidget* parent = nullptr);

	void setHistogram(VipPlotHistogram*);
	VipPlotHistogram* getHistogram() const;
	void updateHistogram(VipPlotHistogram*);

Q_SIGNALS:
	void histogramChanged(VipPlotHistogram*);

private Q_SLOTS:
	void emitHistogramChanged();

private:
	VipPlotItemWidget m_item;
	VipBoxStyleWidget m_style;
	QComboBox m_histStyle;
	QPointer<VipPlotHistogram> m_histo;

	// keep track of colors for stylesheet
	QColor m_border;
	QColor m_back;
};

/// Widget used to edit the axises of a Plot object.
class VIP_GUI_EXPORT VipPlotAxisWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(VipAbstractScale* value READ getAxis WRITE setAxis)

public:
	VipPlotAxisWidget(QWidget* parent = nullptr);
	~VipPlotAxisWidget();

	void setAxis(VipAbstractScale* scale);
	VipAbstractScale* getAxis() const;
	void updateAxis(VipAbstractScale* scale);

private Q_SLOTS:
	void emitAxisChanged();

Q_SIGNALS:
	void axisChanged(VipAbstractScale*);

private:
	VipTextWidget m_title;
	VipTextWidget m_labels;
	QCheckBox m_labelVisible;
	QCheckBox m_visible;
	QCheckBox m_auto_scale;
	VipDoubleEdit m_min;
	VipDoubleEdit m_max;
	QCheckBox m_log;
	QSpinBox m_maj_grad;
	QSpinBox m_min_grad;
	QCheckBox m_manualExponent;
	QSpinBox m_exponent;
	VipPenButton m_pen;

	QPointer<VipAbstractScale> m_scale;
};

class VipAxisColorMap;
class VIP_GUI_EXPORT VipColorScaleWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(VipAxisColorMap* value READ colorScale WRITE setColorScale)

public:
	VipColorScaleWidget(QWidget* parent = nullptr);
	~VipColorScaleWidget();
	VipAxisColorMap* colorScale() const;

	static QPixmap colorMapPixmap(int color_map, const QSize& size, const QPen& pen = Qt::NoPen);

public Q_SLOTS:
	void setColorScale(VipAxisColorMap*);
	void updateColorScale();
	void emitColorScaleChanged();

Q_SIGNALS:
	void colorScaleChanged(VipAxisColorMap*);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VIP_GUI_EXPORT VipColorScaleButton : public QToolButton
{
	Q_OBJECT
public:
	VipColorScaleButton(QWidget* parent = nullptr);
	int colorPalette() const;

	static QMenu* generateColorScaleMenu();
public Q_SLOTS:
	void setColorPalette(int color_palette);

private Q_SLOTS:
	void menuTriggered(QAction* act);

Q_SIGNALS:
	void colorPaletteChanged(int);

private:
	int m_colorPalette;
};

class VipAbstractPlayer;
class VIP_GUI_EXPORT VipAbstractPlayerWidget : public QWidget
{
	Q_OBJECT

public:
	VipAbstractPlayerWidget(QWidget* parent = nullptr);
	~VipAbstractPlayerWidget();

	VipAbstractPlayer* abstractPlayer() const;
public Q_SLOTS:
	void setAbstractPlayer(VipAbstractPlayer*);

private Q_SLOTS:
	void itemChoiceChanged();
	void selectionChanged();
	void delayedSelectionChanged();

Q_SIGNALS:
	void abstractPlayerChanged(VipAbstractPlayer*);
	void itemChanged(QGraphicsObject*);

protected:
	virtual void showEvent(QShowEvent* evt);
	virtual void hideEvent(QHideEvent* evt);

private:
	void setEditor(QWidget*);
	void setPlayerInternal();

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// Widget used to specify the default characteristics of a VipPlotCurve and a VipPlotArea2D.
/// Among other things:
/// - Axes visibility
/// - Grid parameters
/// - Canvas parameters
/// - Curve parameters
/// - Antialiazing
class VIP_GUI_EXPORT VipDefaultPlotAreaSettings : public QWidget
{
	Q_OBJECT

public:
	VipDefaultPlotAreaSettings(QWidget* parent = nullptr);
	~VipDefaultPlotAreaSettings();

	VipPlotCurve* defaultCurve() const;
	void setDefaultCurve(VipPlotCurve* c);

	VipPlotArea2D* defaultPlotArea() const;
	void setDefaultPlotArea(VipPlotArea2D* area);

	void applyToCurve(VipPlotCurve*);
	void applyToArea(VipPlotArea2D*);

	bool shouldApplyToAllPlayers() const;
	void setShouldApplyToAllPlayers(bool);

private Q_SLOTS:
	void updateItems();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @}
// end Gui

#endif
