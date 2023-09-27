#pragma once


#include "VipStandardWidgets.h"
#include "VipSimpleAnnotation.h"
#include "VipToolWidget.h"
#include "VipPlotShape.h"
#include "VipPlayer.h"

class QTextEdit;

///Edit the text and other parameters of an annotation
class VIP_GUI_EXPORT TextEditor : public QWidget
{
	Q_OBJECT
public:
	TextEditor();
	~TextEditor();

	QTextEdit * editor() const;

	///Style of the text box: 'No box', 'Full box', 'Underline'
	QString boxStyle() const;
	int vipSides() const;

	///Text alignment: 'Top','Left', 'Right', 'Bottom', 'TopLeft', 'TopRight', 'BottomLeft', 'BottomRight', 'Center'
	QString textAlignment() const;
	int qtAlignment() const;

	///Text position: 'Inside', 'X Inside' , 'Y Inside', 'Outside'
	QString textPosition() const;
	int vipTextPosition() const;

	void setShapes(const QList<VipPlotShape*>&);
	void updateShapes(const QList<VipPlotShape*>&);

Q_SIGNALS:
	void changed();
private Q_SLOTS:
	void emitChanged();
	void changeFont();
private:
	class PrivateData;
	PrivateData * m_data;
};

///Edit the pen and brush of an annotation
class VIP_GUI_EXPORT ShapeEditor : public QWidget
{
	Q_OBJECT

public:
	ShapeEditor();
	~ShapeEditor();
	QPen pen() const;
	QBrush brush() const;
	void setShapes(const QList<VipPlotShape*>&);
	void updateShapes(const QList<VipPlotShape*>&);

private Q_SLOTS:
	void emitChanged() { Q_EMIT changed(); }
Q_SIGNALS:
	void changed() const;
private:
	class PrivateData;
	PrivateData * m_data;
};

///Edit the symbol of an annotation
class VIP_GUI_EXPORT SymbolEditor : public QWidget
{
	Q_OBJECT

public:
	SymbolEditor();
	~SymbolEditor();

	int symbolType() const;
	int symbolSize() const;
	int arrowAngle() const;
	void setShapes(const QList<VipPlotShape*>&);
	void updateShapes(const QList<VipPlotShape*>&);

private Q_SLOTS:
	void emitChanged() { Q_EMIT changed(); }
Q_SIGNALS:
	void changed() const;
private:
	class PrivateData;
	PrivateData * m_data;
};



///Edit on or more annotations
class VIP_GUI_EXPORT VipAnnotationWidget : public QWidget
{
	Q_OBJECT

public:
	VipAnnotationWidget();
	~VipAnnotationWidget();

	TextEditor * textEditor() const;
	ShapeEditor * shapeEditor() const;
	SymbolEditor * symbolEditor() const;

	///Update annotation widgets based on given shapes
	void setShapes(const QList<VipPlotShape*>&);
	//Update the shapes based on this widget
	void updateShapes(const QList<VipPlotShape*>&);

private Q_SLOTS:
	void emitChanged() {
		Q_EMIT changed();
	}
Q_SIGNALS:
	void changed();
private:
	class PrivateData;
	PrivateData * m_data;
};



class VIP_GUI_EXPORT VipAnnotationToolWidget : public VipToolWidgetPlayer
{
	Q_OBJECT

public:
	VipAnnotationToolWidget(VipMainWindow * w);

	virtual bool setPlayer(VipAbstractPlayer *);

	VipAnnotationWidget * annotationWidget() const;

public Q_SLOTS:
	void updateShapes();
	void importShapes();
private:
	VipAnnotationWidget * m_widget;
	QPointer<VipPlayer2D> m_player;
};

/// Returns the global annotation widget
VIP_GUI_EXPORT VipAnnotationToolWidget * vipGetAnnotationToolWidget(VipMainWindow * w=NULL);
/// Create annotation from selected shapes inside given player and display the annotation widget
VIP_GUI_EXPORT void vipEditAnnotations(VipPlayer2D * player);
/// Remove annotation for selected shapes inside given player.
/// This won't delete the shapes, just remove the linked annotation.
VIP_GUI_EXPORT void vipRemoveAnnotations(VipPlayer2D * player);

/// Remove all annotations and related shapes from given player
VIP_GUI_EXPORT void vipRemoveAllAnnotations(VipPlayer2D * player);

/// Helper function.
/// Create an annotation inside given player.
///
/// @param type annotation type: 'line', 'arrow', 'rectangle', 'ellipse', or 'textbox'
/// @param text annotation text
/// @param start start position of the line, arrow, rectangle, ellipse (bounding rectangle) or text box.
/// @param end end position for line, arrow, rectangle or ellipse
/// @param attributes additional annotation attributes. see #vipAnnotation() for more details.
/// @param yaxis if not empty and for stacked plot only, yaxis is used to find the left axis where the annotation will be added
/// @param error is not NULL and one of the attributes has a wrong syntax, this string gives an error message
///
/// @return created annotation
VIP_GUI_EXPORT VipSimpleAnnotation* vipAnnotation(VipPlayer2D * player, const QString & type, const QString & text,
	const QPointF & start, const QPointF & end = QPointF(), const QVariantMap & attributes = QVariantMap(),
	const QString & yaxis = QString(), QString * error = NULL);
