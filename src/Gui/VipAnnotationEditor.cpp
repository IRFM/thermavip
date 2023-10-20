#include "VipAnnotationEditor.h"
#include "VipCore.h"
#include "VipSymbol.h"
#include "VipQuiver.h"
#include "VipDisplayArea.h"
#include "VipPlayer.h"

#include <qtextedit.h>
#include <qboxlayout.h>
#include <qfontdialog.h>

static const QPen nullPen = QPen(QColor(128,128,128), 1);
static const QBrush nullBrush = QBrush(QColor(128, 128, 128), Qt::SolidPattern);
static const QFont nullFont = QFont();
static const int nullDistanceToText = -1;
static const int nullSymbolType = -2;
static const int nullSymbolSize = 0;
static const int nullBorderRadius = -1;
static const int nullArrowAngle = -1;
static const int nullAlignment = 0;
static const int nullPosition = -1;

static bool isNullPen(const QPen & p) { return p.color() == Qt::transparent && p.width() == 1 && p.style() == Qt::SolidLine; }
static bool isNullBrush(const QBrush & b) { return b.color() == Qt::transparent && b.style() == Qt::SolidPattern; }
static bool isNullFont(const QFont & f) { return f == QFont(); }
static bool isNullDistanceToText(int d) { return d == nullDistanceToText; }
static bool isNullBorderRadius(int r) { return r == nullBorderRadius; }
static bool isNullSymbolType(int t) { return t == nullSymbolType; }
static bool isNullSymbolSize(int s) { return s == nullSymbolSize; }
static bool isNullArrowAngle(int a) {return a == nullArrowAngle;}
static bool isNullAlignment(int a) { return a == nullAlignment; }
static bool isNullPosition(int a) { return a == nullPosition; }


static QString fontToStyleSheet(const QFont & font)
{
	QString res;
	res += "font-family:" + font.family() + ";";
	res += "font-style:" + QString(font.style() == QFont::StyleItalic ? "italic" : (font.style() == QFont::StyleOblique ? "oblique" : "normal")) + ";";
	if(font.bold())
		res += "font-weight: bold;";
	res += "font-size:" + QString::number(font.pointSize()) + "pt;";
	return res;
}


class TextEditor::PrivateData
{
public:
	QAction* font;
	VipPenButton* textPen;
	VipPenButton* textBrush;
	QSpinBox* boxRadius;
	QSpinBox * textDistance;
	QComboBox* boxStyle;
	QComboBox* textAlignment;
	QComboBox* textPosition;
	QToolBar bar;
	QTextEdit editor;
};

TextEditor::TextEditor()
	:QWidget()
{
	m_data = new PrivateData();

	m_data->bar.setIconSize(QSize(20, 20));

	m_data->font = m_data->bar.addAction(vipIcon("font.png"), "Edit text font");
	m_data->font->setProperty("font", QVariant::fromValue(QFont()));

	m_data->textPen = new VipPenButton();
	m_data->textPen->setMode(VipPenButton::Color);
	m_data->bar.addWidget(m_data->textPen);
	m_data->textPen->setToolTip("Edit text color");

	m_data->textBrush = new VipPenButton();
	m_data->textBrush->setMode(VipPenButton::Brush);
	m_data->bar.addWidget(m_data->textBrush);
	m_data->textBrush->setToolTip("Edit text background brush");

	m_data->boxRadius = new QSpinBox();
	m_data->boxRadius->setToolTip("Text box border radius");
	m_data->boxRadius->setRange(-1, 20);
	m_data->boxRadius->setSpecialValueText(" ");
	m_data->boxRadius->setValue(0);
	m_data->boxRadius->setMaximumWidth(50);
	m_data->bar.addWidget(m_data->boxRadius);

	m_data->bar.addWidget(new QLabel());

	m_data->textDistance = new QSpinBox();
	m_data->textDistance->setToolTip("Distance between the text and the shape");
	m_data->textDistance->setRange(-1, 100);
	m_data->textDistance->setValue(0);
	m_data->textDistance->setSpecialValueText(" ");
	m_data->textDistance->setMaximumWidth(50);
	m_data->bar.addWidget(m_data->textDistance);

	m_data->bar.addWidget(new QLabel());
	m_data->bar.addSeparator();
	m_data->bar.addWidget(new QLabel());

	m_data->boxStyle = new QComboBox();
	m_data->boxStyle->setToolTip("Edit text box style");
	m_data->boxStyle->addItems(QStringList() <<QString()<< "No box"<< "Full box"<< "Underline");
	m_data->bar.addWidget(m_data->boxStyle);
	m_data->boxStyle->setMaximumWidth(80);

	m_data->bar.addWidget(new QLabel());

	m_data->textAlignment = new QComboBox();
	m_data->textAlignment->setToolTip("Text alignment");
	m_data->textAlignment->addItems(QStringList() << QString() << "Center"<< "Top"<< "Left"<<"Right"<< "Bottom"<< "TopLeft"<< "TopRight"<< "BottomLeft"<< "BottomRight");
	m_data->textAlignment->setMaximumWidth(80);
	m_data->bar.addWidget(m_data->textAlignment);

	m_data->bar.addWidget(new QLabel());

	m_data->textPosition = new QComboBox();
	m_data->textPosition->setToolTip("Text position around the shape");
	m_data->textPosition->addItems(QStringList() << QString() << "Inside" << "X Inside" << "Y Inside" <<"Outside" );
	m_data->textPosition->setMaximumWidth(80);
	m_data->bar.addWidget(m_data->textPosition);

	m_data->editor.setStyleSheet(fontToStyleSheet(QFont()));
	m_data->editor.setFont(QFont());

	QVBoxLayout * lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(0);
	lay->addWidget(&m_data->bar);
	lay->addWidget(&m_data->editor);
	setLayout(lay);

	connect(&m_data->editor, SIGNAL(textChanged()), this, SLOT(emitChanged()));
	connect(m_data->font, SIGNAL(triggered(bool)), this, SLOT(changeFont()));
	connect(m_data->textPen, SIGNAL(penChanged(const QPen &)), this, SLOT(emitChanged()));
	connect(m_data->textBrush, SIGNAL(penChanged(const QPen &)), this, SLOT(emitChanged()));
	connect(m_data->boxRadius,SIGNAL(valueChanged(int)), this, SLOT(emitChanged()));
	connect(m_data->textDistance, SIGNAL(valueChanged(int)), this, SLOT(emitChanged()));
	connect(m_data->boxStyle, SIGNAL(currentIndexChanged(int)), this, SLOT(emitChanged()));
	connect(m_data->textAlignment, SIGNAL(currentIndexChanged(int)), this, SLOT(emitChanged()));
	connect(m_data->textPosition, SIGNAL(currentIndexChanged(int)), this, SLOT(emitChanged()));
}


TextEditor::~TextEditor()
{
	delete m_data;
}

void TextEditor::changeFont()
{
	QFontDialog dialog;
	dialog.setFont(m_data->editor.font());
	if (dialog.exec() == QDialog::Accepted) {
		m_data->editor.setFont(dialog.selectedFont());
		m_data->editor.setStyleSheet(fontToStyleSheet(dialog.selectedFont()));
		emitChanged();
	}
}

void TextEditor::emitChanged() {

	if (!isNullPen(m_data->textPen->pen()))
		m_data->editor.setTextColor(m_data->textPen->pen().color());
	if (sender() == m_data->font || sender() == m_data->textPen) {
		QString t = m_data->editor.toPlainText();
		m_data->editor.blockSignals(true);
		m_data->editor.clear();
		m_data->editor.setPlainText(t);
		m_data->editor.blockSignals(false);
	}
	Q_EMIT changed();
}

QTextEdit * TextEditor::editor() const
{
	return &m_data->editor;
}

QString TextEditor::boxStyle() const
{
	return m_data->boxStyle->currentText();
}
int TextEditor::vipSides() const
{
	QString t = boxStyle();
	if (t == "Full box") return Vip::AllSides;
	else if (t == "Underline") return Vip::Bottom;
	else return Vip::NoSide;
}

QString TextEditor::textAlignment() const
{
	return m_data->textAlignment->currentText();
}
int TextEditor::qtAlignment() const
{
	QString a = m_data->textAlignment->currentText();
	if (a.isEmpty())
		return nullAlignment;
	else if (a == "Top") return Qt::AlignTop |Qt::AlignHCenter;
	else if (a == "Left") return Qt::AlignLeft | Qt::AlignVCenter;
	else if (a == "Right") return Qt::AlignRight | Qt::AlignVCenter;
	else if (a == "Bottom") return Qt::AlignBottom | Qt::AlignHCenter;
	else if (a == "TopLeft") return Qt::AlignTop |Qt::AlignLeft;
	else if (a == "TopRight") return Qt::AlignTop | Qt::AlignRight;
	else if (a == "BottomLeft") return Qt::AlignBottom | Qt::AlignLeft;
	else if (a == "BottomRight") return Qt::AlignBottom | Qt::AlignRight;
	else if (a == "Center") return Qt::AlignCenter;
	else return nullAlignment;
}

QString TextEditor::textPosition() const
{
	return m_data->textPosition->currentText();
}
int TextEditor::vipTextPosition() const {
	QString t = textPosition();
	if (t == "Inside") return Vip::XInside | Vip::YInside;
	else if (t == "X Inside") return Vip::XInside;
	else if (t == "Y Inside") return Vip::YInside;
	else if (t == "Outside") return Vip::Outside;
	else return nullPosition;
}



static QString sidesToString(int sides)
{
	return sides == Vip::Bottom ? "Underline" : (sides == Vip::AllSides ? "Full box" : (sides == Vip::NoSide ? "No box" : ""));
}
static QString positionToString(int text_pos)
{
	if (text_pos == (Vip::XInside | Vip::YInside)) return "Inside";
	else if (text_pos == Vip::XInside) return "X Inside";
	else if (text_pos == Vip::YInside) return "Y Inside";
	else if(text_pos == Vip::Outside) return "Outside";
	else return QString();
}
static QString alignmentToString(int alignment)
{
	if (alignment == (Qt::AlignTop | Qt::AlignHCenter)) return "Top";
	else if (alignment == (Qt::AlignLeft | Qt::AlignVCenter)) return "Left";
	else if (alignment == (Qt::AlignRight | Qt::AlignVCenter)) return "Right";
	else if (alignment == (Qt::AlignBottom | Qt::AlignHCenter)) return "Bottom";
	else if (alignment == (Qt::AlignTop | Qt::AlignLeft)) return "TopLeft";
	else if (alignment == (Qt::AlignTop | Qt::AlignRight)) return "TopRight";
	else if (alignment == (Qt::AlignBottom | Qt::AlignLeft)) return "BottomLeft";
	else if (alignment == (Qt::AlignBottom | Qt::AlignRight)) return "BottomRight";
	else if (alignment == Qt::AlignCenter) return "Center";
	else return QString();
}

void TextEditor::setShapes(const QList<VipPlotShape*>& shapes)
{
	//check the number of closed shapes
	QList<VipShape> closed;
	QString text;
	QFont font = nullFont;
	QPen textPen = nullPen;
	QBrush textBrush = nullBrush;
	QString boxStyle;
	int textAlign = nullAlignment;
	int textPos = nullPosition;
	int radius = nullBorderRadius;
	int textDistance = nullDistanceToText;

	bool first = true;
	for (int i = 0; i < shapes.size(); ++i)
	{
		if (VipAnnotation*annot = shapes[i]->annotation())
			if (strcmp(annot->name(), "VipSimpleAnnotation") == 0) {
				VipSimpleAnnotation * a = static_cast<VipSimpleAnnotation*>(annot);

				if (first) {
					text = a->text().text();
					font = a->text().font();
					textPen = a->text().textPen();
					textBrush = a->text().backgroundBrush();
					boxStyle = sidesToString(a->text().boxStyle().drawLines());
					textAlign = a->textAlignment();
					textPos = a->textPosition();
					radius = a->text().borderRadius();
					textDistance = a->textDistance();
					first = false;
				}
				else {
					if (a->text().text() != text) text = QString();
					if (a->text().font() != font) font = nullFont;
					if (a->text().textPen().color() != textPen.color()) textPen = nullPen;
					if (a->text().backgroundBrush() != textBrush) textBrush = nullBrush;
					if (sidesToString(a->text().boxStyle().drawLines()) != boxStyle) boxStyle = QString();
					if (textAlign != a->textAlignment()) textAlign = nullAlignment;
					if (textPos != a->textPosition()) textPos = nullPosition;
					if (radius != a->text().borderRadius()) radius = nullBorderRadius;
					if (textDistance != a->textDistance()) textDistance =nullDistanceToText;
				}
			}
	}

	blockSignals(true);

	m_data->editor.setPlainText(text);
	m_data->editor.setFont(font);
	m_data->textPen->setPen(textPen);
	m_data->textBrush->setBrush(textBrush);
	m_data->boxStyle->setCurrentText(boxStyle);
	m_data->textAlignment->setCurrentText(alignmentToString(textAlign));
	m_data->textPosition->setCurrentText(positionToString( textPos));
	m_data->boxRadius->setValue(radius);
	m_data->textDistance->setValue(textDistance);

	QFont f = m_data->font->property("font").value<QFont>();
	if (!isNullFont(f))
		m_data->editor.setFont(f);
	if (!isNullPen(m_data->textPen->pen())) {
		QString t = m_data->editor.toPlainText();
		m_data->editor.clear();
		m_data->editor.setTextColor(m_data->textPen->pen().color());
		m_data->editor.setPlainText(t);
	}

	blockSignals(false);

}


void TextEditor::updateShapes(const QList<VipPlotShape*>& shapes)
{
	for (int i = 0; i < shapes.size(); ++i)
	{
		if (VipAnnotation*annot = shapes[i]->annotation())
			if (strcmp(annot->name(), "VipSimpleAnnotation") == 0) {
				VipSimpleAnnotation * a = static_cast<VipSimpleAnnotation*>(annot);

				if (m_data->editor.toPlainText().size()) a->setText(m_data->editor.toPlainText());
				if (!isNullFont(m_data->editor.font())) a->text().setFont(m_data->editor.font());
				if (!isNullPen(m_data->textPen->pen())) a->text().setTextPen(m_data->textPen->pen());
				if (!isNullBrush(m_data->textBrush->pen().brush())) a->text().setBackgroundBrush(m_data->textBrush->pen().brush());
				if (!isNullAlignment(qtAlignment())) a->setTextAlignment((Qt::Alignment)qtAlignment());
				if (!isNullPosition(vipTextPosition())) a->setTextPosition((Vip::RegionPositions) vipTextPosition());
				if (!isNullBorderRadius(m_data->boxRadius->value())) a->text().boxStyle().setBorderRadius(m_data->boxRadius->value());
				if (!isNullDistanceToText(m_data->textDistance->value())) a->setTextDistance(m_data->textDistance->value());
				if (!m_data->boxStyle->currentText().isEmpty()) {
					a->text().boxStyle().setDrawLines((Vip::Sides) vipSides());
				}

				shapes[i]->update();
			}
	}
}




class ShapeEditor::PrivateData
{
public:
	VipPenButton pen;
	VipPenButton brush;
};
ShapeEditor::ShapeEditor()
{
	m_data = new PrivateData();

	m_data->pen.setMode(VipPenButton::Pen);
	m_data->pen.setText("Annotation border pen");
	m_data->pen.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_data->brush.setMode(VipPenButton::Brush);
	m_data->brush.setText("Annotation background brush");
	m_data->brush.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	QHBoxLayout * lay = new QHBoxLayout();
	//lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(5);
	lay->addWidget(&m_data->pen);
	lay->addWidget(&m_data->brush);
	setLayout(lay);

	connect(&m_data->pen, SIGNAL(penChanged(const QPen&)), this, SLOT(emitChanged()));
	connect(&m_data->brush, SIGNAL(penChanged(const QPen&)), this, SLOT(emitChanged()));
}
ShapeEditor::~ShapeEditor()
{
	delete m_data;
}

QPen ShapeEditor::pen() const { return m_data->pen.pen(); }
QBrush ShapeEditor::brush() const { return m_data->brush.pen().brush(); }

void ShapeEditor::setShapes(const QList<VipPlotShape*>& shapes)
{
	QPen pen = nullPen;
	QBrush brush = nullBrush;
	bool first = true;

	for (int i = 0; i < shapes.size(); ++i)
	{
		if (VipAnnotation*annot = shapes[i]->annotation())
			if (strcmp(annot->name(), "VipSimpleAnnotation") == 0) {
				VipSimpleAnnotation * a = static_cast<VipSimpleAnnotation*>(annot);

				if (first) {
					pen = a->pen();
					brush = a->brush();
					first = false;
				}
				else {
					if (pen != a->pen()) pen = nullPen;
					if (brush != a->brush()) brush = nullBrush;
				}
			}
	}

	blockSignals(true);
	m_data->pen.setPen(pen);
	m_data->brush.setBrush(brush);
	blockSignals(false);
}
void ShapeEditor::updateShapes(const QList<VipPlotShape*>& shapes)
{
	for (int i = 0; i < shapes.size(); ++i)
	{
		if (VipAnnotation*annot = shapes[i]->annotation())
			if (strcmp(annot->name(), "VipSimpleAnnotation") == 0) {
				VipSimpleAnnotation * a = static_cast<VipSimpleAnnotation*>(annot);

				if (!isNullPen(m_data->pen.pen())) {
					a->setPen(m_data->pen.pen());
					a->text().boxStyle().setBorderPen(m_data->pen.pen());
				}
				if (!isNullBrush(m_data->brush.pen().brush())) a->setBrush(m_data->brush.pen().brush());

				shapes[i]->update();
			}
	}
}



class SymbolEditor::PrivateData
{
public:
	QComboBox type;
	QSpinBox size;
	QSpinBox angle;
};
SymbolEditor::SymbolEditor()
{
	m_data = new PrivateData();


	m_data->type.setIconSize(QSize(30, 20));
	m_data->type.setToolTip("Symbol type");
	m_data->type.addItem(QString(), -2);
	m_data->type.addItem("No symbol", -1);
	for (int i = VipSymbol::Ellipse; i <= VipSymbol::Hexagon; ++i)
	{
		QPixmap pix(QSize(30, 20));
		pix.fill(QColor(255, 255, 255, 0));
		QPainter p(&pix);
		p.setRenderHint(QPainter::Antialiasing, true);
		VipSymbol sym;
		sym.setPen(QColor(Qt::red));
		sym.setBrush(QColor(Qt::red));
		sym.setSize(QSizeF(11, 11));
		sym.setStyle(VipSymbol::Style(i));
		sym.drawSymbol(&p, QPointF(15, 10));
		m_data->type.addItem(QIcon(pix), "", i);
	}
	//draw arrow
	{
		QPixmap pix(QSize(30, 20));
		pix.fill(QColor(255, 255, 255, 0));
		QPainter p(&pix);
		p.setRenderHint(QPainter::Antialiasing, true);
		VipQuiverPath q;
		q.setPen(QColor(Qt::red));
		q.setExtremityPen(VipQuiverPath::End,QColor(Qt::red));
		q.setExtremityBrush(VipQuiverPath::End,QColor(Qt::red));
		q.setStyle(VipQuiverPath::EndArrow);
		q.setLength(VipQuiverPath::End, 9);
		q.draw(&p, QLineF(QPointF(3, 10), QPointF(27, 10)));
		m_data->type.addItem(QIcon(pix), "", VipSimpleAnnotation::Arrow);
	}

	m_data->size.setToolTip("Symbol size");
	m_data->size.setRange(0, 100);
	m_data->size.setSpecialValueText(" ");

	m_data->angle.setToolTip("Arrow angle in degree");
	m_data->angle.setRange(nullArrowAngle, 180);
	m_data->angle.setSpecialValueText(" ");

	QGridLayout * lay = new QGridLayout();
	lay->addWidget(new QLabel("Symbol style"), 0, 0);
	lay->addWidget(&m_data->type, 0, 1);
	lay->addWidget(new QLabel("Symbol size"), 1, 0);
	lay->addWidget(&m_data->size, 1, 1);
	lay->addWidget(new QLabel("Arrow angle"), 2, 0);
	lay->addWidget(&m_data->angle, 2, 1);
	setLayout(lay);

	connect(&m_data->type, SIGNAL(currentIndexChanged(int)), this, SLOT(emitChanged()));
	connect(&m_data->size, SIGNAL(valueChanged(int)), this, SLOT(emitChanged()));
	connect(&m_data->angle, SIGNAL(valueChanged(int)), this, SLOT(emitChanged()));
}
SymbolEditor::~SymbolEditor()
{
	delete m_data;
}

int SymbolEditor::symbolType() const
{
	return m_data->type.currentData().toInt();
}
int SymbolEditor::symbolSize() const
{
	return m_data->size.value();
}
int SymbolEditor::arrowAngle() const
{
	return m_data->angle.value();
}

void SymbolEditor::setShapes(const QList<VipPlotShape*>& shapes)
{
	int type = nullSymbolType;
	int size = nullSymbolSize;
	int angle = nullArrowAngle;
	bool first = true;

	for (int i = 0; i < shapes.size(); ++i)
	{
		if (VipAnnotation*annot = shapes[i]->annotation())
			if (strcmp(annot->name(), "VipSimpleAnnotation") == 0) {
				VipSimpleAnnotation * a = static_cast<VipSimpleAnnotation*>(annot);

				if (first) {
					type = a->endStyle();
					size = a->endSize();
					angle = a->arrowAngle();
					first = false;
				}
				else {
					if (type != a->endStyle()) type = nullSymbolType;
					if (size != a->endSize()) size = nullSymbolSize;
					if (angle != a->arrowAngle()) angle = nullArrowAngle;
				}
			}
	}

	blockSignals(true);
	m_data->type.setCurrentIndex(type - nullSymbolType);
	m_data->size.setValue(size);
	m_data->angle.setValue(angle);
	blockSignals(false);
}
void SymbolEditor::updateShapes(const QList<VipPlotShape*>& shapes)
{
	for (int i = 0; i < shapes.size(); ++i)
	{
		if (VipAnnotation*annot = shapes[i]->annotation())
			if (strcmp(annot->name(), "VipSimpleAnnotation") == 0) {
				VipSimpleAnnotation * a = static_cast<VipSimpleAnnotation*>(annot);

				if (!isNullSymbolType(symbolType())) a->setEndStyle(symbolType());
				if (!isNullSymbolSize(symbolSize())) a->setEndSize(symbolSize());
				if (!isNullArrowAngle(arrowAngle())) a->setArrowAngle(arrowAngle());

				shapes[i]->update();
			}
	}
}




class VipAnnotationWidget::PrivateData
{
public:
	TextEditor textEditor;
	ShapeEditor shapeEditor;
	SymbolEditor symbolEditor;
};
VipAnnotationWidget::VipAnnotationWidget()
{
	m_data = new PrivateData();

	QVBoxLayout * lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(0);
	lay->addWidget(&m_data->textEditor);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(&m_data->shapeEditor);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(&m_data->symbolEditor);
	lay->addStretch(1);
	setLayout(lay);

	connect(&m_data->textEditor, SIGNAL(changed()), this, SLOT(emitChanged()));
	connect(&m_data->shapeEditor, SIGNAL(changed()), this, SLOT(emitChanged()));
	connect(&m_data->symbolEditor, SIGNAL(changed()), this, SLOT(emitChanged()));

	this->setMaximumHeight(300);
}
VipAnnotationWidget::~VipAnnotationWidget()
{
	delete m_data;
}

TextEditor * VipAnnotationWidget::textEditor() const { return &m_data->textEditor; }
ShapeEditor * VipAnnotationWidget::shapeEditor() const { return &m_data->shapeEditor; }
SymbolEditor * VipAnnotationWidget::symbolEditor() const { return &m_data->symbolEditor; }

void VipAnnotationWidget::setShapes(const QList<VipPlotShape*>& shapes)
{
	m_data->textEditor.setShapes(shapes);
	m_data->shapeEditor.setShapes(shapes);
	m_data->symbolEditor.setShapes(shapes);
}
void VipAnnotationWidget::updateShapes(const QList<VipPlotShape*>& shapes)
{
	m_data->textEditor.updateShapes(shapes);
	m_data->shapeEditor.updateShapes(shapes);
	m_data->symbolEditor.updateShapes(shapes);

	//set the modifications to the underlying VipShape
	for (int i = 0; i < shapes.size(); ++i) {
		if (VipAnnotation * a = shapes[i]->annotation()) {
			shapes[i]->rawData().setAttribute("_vip_annotation", vipSaveAnnotation(a));
		}
	}
}



VipAnnotationToolWidget::VipAnnotationToolWidget(VipMainWindow * w)
	:VipToolWidgetPlayer(w)
{
	m_widget = new VipAnnotationWidget();
	setWidget(m_widget);
	setWindowTitle("Annotation editor");
	setObjectName("Annotation editor");
	setAllowedAreas(Qt::NoDockWidgetArea);
	setFloating(true);
	setMaximumHeight(300);
	connect(m_widget, SIGNAL(changed()), this, SLOT(updateShapes()));
}

bool VipAnnotationToolWidget::setPlayer(VipAbstractPlayer * pl)
{
	if (m_player) {
		disconnect(m_player->plotWidget2D()->area(), SIGNAL(childSelectionChanged(VipPlotItem *)), this, SLOT(importShapes()));
	}

	m_player = qobject_cast<VipPlayer2D*>(pl);
	if (!m_player)
		return false;

	connect(m_player->plotWidget2D()->area(), SIGNAL(childSelectionChanged(VipPlotItem *)), this, SLOT(importShapes()));
	importShapes();
	return true;
}

VipAnnotationWidget * VipAnnotationToolWidget::annotationWidget() const
{
	return m_widget;
}

static QList<VipPlotShape*> findShapes(VipPlayer2D * player, bool with_annotation)
{
	if (!player)
		return QList<VipPlotShape*>();
	//find shapes
	QList<VipPlotSceneModel*> models = player->plotSceneModels();
	QList<VipPlotShape*> shapes;
	for (int i = 0; i < models.size(); ++i) {
		QList<VipPlotShape* > tmp = models[i]->shapes(1);
		for (int j = 0; j < tmp.size(); ++j) {
			if (tmp[j]->annotation() || !with_annotation)
				shapes.append(tmp[j]);
		}
	}
	return shapes;
}

void VipAnnotationToolWidget::importShapes()
{
	m_widget->setShapes(findShapes(m_player,true));
}
void VipAnnotationToolWidget::updateShapes()
{
	m_widget->updateShapes(findShapes(m_player, true));
	m_player->update();
}

VipAnnotationToolWidget * vipGetAnnotationToolWidget(VipMainWindow * w)
{
	static VipAnnotationToolWidget * widget = new VipAnnotationToolWidget(w);
	return widget;
}

#include "VipGui.h"
static VipAnnotation * createDefaultAnnotation(VipPlayer2D * pl, VipPlotShape * sh)
{
	VipSimpleAnnotation * a = new VipSimpleAnnotation();
	a->setPen(QColor(Qt::red));
	a->setBrush(QBrush());
	a->setEndStyle(VipSimpleAnnotation::Ellipse);
	a->setEndSize(9);
	a->setText("My annotation");
	a->text().setTextPen(vipWidgetTextBrush(pl).color());
	a->text().boxStyle().setDrawLines(Vip::NoSide);
	a->setTextAlignment(Qt::AlignTop | Qt::AlignHCenter);

	if (sh->rawData().type() == VipShape::Polyline) {
		//a->text().boxStyle().setDrawLines(Vip::Bottom);
		a->setEndStyle(VipSimpleAnnotation::Arrow);
		a->setTextAlignment(Qt::AlignTop | Qt::AlignHCenter);
	}
	else if (sh->rawData().type() == VipShape::Point) {
		a->setTextDistance(a->endSize());
	}
	else{
		a->text().boxStyle().setDrawLines(Vip::NoSide);
		a->setTextPosition(Vip::XInside);
	}
	return a;
}

void vipEditAnnotations(VipPlayer2D * player)
{
	QList<VipPlotShape*> shapes = findShapes(player,false);
	for (int i = 0; i < shapes.size(); ++i) {
		if (!shapes[i]->annotation())
			shapes[i]->setAnnotation(createDefaultAnnotation(player,shapes[i]));
	}
	vipGetAnnotationToolWidget()->setPlayer(player);
	vipGetAnnotationToolWidget()->show();
}
void vipRemoveAnnotations(VipPlayer2D * player)
{
	QList<VipPlotShape*> shapes = findShapes(player,true);
	for (int i = 0; i < shapes.size(); ++i)
		shapes[i]->setAnnotation(nullptr);
}


void vipRemoveAllAnnotations(VipPlayer2D * player)
{
	QList<VipPlotSceneModel*> models = player->plotSceneModels();
	for (int i = 0; i < models.size(); ++i) {
		QList<VipPlotShape*> shapes = models[i]->shapes();
		QList<VipShape> to_remove;
		for (int j = 0; j < shapes.size(); ++j) {
			if (shapes[j]->annotation()) {
				to_remove << shapes[j]->rawData();
			}
		}
		models[i]->sceneModel().remove(to_remove);
	}
}

VipSimpleAnnotation* vipAnnotation(VipPlayer2D * player, const QString & type, const QString & text,
	const QPointF & start, const QPointF & end , const QVariantMap & attributes,
	const QString & yaxis , QString * error )
{
	if (!player)
		return nullptr;

	//find the right VipPlotSceneModel
	VipPlotSceneModel * model = player->plotSceneModel();
	if (!yaxis.isEmpty()) {
		if (VipPlotPlayer * p = qobject_cast<VipPlotPlayer*>(player)) {
			if (VipAbstractScale * scale = p->findYScale(yaxis)) {
				model = p->findPlotSceneModel(QList<VipAbstractScale*>() << p->xScale() << scale);
			}
		}
	}
	if (!model) {
		if (error) *error = "cannot find a valid area to insert annotation";
		return nullptr;
	}

	QPair<VipShape, VipSimpleAnnotation*> res = vipAnnotation(type, text, start, end, attributes, error);
	if (!res.second) {
		return nullptr;
	}

	res.first.setAttribute("_vip_annotation", vipSaveAnnotation(res.second));
	delete res.second;

	if (res.first.type() == VipShape::Point)
		res.first.setGroup("Points");
	else if (res.first.type() == VipShape::Polyline)
		res.first.setGroup("Polylines");
	else
		res.first.setGroup("ROI");

	model->sceneModel().add(res.first);
	if (VipPlotShape * sh = model->findShape(res.first))
		return static_cast<VipSimpleAnnotation*>(sh->annotation());
	return nullptr;
}
