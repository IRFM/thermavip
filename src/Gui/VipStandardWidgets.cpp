#include <limits>

#include <QApplication>
#include <QPixmap>
#include <QPainter>
#include <QStyleOption>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QBoxLayout>
#include <QGridLayout>
#include <QWidgetAction>
#include <QButtonGroup>
#include <QFontDialog>
#include <QPalette>
#include <QFileInfo>
#include <qpointer.h>


#include "VipStandardWidgets.h"
#include "VipDisplayArea.h"
#include "VipCore.h"
#include "VipGui.h"



QList<QObject*> VipFindChidren::children(const QString & name)
{
	QList<QWidget*> widgets = QApplication::topLevelWidgets();
	QList<QObject*> res;

	for (int i = 0; i < widgets.size(); ++i)
		res.append(widgets[i]->findChildren<QObject*>(name));
	return res;
}


VipDetectLooseFocus::VipDetectLooseFocus(QWidget * widget)
	:QObject(widget)
{
	qApp->installEventFilter(this);
}

VipDetectLooseFocus::~VipDetectLooseFocus()
{
	qApp->removeEventFilter(this);
}

bool	VipDetectLooseFocus::eventFilter(QObject * //watched
, QEvent *event)
{
	if (!parent())
		return false;

	if (event->type() == QEvent::MouseButtonPress)
	{
		const QWidget *area = static_cast<QWidget*>(parent());
		const QPoint pt = area->mapFromGlobal(QCursor::pos());
		if (!QRect(0, 0, area->width(), area->height()).contains(pt))
		{
			Q_EMIT focusLost();
			this->deleteLater();
		}
	}

	return false;
}



VipVerticalLine::VipVerticalLine(QWidget * parent)
	:QFrame(parent)
{
	setFrameShape(QFrame::VLine);
}

VipHorizontalLine::VipHorizontalLine(QWidget * parent)
	: QFrame(parent)
{
	setFrameShape(QFrame::HLine);
}


QFrame *VipLineWidget::createHLine(QWidget * parent)
{
	QFrame *fr = new VipHorizontalLine(parent);
	fr->setObjectName("hline");
	return fr;
}

QFrame *VipLineWidget::createVLine(QWidget * parent)
{
	QFrame *fr = new VipVerticalLine(parent);
	fr->setObjectName("vline");
	return fr;
}

QFrame *VipLineWidget::createSunkenHLine(QWidget * parent)
{
	QFrame * fr = new QFrame(parent);
	fr->setObjectName("hline");
	fr->setFrameShape(QFrame::HLine);
	fr->setFrameShadow(QFrame::Sunken);
	return fr;
}

QFrame *VipLineWidget::createSunkenVLine(QWidget * parent)
{
	QFrame * fr = new QFrame(parent);
	fr->setObjectName("vline");
	fr->setFrameShape(QFrame::VLine);
	fr->setFrameShadow(QFrame::Sunken);
	return fr;
}



QString VipStandardWidgets::format(const QString & str)
{
	static QChar c = QString("'")[0];
	if (!str.contains(c))
		return c + str + c;
	return str;
}
QWidget * VipStandardWidgets::fromName(const QString & name)
{
	return vipCreateVariant(name.toLatin1().data()).value<QWidget*>();
}

QWidget * VipStandardWidgets::fromStyleSheet(const QString & style_sheet)
{
	//find widget's name
	QStringList lst = style_sheet.split("{");
	if (lst.size() == 0)
		return NULL;

	QString class_name = lst[0].split("#")[0].remove(" ") + "*";
	//take care of '--' for widget inside namespace
	class_name.replace("--", "::");
	QWidget * widget = vipCreateVariant(class_name.toLatin1().data()).value<QWidget*>();
	if (widget)
	{
		//apply the style sheet and make sur it is applied to the widget
		widget->setStyleSheet(style_sheet);
		widget->style()->unpolish(widget);
		widget->style()->polish(widget);

		//now, remove all the qproperty stuff from the style sheet to avoid them being reset each time the widget is polished,
		//which might trigger a VipProcessingObject

		QString st = style_sheet;
		while (true)
		{
			int index = st.indexOf("qproperty-");
			if (index < 0)
				break;

			int end = st.indexOf(";", index);
			if (end < 0)
				break;
			st.remove(index, end - index);
		}
		widget->setStyleSheet(st);

	}

	return widget;
}




VipSpinBox::VipSpinBox(QWidget * parent)
	:QSpinBox(parent)
{
	setRange(-std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
	connect(this, SIGNAL(valueChanged(int)), this, SLOT(valueHasChanged()));
}


VipBoolEdit::VipBoolEdit(QWidget * parent)
	:QCheckBox(parent)
{
	connect(this, SIGNAL(stateChanged(int)), this, SLOT(changed()));
}

bool VipBoolEdit::value() const
{
	return isChecked();
}

void VipBoolEdit::setValue(bool v)
{
	setChecked(v);
}

void VipBoolEdit::changed()
{
	Q_EMIT valueChanged(isChecked());
	Q_EMIT genericValueChanged(isChecked());
}



VipDoubleSpinBox::VipDoubleSpinBox(QWidget * parent)
	:QDoubleSpinBox(parent)
{
	setRange(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
	connect(this, SIGNAL(valueChanged(double)), this, SLOT(valueHasChanged()));
}

QString VipDoubleSpinBox::textFromValue(double value) const
{
	return QDoubleSpinBox::textFromValue(value);
	//QString text = QDoubleSpinBox::textFromValue(value);
	// return text.replace(QLocale().decimalPoint(), QLatin1Char('.'));
}

double VipDoubleSpinBox::valueFromText(const QString& text) const
{
	return QDoubleSpinBox::valueFromText(text);
	// QString tmp = text;
	// tmp.replace(",", ".");
	// QTextStream str(tmp.toLatin1());
	// double v = 0;
	// if((str >> v).status() == QTextStream::Ok)
	// return v;
	// else
	// return QDoubleSpinBox::valueFromText(text);
}

QValidator::State	VipDoubleSpinBox::validate(QString &text, int &pos) const
{
	//if (text[pos] == '.')
	// text[pos] = ',';
	return QDoubleSpinBox::validate(text, pos);
}


VipPrefixSuffixLineEdit::VipPrefixSuffixLineEdit(const QString& prefix, const QString& suffix, QWidget *parent)
	:QLineEdit(parent)
{
	_prefix = new QLabel(this);
	_prefix->setStyleSheet("background: transparent;");
	_prefix->hide();
	_suffix = new QLabel(this);
	_suffix->hide();
	_suffix->setStyleSheet("background: transparent;");

	setPrefix(prefix);
	setSuffix(suffix);
}

QString VipPrefixSuffixLineEdit::prefix() const {
	return _prefix->text();
}
QString VipPrefixSuffixLineEdit::suffix() const {
	return _suffix->text();
}
QLabel * VipPrefixSuffixLineEdit::prefixLabel() const {
	return _prefix;
}
QLabel * VipPrefixSuffixLineEdit::suffixLabel() const {
	return _suffix;
}

void VipPrefixSuffixLineEdit::setPrefix(const QString& prefix)
{
	_prefix->setText(prefix);
	resetMargins();

}

void VipPrefixSuffixLineEdit::setSuffix(const QString& suffix)
{
	_suffix->setText(suffix);
	resetMargins();
}

void VipPrefixSuffixLineEdit::resetMargins()
{
	int left = 0;
	int right = 0;
	if (!prefix().isEmpty())
	{
		left = _prefix->sizeHint().width();
		if (!_prefix->isVisible())
			_prefix->show();

		_prefix->move(2, 0);
		_prefix->resize(left, height());
	}
	else if (_prefix->isVisible())
		_prefix->hide();

	if (!suffix().isEmpty())
	{
		right = _suffix->sizeHint().width();
		if (!_suffix->isVisible())
			_suffix->show();

		_suffix->move(width() - right - 2, 0);
		_suffix->resize(right, height());
	}
	else if (_suffix->isVisible())
		_suffix->hide();

	//int t, b, l, r;
	
	QMargins m = this->textMargins();
	//this->getTextMargins(&l, &t, &r, &b);
	this->setTextMargins(left, m.top(), right, m.bottom());
}

void VipPrefixSuffixLineEdit::resizeEvent(QResizeEvent *)
{
	resetMargins();
}



#include <QTextStream>
#include <QLocale>

VipDoubleEdit::VipDoubleEdit(QWidget * parent)
	:VipPrefixSuffixLineEdit(QString(), QString(), parent), m_integer(false), m_value(0)
{
	m_rightStyle = "";// "QLineEdit { border: 1px solid lightGray; }";
	m_wrongStyle = "QLineEdit { border: 1px solid red; }";
	setValue(0);

	connect(this, SIGNAL(returnPressed()), this, SLOT(enterPressed()));
	connect(this, SIGNAL(textEdited(const QString &)), this, SLOT(edited()));
}

double VipDoubleEdit::readValue(const QString & text, bool integer, bool * ok)
{
	QString str = text;

	//check for hexadecimal first
	if (str.startsWith("0x"))
	{
		str.remove("0x");
		uint v = str.toUInt(ok, 16);
		return v;
	}

	if (!integer)
	{
		QTextStream stream(&str);
		double v;
		stream >> v;
		if (ok)
			*ok = stream.status() == QTextStream::Ok && stream.pos() == str.size();
		return v;
	}
	else
	{
		QTextStream stream(&str);
		int v;
		stream >> v;
		if (ok)
			*ok = stream.status() == QTextStream::Ok && stream.pos() == str.size();
		return v;
	}
}

bool VipDoubleEdit::isValid() const
{
	bool ok = false;
	readValue(text(), m_integer, &ok);
	return ok;
}

double VipDoubleEdit::value() const
{
	return m_value;
}

const QString & VipDoubleEdit::rightStyle() const
{
	return m_rightStyle;
}

const QString & VipDoubleEdit::wrongStyle() const
{
	return m_wrongStyle;
}

const QString & VipDoubleEdit::format() const
{
	return m_format;
}

void VipDoubleEdit::setValue(double value)
{
	bool blocked = this->signalsBlocked();
	this->blockSignals(true);

	QString f = m_format;
	if (f.isEmpty())
		f = "%g";

	char val[50];
	memset(val, 0, sizeof(val));
	snprintf(val, 50, f.toLatin1().data(), value);
	setText(QString(val));
	setStyleSheet(m_rightStyle);

	this->blockSignals(blocked);

	if (value != m_value)
	{
		m_value = value;
		Q_EMIT valueChanged(value);
		Q_EMIT genericValueChanged(value);
	}
}

void VipDoubleEdit::setRightStyle(const QString & style)
{
	m_rightStyle = style;
	if (isValid())
		setStyleSheet(style);
}

void VipDoubleEdit::setWrongStyle(const QString & style)
{
	m_wrongStyle = style;
	if (!isValid())
		setStyleSheet(style);
}

void VipDoubleEdit::setFormat(const QString & format)
{
	m_format = format;
	if (isValid())
		setValue(value());
}

bool VipDoubleEdit::integerFormat() const
{
	return m_integer;
}

void VipDoubleEdit::setIntegerFormat(bool integer)
{
	m_integer = integer;
	if (isValid())
		setValue(value());
}

void VipDoubleEdit::edited()
{
	if (isValid())
	{
		setStyleSheet(m_rightStyle);
		QString str = text();

		m_value = readValue(text(), m_integer);
	}
	else
	{
		setStyleSheet(m_wrongStyle);
	}
}

void VipDoubleEdit::enterPressed()
{
	if (isValid())
	{
		bool blocked = this->signalsBlocked();
		this->blockSignals(true);

		if (!text().startsWith("0x") && !m_integer)
		{
			QString f = m_format;
			if (f.isEmpty())
				f = "%g";

			char val[50];
			memset(val, 0, sizeof(val));
			snprintf(val, 50, f.toLatin1().data(), m_value);
			setText(QString(val));
			setStyleSheet(m_rightStyle);
		}
		this->blockSignals(blocked);


		Q_EMIT valueChanged(value());
		Q_EMIT genericValueChanged(value());
	}
}



class VipMultiComponentDoubleEdit::PrivateData
{
public:
	PrivateData() :separator(","), fixedNumberOfComponents(1), maxNumberOfComponents(-1), integer(false) {}
	QString format;
	QString rightStyle;
	QString wrongStyle;
	QString separator;
	int fixedNumberOfComponents;
	int maxNumberOfComponents;
	bool integer;

	QLineEdit edit;

	VipNDDoubleCoordinate value;
};

VipMultiComponentDoubleEdit::VipMultiComponentDoubleEdit(QWidget * parent)
	:QWidget(parent)
{
	m_data = new PrivateData();
	m_data->rightStyle = "QLineEdit { border: 1px solid lightGray; }";
	m_data->wrongStyle = "QLineEdit { border: 1px solid red; }";

	QHBoxLayout * lay = new QHBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&m_data->edit);
	setLayout(lay);

	m_data->edit.setStyleSheet(m_data->rightStyle);
	connect(&m_data->edit, SIGNAL(textEdited(const QString &)), this, SLOT(edited()));
	connect(&m_data->edit, SIGNAL(returnPressed()), this, SLOT(enterPressed()));
}

VipMultiComponentDoubleEdit::~VipMultiComponentDoubleEdit()
{
	delete m_data;
}
int VipMultiComponentDoubleEdit::fixedNumberOfComponents() const
{
	return m_data->fixedNumberOfComponents;
}
int VipMultiComponentDoubleEdit::maxNumberOfComponents() const
{
	return m_data->maxNumberOfComponents;
}
QString VipMultiComponentDoubleEdit::separator() const
{
	return m_data->separator;
}
const QString & VipMultiComponentDoubleEdit::rightStyle() const
{
	return m_data->rightStyle;
}
const QString & VipMultiComponentDoubleEdit::wrongStyle() const
{
	return m_data->wrongStyle;
}
const QString & VipMultiComponentDoubleEdit::format() const
{
	return m_data->format;
}
bool VipMultiComponentDoubleEdit::isValid() const
{
	bool ok = false;
	readValue(&ok);
	return ok;
}
bool VipMultiComponentDoubleEdit::integerFormat() const
{
	return m_data->integer;
}
VipNDDoubleCoordinate VipMultiComponentDoubleEdit::value() const
{
	return m_data->value;
}
void VipMultiComponentDoubleEdit::setRightStyle(const QString & st)
{
	m_data->rightStyle = st;
	this->applyStyle();
}
void VipMultiComponentDoubleEdit::setWrongStyle(const QString & st)
{
	m_data->wrongStyle = st;
	this->applyStyle();
}
void VipMultiComponentDoubleEdit::setIntegerFormat(bool integer)
{
	m_data->integer = integer;
	this->applyStyle();
	this->applyFormat();
}
void VipMultiComponentDoubleEdit::setFixedNumberOfComponents(int comp)
{
	m_data->fixedNumberOfComponents = comp;
	m_data->maxNumberOfComponents = -1;
	this->applyStyle();
}
void VipMultiComponentDoubleEdit::setMaxNumberOfComponents(int comp)
{
	m_data->maxNumberOfComponents = comp;
	m_data->fixedNumberOfComponents = -1;
	this->applyStyle();
}
void VipMultiComponentDoubleEdit::setSeparator(const QString & sep)
{
	m_data->separator = sep;
	this->applyFormat();
	this->applyStyle();
}
void VipMultiComponentDoubleEdit::setFormat(const QString & format)
{
	m_data->format = format;
	this->applyFormat();
}
void VipMultiComponentDoubleEdit::setValue(const VipNDDoubleCoordinate & value)
{
	if (value != m_data->value)
	{
		m_data->value = value;
		Q_EMIT valueChanged(value);
		Q_EMIT genericValueChanged(QVariant::fromValue(value));
	}
	this->applyFormat();
	this->applyStyle();
}

VipNDDoubleCoordinate VipMultiComponentDoubleEdit::readValue(bool * ok) const
{
	QString str = m_data->edit.text();
	str.replace(separator(), " ");

	VipNDDoubleCoordinate value;
	QStringList lst = str.split(" ", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	for (int i = 0; i < lst.size(); ++i)
	{
		bool is_ok = false;
		VipDoubleEdit::readValue(lst[i], m_data->integer, &is_ok);
		if (!is_ok)
		{
			if (ok) *ok = false;
			return value;
		}
	}

	if (ok) *ok = true;
	if (m_data->fixedNumberOfComponents >= 0 && value.size() != m_data->fixedNumberOfComponents)
	{
		if (ok) *ok = false;
	}
	if (m_data->maxNumberOfComponents >= 0 && value.size() > m_data->maxNumberOfComponents)
	{
		if (ok) *ok = false;
	}
	return value;
}

void VipMultiComponentDoubleEdit::applyStyle()
{
	bool ok = false;
	VipNDDoubleCoordinate value = readValue(&ok);
	if (ok)
	{
		m_data->edit.setStyleSheet(m_data->rightStyle);
		m_data->value = value;
	}
	else
	{
		m_data->edit.setStyleSheet(m_data->wrongStyle);
	}
}

void VipMultiComponentDoubleEdit::applyFormat()
{
	bool ok = false;
	VipNDDoubleCoordinate value = readValue(&ok);
	if (ok && !m_data->integer)
	{
		QString res;
		for (int i = 0; i < value.size(); ++i)
		{
			QString f = m_data->format;
			if (f.isEmpty())
				f = "%g";

			char val[50];
			memset(val, 0, sizeof(val));
			snprintf(val, 50, f.toLatin1().data(), value[i]);
			res += (QString(val));
			if (i < value.size() - 1)
				res += " " + separator() + " ";
		}
		m_data->edit.blockSignals(true);
		m_data->edit.setText(res);
		m_data->edit.blockSignals(false);
	}
}

void VipMultiComponentDoubleEdit::edited()
{
	this->applyStyle();
}

void VipMultiComponentDoubleEdit::enterPressed()
{
	VipNDDoubleCoordinate value = m_data->value;
	this->applyFormat();
	Q_EMIT valueChanged(value);
	Q_EMIT genericValueChanged(QVariant::fromValue(value));
}





VipDoubleSliderEdit::VipDoubleSliderEdit(QWidget * parent)
	:QWidget(parent)
{
	m_slider = new QSlider(Qt::Horizontal);
	m_spin = new VipDoubleSpinBox();
	QHBoxLayout * lay = new QHBoxLayout();
	lay->setSpacing(2);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(m_slider, 1);
	lay->addWidget(m_spin);
	setLayout(lay);

	connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(valueHasChanged()));
	connect(m_spin, SIGNAL(valueChanged(double)), this, SLOT(valueHasChanged()));
}

double VipDoubleSliderEdit::value() const
{
	return m_spin->value();
}

double VipDoubleSliderEdit::minimum() const
{
	return m_spin->minimum();
}

double VipDoubleSliderEdit::maximum() const
{
	return m_spin->maximum();
}

double VipDoubleSliderEdit::singleStep() const
{
	return m_spin->singleStep();
}

bool VipDoubleSliderEdit::showSpinBox() const
{
	return m_spin->isVisible();
}

void VipDoubleSliderEdit::setupSlider()
{
	double range = maximum() - minimum();
	int steps = range / singleStep();
	m_slider->setMinimum(0);
	m_slider->setMaximum(steps);
	m_slider->setSingleStep(1);
}

void VipDoubleSliderEdit::setValue(double v)
{
	double previous = value();

	m_spin->blockSignals(true);
	m_slider->blockSignals(true);
	m_spin->setValue(v);
	m_slider->setValue((m_spin->value() - m_spin->minimum()) / m_spin->singleStep());
	m_spin->blockSignals(false);
	m_slider->blockSignals(false);

	if (previous != value())
		Q_EMIT valueChanged(value());
}

void VipDoubleSliderEdit::setMinimum(double v)
{
	m_spin->setMinimum(v);
	setupSlider();
}

void VipDoubleSliderEdit::setMaximum(double v)
{
	m_spin->setMaximum(v);
	setupSlider();
}

void VipDoubleSliderEdit::setSingleStep(double v)
{
	m_spin->setSingleStep(v);
	setupSlider();
}

void VipDoubleSliderEdit::setShowSpinBox(bool s)
{
	m_spin->setVisible(s);
}

void VipDoubleSliderEdit::valueHasChanged()
{
	m_spin->blockSignals(true);
	m_slider->blockSignals(true);

	if (sender() == m_slider)
	{
		m_spin->setValue(m_slider->value() * m_spin->singleStep() + m_spin->minimum());
	}
	else
	{
		m_slider->setValue((m_spin->value() - m_spin->minimum()) / m_spin->singleStep());
	}

	m_spin->blockSignals(false);
	m_slider->blockSignals(false);

	Q_EMIT valueChanged(value());
	Q_EMIT genericValueChanged(value());
}



VipLineEditIcon::VipLineEditIcon(QWidget* parent)
	: VipLineEdit(parent)
{
}

VipLineEditIcon::~VipLineEditIcon()
{
}

void VipLineEditIcon::setIcon(const QIcon & icon)
{
	m_icon = icon;
	if (m_icon.isNull())
		setTextMargins(1, 1, 1, 1);
	else
		setTextMargins(height(), 1, 1, 1);
}

void VipLineEditIcon::paintEvent(QPaintEvent* event)
{
	VipLineEdit::paintEvent(event);
	if (!m_icon.isNull()) {
		QPainter painter(this);
		QPixmap pxm = m_icon.pixmap(height() - 2, height() - 2);

		int x = 1;// , cx = pxm.width();
		painter.drawPixmap(x, 1, pxm);
		//painter.setPen(QColor("lightgrey"));
		// painter.drawLine(cx + 2, 3, cx + 2, height() - 4);
	}
}



#include <QAbstractItemView>
#include <QDropEvent>
#include <QMimeData>
#include <iostream>
VipComboBox::VipComboBox(QWidget * parent)
	: QComboBox(parent), m_innerDragDropEnabled(false)
{
	//view()->installEventFilter(this);
	connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(valueHasChanged()));
}

void VipComboBox::setCurrentText(const QString & text)
{
	int index = this->findText(text);
	if (index >= 0)
		this->setCurrentIndex(index);
}

void VipComboBox::valueHasChanged()
{
	Q_EMIT valueChanged(currentText());
	Q_EMIT genericValueChanged(currentText());
}

void VipComboBox::setInnerDragDropEnabled(bool enable)
{
	m_innerDragDropEnabled = enable;
	if (enable)
		view()->setDragDropMode(QAbstractItemView::InternalMove);
	else
		view()->setDragDropMode(QAbstractItemView::NoDragDrop);
}

bool VipComboBox::innerDragDropEnabled() const
{
	return m_innerDragDropEnabled;
}

bool VipComboBox::eventFilter(QObject *, QEvent * evt)
{
	if (evt->type() == QEvent::Drop)
	{
		if (m_innerDragDropEnabled)
		{
			const QMimeData * mime = static_cast<QDropEvent*>(evt)->mimeData();
			view()->setDragDropMode(QAbstractItemView::InternalMove);
			Q_EMIT innerItemDropped(mime->text());
			//std::cout<<mime->text().toLatin1().data()<<" "<<mime->formats().join(";").toLatin1().data()<<std::endl;
		}
	}
	return false;
}



VipEnumEdit::VipEnumEdit(QWidget * parent)
	:QComboBox(parent)
{
	connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(valueHasChanged()));
}

QString VipEnumEdit::enumNames() const
{
	QStringList lst;
	for (int i = 0; i < count(); ++i)
		lst << itemText(i);
	return lst.join(",");
}

QString VipEnumEdit::enumValues() const
{
	QStringList lst;
	for (int i = 0; i < count(); ++i)
		lst << itemData(i).toString();
	return lst.join(",");
}

QString VipEnumEdit::value() const
{
	int index = currentIndex();
	return itemData(index).toString();
}

void VipEnumEdit::setEnumNames(const QString & choices)
{
	QStringList lst = choices.split(",");
	this->clear();
	this->addItems(lst);
	for (int i = 0; i < count(); ++i)
		setItemData(i, lst[i]);
	setCurrentIndex(0);
}

void VipEnumEdit::setEnumValues(const QString & choices)
{
	QString v = value();

	QStringList lst = choices.split(",");
	int size = qMin(count(), lst.size());
	for (int i = 0; i < size; ++i)
		setItemData(i, lst[i]);

	QString v2 = value();
	if (v != v2)
	{
		Q_EMIT valueChanged(v2);
		Q_EMIT genericValueChanged(v2);
	}
}

void VipEnumEdit::setValue(const QString & value)
{
	for (int i = 0; i < count(); ++i)
		if (itemData(i).toString() == value)
		{
			this->setCurrentIndex(i);
			break;
		}
}

void VipEnumEdit::valueHasChanged()
{
	Q_EMIT valueChanged(value());
	Q_EMIT genericValueChanged(value());
}



VipColorWidget::VipColorWidget(QWidget * parent)
	:QToolButton(parent)
{
	setAutoRaise(true);
	setColor(QColor(Qt::white));

	connect(this, SIGNAL(clicked(bool)), this, SLOT(triggered()));
}

void VipColorWidget::setColor(const QColor & c)
{
	if (c != m_color)
	{
		m_color = c;
		QPixmap pix(width() - 4, height() - 4);
		pix.fill(QColor(Qt::transparent));
		QPainter p(&pix);
		p.setPen(c);
		p.setBrush(c);
		p.drawRoundedRect(QRect(0, 0, pix.width(), pix.height()), 3, 3);
		this->setIcon(QIcon(pix));

		Q_EMIT colorChanged(c);
	}
}

void VipColorWidget::triggered()
{
	QColorDialog dial(color(), NULL);
	dial.setOption(QColorDialog::ShowAlphaChannel);
	if (dial.exec() == QDialog::Accepted)
	{
		setColor(dial.currentColor());
	}
}



VipBrushWidget::VipBrushWidget(const QBrush & brush, QWidget * parent)
	:QWidget(parent)
{
	QVBoxLayout * lay = new QVBoxLayout();
	lay->addWidget(&m_color);
	lay->addWidget(&m_pattern);

	QHBoxLayout * hlay = new QHBoxLayout();
	hlay->addLayout(lay);
	hlay->addWidget(&m_image, 1);
	hlay->setContentsMargins(0, 0, 0, 0);
	setLayout(hlay);

	m_color.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_color.setText("Fill color");
	m_color.setToolTip("Change the brush color");
	m_color.setAutoRaise(true);

	m_pattern.addItems(QStringList() << "No pattern" << "Uniform color" << "Extremely dense pattern" << "Very dense pattern" <<
		"Somewhat dense pattern" << "Half dense pattern" << "Somewhat sparse pattern" << "Very sparse pattern" <<
		"Extremely sparse pattern" << "Horizontal lines" << "Vertical lines" << "Horizontal and V. lines" <<
		"Backward diagonal lines" << "Forward diagonal lines" << "Crossing diagonal lines" << "Linear gradient" << "Radial gradient" <<
		"Conical gradient" << "Texture pattern");

	m_pattern.setFrame(false);
	m_pattern.setToolTip("Select the brush pattern");

	m_image.setMargin(5);
	m_image.setMinimumHeight(80);
	m_image.setMinimumWidth(40);
	m_image.setScaledContents(true);
	m_image.setToolTip("Preview of the brush's type");

	QObject::connect(&m_color, SIGNAL(clicked(bool)), this, SLOT(setColor()));
	QObject::connect(&m_pattern, SIGNAL(activated(int)), this, SLOT(setPattern(int)));

	setBrush(brush);
}

void VipBrushWidget::setColorOptionVisible(bool vis)
{
	m_color.setVisible(vis);
	if (vis)
	{
		m_image.setMinimumHeight(80);
		this->setMinimumHeight(80);
		this->setMaximumHeight(80);
	}
	else
	{
		m_image.setMinimumHeight(30);
		this->setMinimumHeight(30);
		this->setMaximumHeight(30);
	}
}
bool VipBrushWidget::colorOptionVisible() const
{
	return !m_color.isHidden();
}

void VipBrushWidget::setBrush(const QBrush & brush)
{
	m_brush = brush;

	//set the color button
	int size = 20;
	QColor c = m_brush.color();
	QPixmap pix(size, size);
	pix.fill(QColor(Qt::transparent));
	QPainter p(&pix);
	p.setPen(c);
	p.setBrush(c);
	p.drawRoundedRect(QRect(0, 0, pix.width(), pix.height()), 3, 3);
	m_color.setIcon(QIcon(pix));

	//set the pattern
	Qt::BrushStyle style = brush.style();
	if (style != Qt::TexturePattern)
		m_pattern.setCurrentIndex(int(style));
	else
		m_pattern.setCurrentIndex(18);


	//fill image
	redraw();

	Q_EMIT brushChanged(m_brush);
}

void VipBrushWidget::redraw()
{
	//fill image
	QPixmap pix(m_image.width() - m_image.margin() * 2, m_image.height() - m_image.margin() * 2);
	pix.fill(QColor(Qt::transparent));
	QPainter p(&pix);
	p.setBrush(m_brush);
	p.setRenderHints(QPainter::Antialiasing);
	p.drawRect(QRect(0, 0, m_image.width() - m_image.margin() * 2 - 1, m_image.height() - m_image.margin() * 2 - 1));
	m_image.setPixmap(pix);
}

QBrush VipBrushWidget::brush() const
{
	return m_brush;
}

void VipBrushWidget::setPattern(int index)
{
	if (index != 18)
		m_brush.setStyle(Qt::BrushStyle(index));
	else
	{
		QString fileName = VipFileDialog::getOpenFileName(this, ("Open image pattern"), ("Image file (*.png *.jpg *.jpeg *.bmp *.pgm *.ppm *.tiff *.xbm *.xpm *.ps *.eps)"));
		if (fileName.length() > 0)
		{
			m_brush.setTextureImage(QImage(fileName));
		}
	}

	setBrush(m_brush);
}

void VipBrushWidget::setColor()
{
	QColorDialog dial(m_brush.color(), NULL);
	dial.setOption(QColorDialog::ShowAlphaChannel);
	if (dial.exec() == QDialog::Accepted)
	{
		m_brush.setColor(dial.currentColor());
		setBrush(m_brush);
	}
}





VipPenWidget::VipPenWidget(const QPen & pen, QWidget * parent)
	:QWidget(parent)
{
	QGridLayout * glay = new QGridLayout();
	int row = -1;
	glay->addWidget(new QLabel("Fill rules:"), ++row, 0);
	glay->addWidget(&m_brush, row, 1);
	glay->addWidget(new QLabel("Pen width:"), ++row, 0);
	glay->addWidget(&m_width, row, 1);
	glay->addWidget(new QLabel("Pen style:"), ++row, 0);
	glay->addWidget(&m_style, row, 1);
	glay->addWidget(new QLabel("Pen cap:"), ++row, 0);
	glay->addWidget(&m_cap, row, 1);
	glay->addWidget(new QLabel("Pen join:"), ++row, 0);
	glay->addWidget(&m_join, row, 1);
	glay->addWidget(new QLabel("Preview:"), ++row, 0);
	glay->addWidget(&m_image, row, 1);
	setLayout(glay);

	m_width.setFrame(false);
	m_width.setRange(0, 100);
	m_width.setSingleStep(0.5);
	m_width.setToolTip("Select the width of the pen (1->100)");
	m_style.setFrame(false);
	m_style.setToolTip("Select the pen style");
	m_cap.setFrame(false);
	m_cap.setToolTip("Select the line end caps");
	m_join.setFrame(false);
	m_join.setToolTip("Select the join between two connected lines");


	m_join.addItems(QStringList() << "Miter Join" << "Bevel Join" << "Round Join" << "Svg Miter Join");
	m_cap.addItems(QStringList() << "Square Cap" << "Flat Cap" << "Round Cap");
	m_style.setIconSize(QSize(150, 20));

	//m_image.setMargin(5);
	m_image.setMinimumHeight(50);
	m_image.setMinimumWidth(170);
	m_image.setToolTip("Preview of the pen's type");


	QObject::connect(&m_style, SIGNAL(activated(int)), this, SLOT(setStyle(int)));
	QObject::connect(&m_cap, SIGNAL(activated(int)), this, SLOT(setCap(int)));
	QObject::connect(&m_join, SIGNAL(activated(int)), this, SLOT(setJoin(int)));
	QObject::connect(&m_width, SIGNAL(valueChanged(double)), this, SLOT(setWidth(double)));
	QObject::connect(&m_brush, SIGNAL(brushChanged(const QBrush	 &)), this, SLOT(setBrush(const QBrush &)));

	this->setMaximumSize(QSize(300, 300));

	setPen(pen);
}

void VipPenWidget::setPen(const QPen & pen)
{
	m_pen = pen;

	m_brush.setBrush(pen.brush());
	m_width.setValue(pen.widthF());
	m_style.setCurrentIndex(int(pen.style()));

	if (pen.capStyle() == Qt::FlatCap)
		m_cap.setCurrentIndex(0);
	else if (pen.capStyle() == Qt::SquareCap)
		m_cap.setCurrentIndex(1);
	else if (pen.capStyle() == Qt::RoundCap)
		m_cap.setCurrentIndex(2);

	if (pen.joinStyle() == Qt::MiterJoin)
		m_join.setCurrentIndex(0);
	else if (pen.joinStyle() == Qt::BevelJoin)
		m_join.setCurrentIndex(1);
	else if (pen.joinStyle() == Qt::RoundJoin)
		m_join.setCurrentIndex(2);
	else if (pen.joinStyle() == Qt::SvgMiterJoin)
		m_join.setCurrentIndex(3);

	redraw();
	Q_EMIT penChanged(pen);
}

QPen VipPenWidget::pen() const
{
	return m_pen;
}

void VipPenWidget::setColorOptionVisible(bool vis)
{
	m_brush.setColorOptionVisible(vis);
}
bool VipPenWidget::colorOptionVisible() const
{
	return m_brush.colorOptionVisible();
}

void VipPenWidget::setWidth(double w)
{
	m_pen.setWidthF(w);
	setPen(m_pen);
}

void VipPenWidget::setStyle(int st)
{
	m_pen.setStyle(Qt::PenStyle(st));
	setPen(m_pen);
}

void VipPenWidget::setCap(int st)
{
	if (st == 0)
		m_pen.setCapStyle(Qt::FlatCap);
	else if (st == 1)
		m_pen.setCapStyle(Qt::SquareCap);
	else if (st == 2)
		m_pen.setCapStyle(Qt::RoundCap);

	setPen(m_pen);
}

void VipPenWidget::setJoin(int st)
{
	if (st == 0)
		m_pen.setJoinStyle(Qt::MiterJoin);
	else if (st == 1)
		m_pen.setJoinStyle(Qt::BevelJoin);
	else if (st == 2)
		m_pen.setJoinStyle(Qt::RoundJoin);
	else if (st == 3)
		m_pen.setJoinStyle(Qt::SvgMiterJoin);

	setPen(m_pen);
}

void VipPenWidget::setBrush(const QBrush & brush)
{
	m_pen.setBrush(brush);
	m_brush.blockSignals(true);
	setPen(m_pen);
	m_brush.blockSignals(false);
}

void VipPenWidget::showFullOptions(bool sh)
{
	QGridLayout * glay = static_cast<QGridLayout*>(layout());

	for (int row = 1; row < glay->rowCount(); ++row)
	{
		glay->itemAtPosition(row, 0)->widget()->setVisible(sh);
		glay->itemAtPosition(row, 1)->widget()->setVisible(sh);
	}

	if (sh)
		this->setMaximumSize(QSize(300, 300));
	else
		this->setMaximumSize(QSize(300, 100));

}

void VipPenWidget::redraw()
{
	//redraw the pen styles
	int index = m_style.currentIndex();
	m_style.clear();
	for (int i = 0; i <= Qt::CustomDashLine; ++i)
	{
		QPixmap im(150, 20);
		im.fill(QColor(230, 230, 230, 0));
		QPainter p(&im);
		QPen pen(m_brush.brush().color());
		pen.setStyle(Qt::PenStyle(i));
		pen.setWidthF(m_width.value());
		p.setPen(pen);
		p.drawLine(QPoint(0, 10), QPoint(150, 10));
		m_style.addItem(QIcon(im), "");
	}
	if (index >= 0)
		m_style.setCurrentIndex(index);

	//redraw the label
	QPixmap pix(170, 50);
	pix.fill(QColor(255, 255, 255, 0));
	QPainter p(&pix);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setPen(m_pen);
	p.drawPolyline((QPolygon() << QPoint(10, 35) << QPoint(80, 15) << QPoint(70, 35) << QPoint(160, 15)));
	m_image.setPixmap(pix);
	//m_image.setMask(pix.Mask());

}








VipPenButton::VipPenButton(const QPen & pen, QWidget * parent)
	:QToolButton(parent), m_mode(Pen)
{
	m_pen = new VipPenWidget(pen);

	//this->setToolButtonStyle (Qt::ToolButtonTextBesideIcon);
	this->setPopupMode(QToolButton::MenuButtonPopup);
	this->setAutoRaise(true);
	this->setPen(pen);
	this->setMaximumHeight(22);

	QWidgetAction * action = new QWidgetAction(&m_menu);
	action->setDefaultWidget(m_pen);
	m_menu.addAction(action);
	this->setMenu(&m_menu);

	connect(m_pen, SIGNAL(penChanged(const QPen &)), this, SLOT(setPen(const QPen&)));
	connect(this, SIGNAL(clicked(bool)), this, SLOT(triggered()));
}

VipPenButton::~VipPenButton()
{
	QList<QAction*> act = m_menu.actions();
	for (int i = 0; i< act.size(); ++i)
		delete act[i];
}

void VipPenButton::setMode(VipPenButton::Mode mode)
{
	m_mode = mode;

	if (mode == Color)
	{
		m_pen->showFullOptions(false);
		this->setPopupMode(QToolButton::DelayedPopup);
		this->setMenu(NULL);
	}
	else if (mode == Brush)
	{
		m_pen->showFullOptions(false);
		this->setPopupMode(QToolButton::MenuButtonPopup);
		this->setMenu(&m_menu);
	}
	else
	{
		m_pen->showFullOptions(true);
		this->setPopupMode(QToolButton::MenuButtonPopup);
		this->setMenu(&m_menu);
	}
}

void VipPenButton::showFullPenOptions(bool sh)
{
	m_pen->showFullOptions(sh);
}

QPen VipPenButton::pen() const
{
	return m_pen->pen();
}

VipPenButton::Mode VipPenButton::mode() const
{
	return m_mode;
}

void VipPenButton::setColorOptionVisible(bool vis)
{
	if (vis != colorOptionVisible())
	{
		m_pen->setColorOptionVisible(vis);

		if (vis) {
			if (m_mode == Color) this->setPopupMode(QToolButton::DelayedPopup);
			else  this->setPopupMode(QToolButton::MenuButtonPopup);
			connect(this, SIGNAL(clicked(bool)), this, SLOT(triggered()));
		}
		else {
			this->setPopupMode(QToolButton::InstantPopup);
			disconnect(this, SIGNAL(clicked(bool)), this, SLOT(triggered()));
		}
	}
}
bool VipPenButton::colorOptionVisible() const
{
	return m_pen->colorOptionVisible();
}

void VipPenButton::setPen(const QPen & pen)
{
	m_pen->blockSignals(true);
	m_pen->setPen(pen);
	m_pen->blockSignals(false);
	redraw();
	Q_EMIT penChanged(pen);
}

void VipPenButton::setBrush(const QBrush & brush)
{
	m_pen->blockSignals(true);
	m_pen->setBrush(brush);
	m_pen->blockSignals(false);
	redraw();
	Q_EMIT penChanged(pen());
}

void VipPenButton::triggered()
{
	QColorDialog dial(pen().brush().color(), NULL);
	dial.setOption(QColorDialog::ShowAlphaChannel);
	if (dial.exec() == QDialog::Accepted)
	{
		QPen pen(this->pen());
		pen.setColor(dial.currentColor());
		setPen(pen);
	}
}

void VipPenButton::redraw()
{
	//set the color button
	int w, h;
	if (text().isEmpty())
	{
		w = width() - 3;
		if (popupMode() == QToolButton::MenuButtonPopup)
			w -= 17;
		else if (popupMode() == QToolButton::InstantPopup)
			w -= 10;
		h = height() - 3;
	}
	else
	{
		w = qMax(height() - 4, 3);
		h = w;
	}

	QColor c = pen().brush().color();
	QPixmap pix(w, h);
	pix.fill(QColor(Qt::transparent));
	QPainter p(&pix);
	//if(m_mode == Color)
	{
		p.setPen(c);
		p.setBrush(c);
		p.setRenderHints(QPainter::Antialiasing);
		p.drawRoundedRect(QRect(0, 0, pix.width(), pix.height()), 3, 3);
	}
	//else if(m_mode == Brush)
	// {
	// p.setPen(pen().brush().color());
	// p.setBrush(pen().brush());
	// p.setRenderHints(QPainter::Antialiasing);
	// p.drawRoundedRect(QRect(0,0,pix.width(),pix.height()),3,3);
	// }
	// else if(m_mode == Pen)
	// {
	// double width = pen().widthF()/2;
	// p.setPen(pen());
	// p.setBrush(QBrush());
	// p.setRenderHints(QPainter::Antialiasing);
	// p.drawRoundedRect(QRect(width,width,pix.width()-width,pix.height()-width),3,3);
	// }
	setIcon(QIcon(pix));
}

void VipPenButton::resizeEvent(QResizeEvent* evt)
{
	QToolButton::resizeEvent(evt);
	redraw();
}





VipTextWidget::VipTextWidget(QWidget * parent)
	:QWidget(parent)
{
	//QGridLayout * grid = new QGridLayout();
//
	// grid->addWidget(&textChoice, 0, 0);
	// grid->addWidget(&textColor, 1, 0);
	// textColor.setMaximumHeight(10);
	// textChoice.setMaximumHeight(20);
//
	// grid->addWidget(&backgroundPen, 0, 1, 2, 1);
	// backgroundPen.setMaximumWidth(30);
	// grid->addWidget(&backgroundBrush, 0, 2, 2, 1);
	// backgroundBrush.setMaximumWidth(30);
	// grid->setContentsMargins(0, 0, 0, 0);
	// grid->setSpacing(0);
	QHBoxLayout * tlay = new QHBoxLayout();
	tlay->setContentsMargins(0, 0, 0, 0);
	tlay->setSpacing(0);
	tlay->addWidget(&textChoice);
	tlay->addWidget(&textColor);
	textColor.setMaximumWidth(15);
	textChoice.setMaximumHeight(20);

	//backgroundPen.setText("Border");
	backgroundPen.setToolTip("Text's border pen");
	//backgroundBrush.setText("Background");
	backgroundBrush.setToolTip("Text's background brush");
	textColor.setToolTip("Text color");
	textColor.setMode(VipPenButton::Color);
	backgroundBrush.setMode(VipPenButton::Brush);
	textChoice.setIcon(vipIcon("font.png"));
	textChoice.setToolTip("Select text font");
	textChoice.setAutoRaise(true);
	text.setToolTip("Enter your text");

	m_options = new QWidget();
	m_options->setLayout(tlay);
	m_options->setMaximumWidth(80);

	QHBoxLayout * hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay->addWidget(&text, 2);
	hlay->addWidget(m_options);
	hlay->addStretch(1);
	setLayout(hlay);

	connect(&textChoice, SIGNAL(clicked(bool)), this, SLOT(fontChoice()));
	connect(&text, SIGNAL(returnPressed()), this, SLOT(textEdited()));
	connect(&textColor, SIGNAL(penChanged(const QPen&)), this, SLOT(textEdited()));
	connect(&backgroundPen, SIGNAL(penChanged(const QPen&)), this, SLOT(textEdited()));
	connect(&backgroundBrush, SIGNAL(penChanged(const QPen&)), this, SLOT(textEdited()));

	this->setMaximumHeight(30);
}

void VipTextWidget::textEdited()
{
	Q_EMIT changed(getText());
}

QLineEdit * VipTextWidget::edit() const
{
	return const_cast<QLineEdit*>(&text);
}

void VipTextWidget::setText(const VipText & t)
{
	text.setText(t.text());
	text.setFont(t.font());
	backgroundPen.setPen(t.borderPen());
	backgroundBrush.setBrush(t.backgroundBrush());
	textColor.setPen(t.textPen());

	QPalette palette;
	palette.setColor(QPalette::Text, t.textPen().color());
	text.setPalette(palette);

	Q_EMIT changed(getText());
}

VipText VipTextWidget::getText() const
{
	VipText t;
	t.setBorderPen(backgroundPen.pen());
	t.setBackgroundBrush(backgroundBrush.pen().brush());
	t.setTextPen(textColor.pen());
	t.setFont(text.font());
	t.setText(text.text());
	return t;
}

void VipTextWidget::fontChoice()
{
	QFontDialog dial(text.font());
	if (dial.exec() == QDialog::Accepted)
	{
		QFont textfont = dial.currentFont();
		text.setFont(textfont);
		textEdited();
	}
}





VipBoxStyleWidget::VipBoxStyleWidget(QWidget * parent)
	:QWidget(parent)
{
	QGridLayout * glay = new QGridLayout();

	int row = -1;

	glay->setContentsMargins(0, 0, 0, 0);
	glay->addWidget(&m_background, ++row, 0);
	glay->addWidget(&m_border, ++row, 0);
	glay->addWidget(new QLabel("Border radius"), ++row, 0);
	glay->addWidget(&m_radius, row, 1);

	m_background.setText("Background brush");
	m_background.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_background.setMode(VipPenButton::Brush);

	m_border.setText("Border pen");
	m_border.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_border.setMode(VipPenButton::Pen);

	setLayout(glay);

	connect(&m_background, SIGNAL(penChanged(const QPen&)), this, SLOT(emitBoxStyleChanged()));
	connect(&m_border, SIGNAL(penChanged(const QPen&)), this, SLOT(emitBoxStyleChanged()));
	connect(&m_radius, SIGNAL(valueChanged(int)), this, SLOT(emitBoxStyleChanged()));
}

void VipBoxStyleWidget::setBoxStyle(const VipBoxStyle & box)
{
	m_style = box;

	m_background.blockSignals(true);
	m_border.blockSignals(true);
	m_radius.blockSignals(true);

	m_background.setPen(QPen(box.backgroundBrush(), 1));
	m_border.setPen(box.borderPen());
	m_radius.setValue(box.borderRadius());

	m_background.blockSignals(false);
	m_border.blockSignals(false);
	m_radius.blockSignals(false);

	Q_EMIT boxStyleChanged(m_style);
}

VipBoxStyle VipBoxStyleWidget::getBoxStyle() const
{
	return m_style;
}

void VipBoxStyleWidget::emitBoxStyleChanged()
{
	m_style.setBackgroundBrush(m_background.pen().brush());
	m_style.setBorderPen(m_border.pen());
	m_style.setBorderRadius(m_radius.value());
	if (m_radius.value())
		m_style.setRoundedCorners(Vip::AllCorners);
	else
		m_style.setRoundedCorners(Vip::Corners());

	Q_EMIT boxStyleChanged(m_style);
}









static QMap<QString, QString>  m_last_dirs;//= vipGetDataDirectory();
static QMap<QString, QString> m_last_filters;
static QString m_default_dir;

void VipFileDialog::setDefaultDirectory(const QString & dirname)
{
	m_default_dir = dirname;
}

QString VipFileDialog::getExistingDirectory(QWidget * parent, const QString & caption, QFileDialog::Options options)
{
	QString dir = m_default_dir.isEmpty() ? m_last_dirs[caption] : m_default_dir;
	QString res = QFileDialog::getExistingDirectory(parent, caption, dir, options);
	m_default_dir = QString();
	if (!res.isEmpty())
	{
		QFileInfo info(res);
		m_last_dirs[caption] = info.absolutePath();
	}
	return res;
}

QString VipFileDialog::getOpenFileName(QWidget * parent, const QString & caption, const QString & filter, QString * selectedFilter, QFileDialog::Options options)
{
	QString & last_filter = m_last_filters[caption];
	QString filters = filter;
	if (!last_filter.isEmpty() && last_filter != filters)
		filters = last_filter + ";;" + filters;

	QString dir = m_default_dir.isEmpty() ? m_last_dirs[caption] : m_default_dir;
	QString res = QFileDialog::getOpenFileName(parent, caption, dir, filter, &last_filter, options);
	m_default_dir = QString();
	if (!res.isEmpty())
	{
		QFileInfo info(res);
		m_last_dirs[caption] = info.absolutePath();
		if (selectedFilter)
			*selectedFilter = last_filter;
	}
	return res;
}


QStringList VipFileDialog::getOpenFileNames(QWidget * parent, const QString & caption, const QString & filter, QString * selectedFilter, QFileDialog::Options options)
{
	QString & last_filter = m_last_filters[caption];
	QString filters = filter;
	if (!last_filter.isEmpty() && last_filter != filters)
		filters = last_filter + ";;" + filters;

	QString dir = m_default_dir.isEmpty() ? m_last_dirs[caption] : m_default_dir;
	QStringList res = QFileDialog::getOpenFileNames(parent, caption, dir, filter, &last_filter, options);
	m_default_dir = QString();
	if (res.size() != 0)
	{
		QFileInfo info(res[0]);
		m_last_dirs[caption] = info.absolutePath();
		if (selectedFilter)
			*selectedFilter = last_filter;
	}
	return res;
}

#include <iostream>
QString VipFileDialog::getSaveFileName(QWidget * parent, const QString & caption, const QString & filter, QString * selectedFilter, QFileDialog::Options options)
{
	QString & last_filter = m_last_filters[caption];
	QString filters = filter;
	if (!last_filter.isEmpty() && last_filter != filters)
		filters = last_filter + ";;" + filters;

	QString dir = m_default_dir.isEmpty() ? m_last_dirs[caption] : m_default_dir;
	QString res = QFileDialog::getSaveFileName(parent, caption, dir, filter, &last_filter, options);
	m_default_dir = QString();
	if (!res.isEmpty())
	{
		QFileInfo info(res);
		m_last_dirs[caption] = info.absolutePath();
		if (selectedFilter)
			*selectedFilter = last_filter;
	}

	//make sure the result has a valid suffix
	QString suffix = QFileInfo(res).suffix();
	if (suffix.isEmpty() && !last_filter.isEmpty())
	{
		int index1 = last_filter.indexOf("*.");
		int index2 = last_filter.indexOf(")", index1);
		if (index1 > 0 && index2 > 0)
		{
			QString s = last_filter.mid(index1 + 2, index2 - index1 - 2);
			s.remove(" ");
			if (s.size())
				res += "." + s;
		}
	}

	return res;
}
QString VipFileDialog::getSaveFileName2(QWidget * parent, const QString &filename, const QString & caption, const QString & filter, QString * selectedFilter, QFileDialog::Options options)
{
	QString & last_filter = m_last_filters[caption];
	QString filters = filter;
	if (!last_filter.isEmpty() && last_filter != filters)
		filters = last_filter + ";;" + filters;

	QString dir = QFileInfo(filename).canonicalPath();
	if (dir.isEmpty() || dir == ".")
		dir = m_default_dir.isEmpty() ? m_last_dirs[caption] : m_default_dir;
	QString fname = QFileInfo(filename).fileName();
	fname.replace("/", "_");
	fname.replace("\\", "_");
	fname.replace("*", "_");
	fname.replace(":", "_");
	fname.replace("<", "_");
	fname.replace(">", "_");
	fname.replace("|", "_");
	if (!fname.isEmpty())
		dir += "/" + fname;
	//printf("fname: %s\n",fname.toLatin1().data());
	QString res = QFileDialog::getSaveFileName(parent, caption, dir, filter, &last_filter, options);
	m_default_dir = QString();
	if (!res.isEmpty())
	{
		QFileInfo info(res);
		m_last_dirs[caption] = info.absolutePath();
		if (selectedFilter)
			*selectedFilter = last_filter;
	}

	//make sure the result has a valid suffix
	QString suffix = QFileInfo(res).suffix();
	if (suffix.isEmpty() && !last_filter.isEmpty())
	{
		int index1 = last_filter.indexOf("*.");
		int index2 = last_filter.indexOf(")", index1);
		if (index1 > 0 && index2 > 0)
		{
			QString s = last_filter.mid(index1 + 2, index2 - index1 - 2);
			s.remove(" ");
			if (s.size())
				res += "." + s;
		}
	}

	return res;
}



VipFileName::VipFileName(QWidget * parent)
	:QWidget(parent), m_mode(Open)
{
	m_layout = new QGridLayout();
	m_layout->setContentsMargins(0, 0, 0, 0);
	m_layout->setSpacing(0);
	m_layout->addWidget(&m_edit, 0, 0);
	m_layout->addWidget(&m_button, 0, 1);
	m_edit.setToolTip("File full path");
	m_button.setAutoRaise(true);
	m_button.setText("...");
	m_button.setToolTip("Select file...");
	setLayout(m_layout);

	connect(&m_button, SIGNAL(clicked(bool)), this, SLOT(buttonTriggered()));
}

VipFileName::VipFileName(const QString & filename, QWidget * parent)
	:QWidget(parent), m_mode(Open)
{
	m_layout = new QGridLayout();
	m_layout->setContentsMargins(0, 0, 0, 0);
	m_layout->setSpacing(0);
	m_layout->addWidget(&m_edit, 0, 0);
	m_layout->addWidget(&m_button, 0, 1);
	m_edit.setToolTip("File full path");
	m_button.setAutoRaise(true);
	m_button.setText("...");
	m_button.setToolTip("Select file...");
	setLayout(m_layout);

	setFilename(filename);

	connect(&m_button, SIGNAL(clicked(bool)), this, SLOT(buttonTriggered()));
}

VipFileName::Mode VipFileName::mode() const
{
	return m_mode;
}

QString VipFileName::filename() const
{
	return m_prefix + m_edit.text();
}

QString VipFileName::filters() const
{
	return m_filters;
}

QString VipFileName::prefix() const
{
	return m_prefix;
}

QString VipFileName::title() const
{
	return m_title;
}

QString VipFileName::defaultPath() const
{
	return m_default_path;
}

QGridLayout * VipFileName::gridLayout()
{
	return m_layout;
}

void VipFileName::setFilename(const QString & filename)
{
	if (filename != m_edit.text())
	{
		m_edit.setText(filename);
		emit changed(filename);
	}
}

void VipFileName::setFilters(const QString & filters)
{
	m_filters = filters;
}

void VipFileName::setPrefix(const QString & prefix)
{
	m_prefix = prefix;
}

void VipFileName::setTitle(const QString & title)
{
	m_title = title;
}

void VipFileName::setDefaultPath(const QString & path)
{
	m_default_path = path;
}

void VipFileName::setDefaultOpenDir(const QString & dir)
{
	m_default_open_dir = dir;
}

QString VipFileName::defaultOpenDir() const
{
	return m_default_open_dir;
}

void VipFileName::setMode(Mode mode)
{
	m_mode = mode;
}

void VipFileName::buttonTriggered()
{
	QString fileName;
	QString filters;
	if (m_filters.isEmpty()) filters = "All files (*)";
	else filters = m_filters;

	VipFileDialog::setDefaultDirectory(m_default_open_dir);
	if (m_mode == Save)
		fileName = VipFileDialog::getSaveFileName(this, m_title, filters);
	else if (m_mode == Open)
		fileName = VipFileDialog::getOpenFileName(this, m_title, filters);
	else if (m_mode == OpenDir)
		fileName = VipFileDialog::getExistingDirectory(this, m_title);
	VipFileDialog::setDefaultDirectory(QString());

	if (!fileName.isEmpty())
	{
		setFilename(fileName);
	}
}











#include <qwidgetaction.h>

struct Action {
	QAction * action;
	QWidget * widget;
	QWidget * parent;
	Action(QAction * act = NULL, QWidget * w = NULL, QWidget *p = NULL)
		:action(act), widget(w), parent(p) {}
};

static QList<Action> findActions(QWidget * bar, QAction * exclude = NULL) {
	QList<QAction*> acts = bar->actions();
	QList<Action> res;
	for (int i = 0; i < acts.size(); ++i) {
		if (acts[i] == exclude)
			continue;
		if (QWidgetAction * a = qobject_cast<QWidgetAction*>(acts[i])) {
			if (QWidget * w = a->defaultWidget()) {
				//special case: tool bar
				//if (QToolBar* b = qobject_cast<QToolBar*>(w))
				// res += findActions(b);
				// else
				res.append(Action(a, w, a->associatedWidgets()[0]));
			}

		}
		else {
			QList<QWidget*> ws = acts[i]->associatedWidgets();
			if (ws.size() > 1)
				res.append(Action(acts[i], ws[1], ws[0]));
			else
				res.append(Action(acts[i], NULL, ws[0]));
		}
	}
	return res;
}

static QList<Action> hiddenActions(QWidget * parent, const QList<Action> & acts, int * visible_actions_width = NULL)
{
	int width = 0;
	QList<Action> res;
	int i = 0;
	for (; i < acts.size(); ++i)
	{
		//Action act = acts[i];
		if (acts[i].action->isVisible()) {
			//set action size as long as it is inside the tool bar (and we can compute it)
			if (acts[i].widget) {
				if (acts[i].widget->isVisible()) {
					width = acts[i].widget->mapTo(parent, QPoint(acts[i].widget->width(), 0)).x();
				}
				else {
					//stop at first hidden widget
					break;
				}
			}
			else
				width += 20; //probably a separator
		}
	}

	if (visible_actions_width)
		*visible_actions_width = width;

	return acts.mid(i);
}

static void transferActions(QWidget * from, QWidget * to, const QList<Action> & acts)
{
	for (int i = 0; i < acts.size(); ++i)
	{
		Action a = acts[i];
		from->removeAction(a.action);
		//acts[i].parent->
to->addAction(a.action);
	}
}



VipAdditionalToolBar::VipAdditionalToolBar(VipToolBar * parent)
	:QToolBar(parent), parent(parent)
{}

bool VipAdditionalToolBar::eventFilter(QObject * watched, QEvent *event)
{
	//filter all app events, and make sure to hide on first click or other event outside the tool bar
	if (event->type() == QEvent::MouseButtonPress)
	{
		if (QWidget * w = qobject_cast<QWidget*>(watched)) {
			if (w != this) {
				//check that the widget belongs to the tool bar
				while (w) {
					if (w == this) return false;
					w = w->parentWidget();
				}
			}
			else {
				//when clicking outside, the event is still sent to the tool bar.
				//Therefore, check that the cursor is still inside the tool bar.
				QRect r(this->mapToGlobal(QPoint(0, 0)), this->mapToGlobal(QPoint(width(), height())));
				if (!r.contains(QCursor::pos()))
					hide();
			}
			//the event was on a widget outisde: hide the tool bar
			hide();
		}
	}
	return false;
}

void VipAdditionalToolBar::showEvent(QShowEvent *)
{
	//filter all app events
	qApp->installEventFilter(this);
}
void VipAdditionalToolBar::hideEvent(QHideEvent *)
{
	qApp->removeEventFilter(this);
	parent->aboutToHide();
}



class VipToolBar::PrivateData
{
public:
	QMenu *additional;
	VipAdditionalToolBar * additionalToolBar;
	QToolButton showAdditional;
	QAction* showAdditionalAction;

	VipToolBar::ShowAdditionals _ShowAdditionals;

	QList<Action> acts, hidden;
	int width;
	bool aboutToShow;
	bool dirtyCompute;
	bool customBehaviorEnabled;

	QTimer timer;

	PrivateData()
		: showAdditionalAction(NULL), _ShowAdditionals(ShowInMenu), aboutToShow(false), dirtyCompute(true), customBehaviorEnabled(true)
	{}
};

VipToolBar::VipToolBar(QWidget * parent)
	:QToolBar(parent)
{
	m_data = new PrivateData();
	m_data->additionalToolBar = new VipAdditionalToolBar(this);
	m_data->additionalToolBar->setWindowFlags(this->windowFlags() | Qt::Popup);
	m_data->additionalToolBar->hide();
	m_data->additionalToolBar->setPalette(palette());

	m_data->additional = new QMenu(&m_data->showAdditional);
	m_data->additional->setToolTipsVisible(true);

	m_data->showAdditional.setMenu(m_data->additional);
	m_data->showAdditional.setPopupMode(QToolButton::InstantPopup);
	m_data->showAdditional.setParent(this);
	m_data->showAdditional.setMaximumWidth(13);
	m_data->showAdditional.resize(13, height());
	m_data->showAdditional.hide();
	m_data->showAdditional.setAutoFillBackground(true);
	m_data->showAdditional.setPalette(palette());
	m_data->showAdditional.setToolTip("Show additional actions");
	//to hide the tool button menu indicator:
	//QToolButton::menu-indicator{ image: none; }

	m_data->timer.setSingleShot(true);
	m_data->timer.setInterval(200);

	connect(m_data->additional, &QMenu::aboutToShow, this, &VipToolBar::aboutToShow);
	connect(m_data->additional, &QMenu::aboutToHide, this, &VipToolBar::aboutToHide);
	connect(&m_data->timer, SIGNAL(timeout()), this, SLOT(compute()));
}

VipToolBar::~VipToolBar()
{
	delete m_data->additionalToolBar;
	delete m_data->additional;
	delete m_data;
}

QSize VipToolBar::sizeHint() const
{
	QSize s = QToolBar::sizeHint();
	s.rwidth() += m_data->additional->sizeHint().width() + 13; //add 13 for the showAdditional button
	s.rwidth() += m_data->additionalToolBar->sizeHint().width();
	return s;
}

void VipToolBar::aboutToShow()
{
	//body.remove( QRegExp( "<(?:div|span|tr|td|br|body|html|tt|a|strong|p)[^>]*>", Qt::CaseInsensitive ) );

	m_data->aboutToShow = true;

	if (m_data->_ShowAdditionals == ShowInMenu) {
		//add hidden actions in menu
		for (int i = 0; i < m_data->hidden.size(); ++i)
		{
			//m_data->hidden[i].parent
this->removeAction(m_data->hidden[i].action);
			m_data->additional->addAction(m_data->hidden[i].action);
		}
	}
	else {
		//copy parameters
		m_data->additionalToolBar->setIconSize(iconSize());
		m_data->additionalToolBar->setToolButtonStyle(toolButtonStyle());

		//add hidden actions in tool bar
		for (int i = 0; i < m_data->hidden.size(); ++i)
		{
			//m_data->hidden[i].parent
this->removeAction(m_data->hidden[i].action);
			m_data->additionalToolBar->addAction(m_data->hidden[i].action);
		}

		//resize to avoid showing the bottom-arrow span button
		//m_data->additionalToolBar->resize(m_data->additionalToolBar->size() + QSize(10, 0));

		int w = m_data->additionalToolBar->sizeHint().width();
		if (w > this->width()) {
			//align to the left
			m_data->additionalToolBar->move(this->mapToGlobal(QPoint(0, height())));
		}
		else {
			//align to the right
			m_data->additionalToolBar->move(this->mapToGlobal(QPoint(width() - w, height())));
		}
		//show in event loop
		QMetaObject::invokeMethod(m_data->additionalToolBar, "show", Qt::QueuedConnection);
	}

	m_data->aboutToShow = false;
}

void VipToolBar::aboutToHide()
{

	if (m_data->_ShowAdditionals == ShowInMenu) {
		//transfer back
		transferActions(m_data->additional, this, m_data->hidden);
	}
	else if (sender() != m_data->additional) {
		//transfer back
		transferActions(m_data->additionalToolBar, this, m_data->hidden);
	}
}

QToolButton *VipToolBar::showButton() const
{
	return &m_data->showAdditional;
}

QMenu * VipToolBar::showAdditionalMenu() const
{
	return m_data->additional;
}
QToolBar * VipToolBar::showAdditionalToolBar() const
{
	return m_data->additionalToolBar;
}

VipToolBar::ShowAdditionals VipToolBar::showAdditionals() const
{
	return m_data->_ShowAdditionals;
}

void VipToolBar::setShowAdditionals(ShowAdditionals sh)
{
	sizeHint();
	if (sh != m_data->_ShowAdditionals) {
		m_data->_ShowAdditionals = sh;
		if (sh == ShowInMenu) {
			m_data->additional->setStyleSheet("");
		}
		else {
			//transferActions(m_data->additional, m_data->additionalToolBar);
			m_data->additional->setStyleSheet("background: transparent; border: 0px;");
		}
	}
}

void VipToolBar::delayCompute()
{
	if (!m_data->customBehaviorEnabled)
		return;
	if (m_data->dirtyCompute)
	{
		m_data->dirtyCompute = false;
		QMetaObject::invokeMethod(this, "compute", Qt::QueuedConnection);
	}
}

void VipToolBar::setCustomBehaviorEnabled(bool enable)
{
	if (enable != m_data->customBehaviorEnabled)
	{
		m_data->customBehaviorEnabled = enable;
		if (enable) {
			compute();
		}
		else {
			m_data->showAdditional.hide();
		}
	}
}
bool VipToolBar::customBehaviorEnabled() const
{
	return m_data->customBehaviorEnabled;
}

void VipToolBar::compute()
{
	if (!m_data->customBehaviorEnabled)
		return;

	m_data->width = 0;
	m_data->acts = findActions(this) + findActions(m_data->additionalToolBar) + findActions(m_data->additional);
	m_data->hidden = hiddenActions(this, m_data->acts, &m_data->width);
	if (m_data->hidden.size()) {
		m_data->showAdditional.show();
		m_data->showAdditional.move(this->width() - m_data->showAdditional.width(), 0);
		m_data->showAdditional.resize(m_data->showAdditional.width(), this->height());
		//m_data->showAdditional.raise();
	}
	else
		m_data->showAdditional.hide();

	m_data->dirtyCompute = true;
}

void VipToolBar::showEvent(QShowEvent * evt)
{
	QToolBar::showEvent(evt);
	compute();
}

void VipToolBar::resizeEvent(QResizeEvent * evt)
{
	QToolBar::resizeEvent(evt);
	compute();
}

void VipToolBar::actionEvent(QActionEvent *evt)
{
	QToolBar::actionEvent(evt);
	//We need to trigger a compute() each time the user add/remove an action to/from the toolbar.
	//However we DON'T want to trigger while it is creating the additional menu/toolbar in aboutToShow().

	if (!m_data->aboutToShow) {
		delayCompute();
	}
}










VipValueToTimeButton::VipValueToTimeButton(QWidget * parent)
	:QToolButton(parent), m_auto_unit(true)
{
	QMenu * menu = new QMenu();
	QAction * ns = menu->addAction("Nanoseconds");
	ns->setProperty("unit", QString("ns"));
	ns->setCheckable(true);
	QAction * nsse = menu->addAction("Nanoseconds since Epoch");
	nsse->setProperty("unit", QString("ns"));
	nsse->setCheckable(true);
	QAction * micro = menu->addAction("Microseconds");
	micro->setProperty("unit", QString("us"));
	micro->setCheckable(true);
	QAction * microse = menu->addAction("Microseconds since Epoch");
	microse->setProperty("unit", QString("us"));
	microse->setCheckable(true);
	QAction * milli = menu->addAction("Milliseconds");
	milli->setProperty("unit", QString("ms"));
	milli->setCheckable(true);
	QAction * millise = menu->addAction("Milliseconds since Epoch");
	millise->setProperty("unit", QString("ms"));
	millise->setCheckable(true);
	QAction * se = menu->addAction("Seconds");
	se->setProperty("unit", QString("s"));
	se->setCheckable(true);
	QAction * sese = menu->addAction("Seconds since Epoch");
	sese->setProperty("unit", QString("s"));
	sese->setCheckable(true);
	menu->addSeparator();
	QAction * auto_unit = menu->addAction("Automatic unit");
	auto_unit->setProperty("unit", QString("auto"));

	menu->addSeparator();
	QAction * integer = menu->addAction("Display time as integer");
	integer->setProperty("unit", QString("integer"));
	integer->setCheckable(true);
	QAction * datatime = menu->addAction("Display absolute date time");
	datatime->setProperty("unit", QString("datetime"));
	datatime->setCheckable(true);

	menu->addSeparator();
	QAction * offset = menu->addAction("Display time offset from left date");
	offset->setProperty("unit", QString("offset"));
	offset->setCheckable(true);
	offset->setChecked(VipGuiDisplayParamaters::instance()->displayTimeOffset());
	//this option has no effect for now, hide it
	offset->setVisible(false);

	m_time.fixedStartValue = !VipGuiDisplayParamaters::instance()->displayTimeOffset();

	this->setMenu(menu);
	this->setPopupMode(QToolButton::InstantPopup);
	this->setText("ns  ");
	this->setMaximumWidth(40);
	this->setToolTip("Time unit");
	connect(this, SIGNAL(triggered(QAction*)), this, SLOT(timeUnitTriggered(QAction*)));
}

QAction * VipValueToTimeButton::findAction(const QString & unit_name) const
{
	QList<QAction*> acts = menu()->actions();
	for (int i = 0; i < acts.size(); ++i)
		if (acts[i]->property("unit").toString() == unit_name)
			return acts[i];
	return NULL;
}

const VipValueToTime & VipValueToTimeButton::currentValueToTime() const
{
	return m_time;
}

void VipValueToTimeButton::setValueToTime(VipValueToTime::TimeType type)
{
	QAction * action = this->menu()->actions()[type];
	action->setChecked(true);
	timeUnitTriggered(action);
}

void VipValueToTimeButton::setDisplayType(VipValueToTime::DisplayType type)
{
	QAction * integer = findAction("integer");
	QAction * datetime = findAction("datetime");
	if (type == VipValueToTime::Double) {
		integer->setChecked(false);
		datetime->setChecked(false);
		timeUnitTriggered(NULL);
	}
	else if (type == VipValueToTime::Integer) {
		integer->setChecked(true);
		datetime->setChecked(false);
		timeUnitTriggered(integer);
	}
	else if (type == VipValueToTime::AbsoluteDateTime) {
		integer->setChecked(false);
		datetime->setChecked(true);
		timeUnitTriggered(datetime);
	}
}

VipValueToTime::DisplayType VipValueToTimeButton::displayType() const
{
	return m_time.displayType;
}

void VipValueToTimeButton::setDisplayTimeOffset(bool enable)
{
	QAction * offset = findAction("offset");
	offset->setChecked(enable);
	timeUnitTriggered(offset);
}
bool VipValueToTimeButton::displayTimeOffset() const
{
	return !m_time.fixedStartValue;
}

bool VipValueToTimeButton::automaticUnit() const
{
	return m_auto_unit;
}

void VipValueToTimeButton::setAutomaticUnit(bool auto_unit)
{
	m_auto_unit = auto_unit;
}

void VipValueToTimeButton::timeUnitTriggered(QAction* action)
{
	if (!action || action->property("unit").toString() == "integer" || action->property("unit").toString() == "datetime")
	{
		//trigger display integer or date time
		QAction * integer = findAction("integer");
		QAction * datetime = findAction("datetime");

		if (action && action == datetime && datetime->isChecked()) {
			//uncheck integer
			integer->blockSignals(true);
			integer->setChecked(false);
			integer->blockSignals(false);
		}
		else if (action && action == integer && integer->isChecked()) {
			//uncheck datetime
			datetime->blockSignals(true);
			datetime->setChecked(false);
			datetime->blockSignals(false);
		}
		else if (!action) {
			//uncheck all
			datetime->blockSignals(true);
			datetime->setChecked(false);
			datetime->blockSignals(false);
			integer->blockSignals(true);
			integer->setChecked(false);
			integer->blockSignals(false);
		}

		if (!integer->isChecked() && !datetime->isChecked())
			m_time.displayType = VipValueToTime::Double;
		else if (integer->isChecked())
			m_time.displayType = VipValueToTime::Integer;
		else
			m_time.displayType = VipValueToTime::AbsoluteDateTime;
	}
	else if (action && action->property("unit").toString() == "offset")
	{
		m_time.fixedStartValue = !action->isChecked();
	}
	else
	{
		//trigger a time unit or automatic unit
		QString action_text = action->property("unit").toString();
		if (action_text != "auto")
		{
			m_auto_unit = false;
			QList<QAction*> acts = this->menu()->actions();
			m_time.type = (VipValueToTime::TimeType)acts.indexOf(action);
			this->setText(action_text + " ");

			//uncheck the time actions
			for (int i = 0; i < 8; ++i)
			{
				if (acts[i] != action)
				{
					acts[i]->blockSignals(true);
					acts[i]->setChecked(false);
					acts[i]->blockSignals(false);
				}
			}

		}
		else
			m_auto_unit = true;
	}

	Q_EMIT timeUnitChanged();
}




class VipCloseToolBar::PrivateData
{
public:
	QPointer<QWidget> widget;
	QAction * minimize;
	QAction * maximize;
	QAction * close;
	QPoint pt;
	QPoint previous_pos;
};

VipCloseToolBar::VipCloseToolBar(QWidget  * widget, QWidget * parent)
	:QToolBar(parent)
{
	m_data = new PrivateData();

	setIconSize(QSize(18, 18));
	QWidget* empty = new QWidget();
	empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	addWidget(empty);
	m_data->minimize = addAction(vipIcon("minimize.png"), "Minimize window");
	m_data->maximize = addAction(vipIcon("maximize.png"), "Maximize window");
	m_data->close = addAction(vipIcon("close.png"), "Close window");

	connect(m_data->maximize, SIGNAL(triggered(bool)), this, SLOT(maximizeOrShowNormal()));

	setWidget(widget);
}

VipCloseToolBar::~VipCloseToolBar()
{
	delete m_data;
}

void VipCloseToolBar::setWidget(QWidget* widget)
{
	if (QWidget * w = m_data->widget)
	{
		w->removeEventFilter(this);
		disconnect(m_data->minimize, SIGNAL(triggered(bool)), w, SLOT(showMinimized()));
		disconnect(m_data->close, SIGNAL(triggered(bool)), w, SLOT(close()));
	}

	m_data->widget = widget;
	if (widget)
	{
		widget->installEventFilter(this);
		connect(m_data->minimize, SIGNAL(triggered(bool)), widget, SLOT(showMinimized()));
		connect(m_data->close, SIGNAL(triggered(bool)), widget, SLOT(close()));
	}
}

QWidget * VipCloseToolBar::widget() const
{
	return m_data->widget;
}

void VipCloseToolBar::maximizeOrShowNormal()
{
	if (!widget())
		return;

	if (widget()->isMaximized())
		widget()->showNormal();
	else
		widget()->showMaximized();
}

bool VipCloseToolBar::eventFilter(QObject*, QEvent* evt)
{
	if (evt->type() == QEvent::WindowStateChange)
	{
		if (!widget())
			return false;

		if (widget()->isMaximized())
		{
			m_data->maximize->setText("Restore");
			m_data->maximize->setIcon(vipIcon("restore.png"));
		}
		else
		{
			m_data->maximize->setText("Maximize");
			m_data->maximize->setIcon(vipIcon("maximize.png"));
		}
	}
	return false;
}

#include <QMouseEvent>

void VipCloseToolBar::mouseDoubleClickEvent(QMouseEvent *)
{
	VipCloseToolBar::maximizeOrShowNormal();
}

void VipCloseToolBar::mousePressEvent(QMouseEvent * evt)
{
	if (!widget())
		return;

	m_data->pt = widget()->mapToParent(evt->pos());
	m_data->previous_pos = widget()->pos();
}

void VipCloseToolBar::mouseReleaseEvent(QMouseEvent *)
{
	m_data->pt = QPoint();
}

void VipCloseToolBar::mouseMoveEvent(QMouseEvent * evt)
{
	if (!widget())
		return;

	if (m_data->pt != QPoint())
	{
		QPoint diff = widget()->mapToParent(evt->pos()) - m_data->pt;
		widget()->move(m_data->previous_pos + diff);
	}
}


class VipGenericDialog::PrivateData
{
public:
	PrivateData() :ok("Ok"), cancel("Cancel"), apply("Apply") {}
	QPushButton ok;
	QPushButton cancel;
	QPushButton apply;
};

VipGenericDialog::VipGenericDialog(QWidget * panel, const QString & title, QWidget * parent)
	:QDialog(//parent ? parent : vipGetMainWindow()
parent)
{
	m_data = new PrivateData();
	hide();
	//centerWidget(this);
	setWindowTitle(title);

	m_data->apply.hide();
	connect(&m_data->ok, SIGNAL(clicked(bool)), this, SLOT(accept()));
	connect(&m_data->cancel, SIGNAL(clicked(bool)), this, SLOT(reject()));

	QHBoxLayout * buttonLayout = new QHBoxLayout();
	buttonLayout->setSpacing(0);
	buttonLayout->addStretch(1);
	buttonLayout->addWidget(&m_data->ok);
	buttonLayout->addWidget(&m_data->apply);
	buttonLayout->addSpacing(5);
	buttonLayout->addWidget(&m_data->cancel);
	buttonLayout->addStretch(1);

	QVBoxLayout * globalLayout = new QVBoxLayout();
	globalLayout->setSpacing(0);
	globalLayout->addWidget(panel);
	globalLayout->addLayout(buttonLayout);
	setLayout(globalLayout);

	//this->setWindowFlags(Qt::Tool|Qt::WindowStaysOnTopHint|Qt::CustomizeWindowHint|Qt::WindowCloseButtonHint);
};

VipGenericDialog::~VipGenericDialog()
{
	delete m_data;
}

QPushButton * VipGenericDialog::applyButton()
{
	return &m_data->apply;
}





#include <QMouseEvent>
#include <QDrag>
#include <QSizeGrip>
#include <QWidgetAction>

class VipDragMenu::PrivateData
{
public:
	PrivateData() :widget(NULL) {}
	QPoint pos;
	QPointer<QWidget> widget;
	QSizeGrip *grip;
	QMap<QMimeData*, QPointer<QDrag> > drag;
};

VipDragMenu::VipDragMenu(QWidget * parent)
	:QMenu(parent) {
	m_data = new PrivateData();
	m_data->grip = new QSizeGrip(this);
	m_data->grip->setVisible(false);
}

VipDragMenu::VipDragMenu(const QString & title, QWidget * parent)
	:QMenu(title, parent) {
	m_data = new PrivateData();
	m_data->grip = new QSizeGrip(this);
}

VipDragMenu::~VipDragMenu()
{
	for (QMap<QMimeData*, QPointer<QDrag > >::iterator it = m_data->drag.begin(); it != m_data->drag.end(); ++it)
		if (it.value())
			delete it.value();

	delete m_data;
}

void VipDragMenu::setWidget(QWidget * w)
{
	if (m_data->widget)
		m_data->widget->removeEventFilter(this);

	clear();
	if (w)
	{
		QWidgetAction * action = new QWidgetAction(this);
		action->setDefaultWidget(w);
		addAction(action);

		w->installEventFilter(this);
	}
	m_data->widget = w;
	m_data->grip->raise();

	resize(w->size() + QSize(6, 6));
}

QWidget * VipDragMenu::widget() const
{
	return m_data->widget;
}

void VipDragMenu::setResizable(bool enable)
{
	m_data->grip->setVisible(enable);
}

bool VipDragMenu::isResizable() const
{
	return m_data->grip->isVisible();
}

void VipDragMenu::mousePressEvent(QMouseEvent * evt)
{
	m_data->pos = evt->pos();
	QMenu::mousePressEvent(evt);
}

void VipDragMenu::mouseReleaseEvent(QMouseEvent * evt)
{
	m_data->pos = QPoint(0, 0);
	QMenu::mouseReleaseEvent(evt);
}

static void execDrag(QMimeData * m)
{
	QDrag d(vipGetMainWindow());
	d.setMimeData(m);
	//Qt::DropAction drop =
 d.exec();
	//QObject* w = d.target();
	QCoreApplication::removePostedEvents(&d, QEvent::DeferredDelete);
}

void VipDragMenu::mouseMoveEvent(QMouseEvent * evt)
{
	QPoint pt = evt->pos();
	if ((pt - m_data->pos).manhattanLength() > 5 && m_data->pos != QPoint(0, 0))
	{
		//start dragging
		QAction * action = this->actionAt(pt);
		if (!action)
		{
			m_data->pos = QPoint(0, 0);
			return;
		}

		QMimeData * mime = action->property("QMimeData").value<QMimeData*>();
		if (!mime )
		{
			m_data->pos = QPoint(0, 0);
			return;
		}

		if (m_data->drag.find(mime) == m_data->drag.end())
			m_data->drag[mime] = new QDrag(this);

		//this->close();
		QDrag * drag = m_data->drag[mime];
		drag->setMimeData(mime);
		drag->exec();
		QCoreApplication::removePostedEvents(drag, QEvent::DeferredDelete);
		//QCoreApplication::processEvents();
		//QMetaObject::invokeMethod(QCoreApplication::instance(), std::bind(execDrag, mime), Qt::QueuedConnection);
		m_data->pos = QPoint(0, 0);

	}
	else
	{
		QMenu::mouseMoveEvent(evt);
	}
}

void VipDragMenu::resizeEvent(QResizeEvent *)
{
	if (m_data->widget)
	{
		m_data->widget->removeEventFilter(this);
		m_data->widget->resize(this->size() - QSize(6, 6));
		m_data->widget->installEventFilter(this);
		this->resize(m_data->widget->size() + QSize(6, 6));
	}

	m_data->grip->move(width() - m_data->grip->width(), height() - m_data->grip->height());
	m_data->grip->raise();
}

bool VipDragMenu::eventFilter(QObject *, QEvent * evt)
{
	if (evt->type() == QEvent::Resize)
	{
		if (m_data->widget)
			this->resize(m_data->widget->size() + QSize(6, 6));
	}
	return false;
}








class VipShowWidgetOnHover::PrivateData
{
public:
	QPointer<QWidget> hover;
	QList<QPointer<QWidget> > hovers;
	QPointer<QWidget> show;
	int showDelay;
	int hideDelay;
	int hideAfter;
	double currentSize;
	bool enable;
	qint64 start;
	QTimer timer;
};

VipShowWidgetOnHover::VipShowWidgetOnHover(QObject * parent)
	:QObject(parent)
{
	m_data = new PrivateData();
	m_data->timer.setSingleShot(true);
	m_data->showDelay = 500;
	m_data->hideDelay = 500;
	m_data->hideAfter = 500;
	m_data->currentSize = 0;
	m_data->start = 0;
	m_data->enable = true;
	m_data->timer.setInterval(10);
	m_data->timer.setSingleShot(true);
}
VipShowWidgetOnHover::~VipShowWidgetOnHover()
{
	if (m_data->hover)
		m_data->hover->removeEventFilter(this);
	if (m_data->show)
		m_data->show->removeEventFilter(this);
	delete m_data;
}

void VipShowWidgetOnHover::setHoverWidget(QWidget * hover)
{
	if (m_data->hover)
		m_data->hover->removeEventFilter(this);
	m_data->hover = NULL;
	for (int i = 0; i < m_data->hovers.size(); ++i)
		if (m_data->hovers[i])
			m_data->hovers[i]->removeEventFilter(this);
	m_data->hovers.clear();

	m_data->hover = hover;
	if (m_data->hover)
		m_data->hover->installEventFilter(this);
}
QWidget * VipShowWidgetOnHover::hoverWidget() const
{
	return m_data->hover;
}

void VipShowWidgetOnHover::setHoverWidgets(const QList<QWidget*> & hovers)
{
	if (m_data->hover)
		m_data->hover->removeEventFilter(this);
	m_data->hover = NULL;
	for (int i = 0; i < m_data->hovers.size(); ++i)
		if (m_data->hovers[i])
			m_data->hovers[i]->removeEventFilter(this);
	m_data->hovers.clear();

	for (int i = 0; i < hovers.size(); ++i) {
		if (hovers[i]) {
			m_data->hovers.append(hovers[i]);
			hovers[i]->installEventFilter(this);
		}
	}
}
QList<QWidget*> VipShowWidgetOnHover::hoverWidgets() const
{
	QList<QWidget*> res;
	for (int i = 0; i < m_data->hovers.size(); ++i)
		if (m_data->hovers[i])
			res.append(m_data->hovers[i]);
	return res;
}

void VipShowWidgetOnHover::setShowWidget(QWidget * show)
{
	if (m_data->show)
		m_data->show->removeEventFilter(this);
	m_data->show = show;
	if (m_data->show)
		m_data->show->installEventFilter(this);
}
QWidget * VipShowWidgetOnHover::showWidget() const
{
	return m_data->show;
}

void VipShowWidgetOnHover::setShowDelay(int msecs)
{
	m_data->showDelay = msecs;
}
int VipShowWidgetOnHover::showDelay() const
{
	return m_data->showDelay;
}

void VipShowWidgetOnHover::setHideDelay(int msecs)
{
	m_data->hideDelay = msecs;
}
int VipShowWidgetOnHover::hideDelay() const
{
	return m_data->hideDelay;
}

void VipShowWidgetOnHover::setHideAfter(int msecs)
{
	m_data->hideAfter = msecs;
}
int VipShowWidgetOnHover::hideAfter() const
{
	return m_data->hideAfter;
}

void VipShowWidgetOnHover::setEnabled(bool enable)
{
	m_data->enable = enable;
	if (!enable && m_data->show)
		m_data->show->setMaximumHeight(10000);
}
bool VipShowWidgetOnHover::isEnabled() const
{
	return m_data->enable;
}

void VipShowWidgetOnHover::startShow()
{
	if (!m_data->enable)
		return;
	QSize hint = m_data->show->sizeHint();
	//QSize cur = m_data->show->size();
	//disconnect any possible startHide()
	m_data->timer.stop();
	m_data->timer.disconnect();
	m_data->timer.setInterval(10);
	double speed = hint.height() / (double)m_data->showDelay;
	m_data->currentSize += (QDateTime::currentMSecsSinceEpoch() - m_data->start) * speed;
	int h = m_data->currentSize;
	m_data->show->setMaximumHeight(h);
	m_data->show->show();
	if (m_data->show->height() < hint.height()) {
		connect(&m_data->timer, SIGNAL(timeout()), this, SLOT(startShow()));
		m_data->timer.start();
	}
	else {
		m_data->show->setMaximumHeight(10000);
	}
}

void VipShowWidgetOnHover::startHide()
{
	if (!m_data->enable)
		return;
	QSize hint = m_data->show->sizeHint();
	//QSize cur = m_data->show->size();
	//disconnect any possible startShow()
	m_data->timer.stop();
	m_data->timer.disconnect();
	m_data->timer.setInterval(10);
	double speed = hint.height() / (double)m_data->showDelay;
	m_data->currentSize -= (QDateTime::currentMSecsSinceEpoch() - m_data->start) * speed;
	if (m_data->currentSize < 0) m_data->currentSize = 0;
	int h = m_data->currentSize;
	m_data->show->setMaximumHeight(h);
	if (m_data->show->height() > 0) {
		connect(&m_data->timer, SIGNAL(timeout()), this, SLOT(startHide()));
		m_data->timer.start();
	}
	else {
		m_data->show->hide();
		m_data->show->setMaximumHeight(10000);
	}
}

void VipShowWidgetOnHover::resetStart()
{
	if (!m_data->enable)
		return;
	m_data->start = QDateTime::currentMSecsSinceEpoch();
}

bool VipShowWidgetOnHover::eventFilter(QObject * //watched
, QEvent * evt)
{
	if (!m_data->enable)
		return false;
	if ((!m_data->hover && m_data->hovers.isEmpty()) || !m_data->show)
		return false;
	if (evt->type() == QEvent::Enter)
	{
		m_data->start = QDateTime::currentMSecsSinceEpoch();
		m_data->currentSize = m_data->show->isVisible() ? m_data->show->height() : 0;
		startShow();
	}
	else if (evt->type() == QEvent::Leave)
	{
		QSize hint = m_data->show->sizeHint();
		QSize cur = m_data->show->size();
		if (cur.height() < hint.height()) {
			//if we leave before fully shown, hide completely
			m_data->timer.stop();
			m_data->timer.disconnect();
			m_data->show->hide();
		}
		else {
			m_data->timer.setInterval(m_data->hideAfter);
			connect(&m_data->timer, SIGNAL(timeout()), this, SLOT(resetStart()));
			connect(&m_data->timer, SIGNAL(timeout()), this, SLOT(startHide()));
			m_data->currentSize = m_data->show->isVisible() ? m_data->show->height() : 0;
			m_data->timer.start();
		}
	}
	return false;
}




VipFunctionDispatcher<1> & vipFDObjectEditor()
{
	static VipFunctionDispatcher<1> disp;
	return disp;
}

QWidget * vipObjectEditor(const QVariant & obj)
{
	VipFunctionDispatcher<1>::function_list_type lst = vipFDObjectEditor().match(obj);
	if (lst.size())
		return lst.back()(obj).value<QWidget*>();
	return NULL;
}

bool vipHasObjectEditor(const QVariant & obj)
{
	bool res = vipFDObjectEditor().match(obj).size() > 0;
	return res;
}
