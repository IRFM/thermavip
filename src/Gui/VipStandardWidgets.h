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

#ifndef VIP_STANDARD_WIDGETS_H
#define VIP_STANDARD_WIDGETS_H

#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QToolBar>
#include <QMessageBox>


#include "VipConfig.h"
#include "VipDataType.h"
#include "VipScaleDraw.h"
#include "VipText.h"
#include "VipPimpl.h"

/// \addtogroup Gui
/// @{

// forward declaration
class QGridLayout;

class VIP_GUI_EXPORT VipFindChidren
{
public:
	static QList<QObject*> children(const QString& name = QString());

	template<class T>
	static QList<T> findChildren(const QString& name = QString())
	{
		QList<T> res;
		QList<QObject*> c = children(name);
		for (int i = 0; i < c.size(); ++i)
			if (T tmp = qobject_cast<T>(c[i]))
				res << tmp;
		return res;
	}
};

/// Helper class to detect whenever a widget loose the focus because of a keyboard hit.
class VIP_GUI_EXPORT VipDetectLooseFocus : public QObject
{
	Q_OBJECT

public:
	VipDetectLooseFocus(QWidget* parent);
	~VipDetectLooseFocus();

	bool eventFilter(QObject* watched, QEvent* event);

Q_SIGNALS:
	void focusLost();
};

class VIP_GUI_EXPORT VipVerticalLine : public QFrame
{
	Q_OBJECT
public:
	VipVerticalLine(QWidget* parent = nullptr);
};

class VIP_GUI_EXPORT VipHorizontalLine : public QFrame
{
	Q_OBJECT
public:
	VipHorizontalLine(QWidget* parent = nullptr);
};

class VIP_GUI_EXPORT VipLineWidget
{
public:
	static QFrame* createHLine(QWidget* parent = nullptr);
	static QFrame* createVLine(QWidget* parent = nullptr);
	static QFrame* createSunkenHLine(QWidget* parent = nullptr);
	static QFrame* createSunkenVLine(QWidget* parent = nullptr);
};

class VIP_GUI_EXPORT VipFileDialog
{
public:
	/// Open a dialog box to open a directory.
	/// The selected path will be kept in memory and used in the next call to #VipFileDialog::getExistingDirectory, #VipFileDialog::getOpenFileName, #VipFileDialog::getOpenFileNames or
	/// #VipFileDialog::getSaveFileName.
	static QString getExistingDirectory(QWidget* parent, const QString& caption, QFileDialog::Options options = QFileDialog::Options());
	/// Open a dialog box to select a file.
	/// The selected path will be kept in memory and used in the next call to #VipFileDialog::getExistingDirectory, #VipFileDialog::getOpenFileName, #VipFileDialog::getOpenFileNames or
	/// #VipFileDialog::getSaveFileName.
	static QString getOpenFileName(QWidget* parent,
				       const QString& caption,
				       const QString& filter = QString(),
				       QString* selectedFilter = nullptr,
				       QFileDialog::Options options = QFileDialog::Options());
	/// Open a dialog box to select one or more files.
	/// The selected path will be kept in memory and used in the next call to #VipFileDialog::getExistingDirectory, #VipFileDialog::getOpenFileName, #VipFileDialog::getOpenFileNames or
	/// #VipFileDialog::getSaveFileName.
	static QStringList getOpenFileNames(QWidget* parent,
					    const QString& caption,
					    const QString& filter = QString(),
					    QString* selectedFilter = nullptr,
					    QFileDialog::Options options = QFileDialog::Options());
	/// Open a dialog box to select one file in saving mode.
	/// The selected path will be kept in memory and used in the next call to #VipFileDialog::getExistingDirectory, #VipFileDialog::getOpenFileName, #VipFileDialog::getOpenFileNames or
	/// #VipFileDialog::getSaveFileName.
	static QString getSaveFileName(QWidget* parent,
				       const QString& caption,
				       const QString& filter = QString(),
				       QString* selectedFilter = nullptr,
				       QFileDialog::Options options = QFileDialog::Options());
	static QString getSaveFileName2(QWidget* parent,
					const QString& filename,
					const QString& caption,
					const QString& filter = QString(),
					QString* selectedFilter = nullptr,
					QFileDialog::Options options = QFileDialog::Options());

	/// Set the default directory for the next call to one of the file dialog functions
	static void setDefaultDirectory(const QString& dirname);
};

class VIP_GUI_EXPORT VipStandardWidgets
{
	static QString format(const QString& str);

public:
	static QWidget* fromName(const QString& name);
	static QWidget* fromStyleSheet(const QString& style_sheet);

	/// Create the style sheet for a VipSpinBox.
	/// example:
	/// \code
	///		QWidget *w = fromStyleSheet(spinBox(0,10,1,2));
	/// w2->show();
	/// \endcode
	static QString spinBox(int min, int max, int step, int value)
	{
		return "VipSpinBox{ qproperty-minimum:" + QString::number(min) + "; qproperty-maximum:" + QString::number(max) + "; qproperty-singleStep:" + QString::number(step) +
		       "; qproperty-value:" + QString::number(value) + "; }";
	}

	/// Create the style sheet for a VipDoubleSpinBox.
	/// example:
	/// \code
	///		QWidget *w = fromStyleSheet(doubleSpinBox(0.1,10.2,0.1,2.5));
	/// w2->show();
	/// \endcode
	static QString doubleSpinBox(double min, double max, double step, double value)
	{
		return "VipDoubleSpinBox{  qproperty-minimum:" + QString::number(min) + "; qproperty-maximum:" + QString::number(max) + "; qproperty-singleStep:" + QString::number(step) +
		       "; qproperty-value:" + QString::number(value) + ";}";
	}

	/// Create the style sheet for a VipDoubleSliderEdit.
	/// example:
	/// \code
	///	QWidget *w = fromStyleSheet(doubleSliderEdit(2.5,0.1,10.2,0.1));
	/// w2->show();
	/// \endcode
	static QString doubleSliderEdit(double min, double max, double step, double value, bool show_spin_box)
	{
		return "VipDoubleSliderEdit{ qproperty-minimum:" + QString::number(min) + "; qproperty-maximum:" + QString::number(max) + "; qproperty-singleStep:" + QString::number(step) +
		       "; qproperty-value:" + QString::number(value) + "; qproperty-showSpinBox:" + QString::number(show_spin_box) + ";}";
	}

	/// Create the style sheet for a VipDoubleEdit.
	/// example:
	/// \code
	///		QWidget *w = fromStyleSheet(doubleEdit(2.5));
	/// w2->show();
	/// \endcode
	static QString doubleEdit(double value, const QString& d_format = "%g") { return "VipDoubleEdit{ qproperty-value:" + QString::number(value) + "; qproperty-format:" + format(d_format) + ";}"; }

	/// Create the style sheet for a VipComboBox.
	/// example:
	/// \code
	///		QWidget *w = fromStyleSheet(comboBox("choice1,choice2,choice3","choice2"));
	/// w2->show();
	/// \endcode
	static QString comboBox(const QString& choices, const QString& value) { return "VipComboBox{ qproperty-choices:" + format(choices) + "; qproperty-value:" + format(value) + " ;}"; }

	/// Create the style sheet for a VipEnumEdit.
	/// example:
	/// \code
	///	QWidget *w = fromStyleSheet(enumWidget("enum1,enum2,enum3","",1));
	/// w2->show();
	/// \endcode
	static QString enumWidget(const QString& enums, const QString& values, int value)
	{
		return "VipEnumEdit{ qproperty-enumNames:" + format(enums) + "; qproperty-enumValues:" + format(values) + "; qproperty-value:" + QString::number(value) + " ;}";
	}

	/// Create the style sheet for a VipLineEdit.
	/// example:
	/// \code
	///		QWidget *w = fromStyleSheet(lineEdit("text"));
	/// w2->show();
	/// \endcode
	static QString lineEdit(const QString& value) { return "VipLineEdit{ qproperty-value:" + format(value) + " ;}"; }

	/// Create the style sheet for a VipBrushWidget.
	/// example:
	/// \code
	///		QWidget *w = fromStyleSheet(brushWidget("red"));
	/// w2->show();
	/// \endcode
	static QString brushWidget(const QString& color) { return "VipBrushWidget{ qproperty-value:" + color + " ;}"; }

	/// Create the style sheet for a VipPenWidget.
	/// example:
	/// \code
	///		QWidget *w = fromStyleSheet(penWidget("red",2.5,2,2,8));
	/// w2->show();
	/// \endcode
	static QString penWidget(const QString& brush, double width, int style, int cap, int join)
	{
		return "VipPenWidget{ qproperty-brush:" + brush + " ;qproperty-width:" + QString::number(width) + " ;qproperty-style:" + QString::number(style) +
		       " ;qproperty-cap:" + QString::number(cap) + " ;qproperty-join:" + QString::number(join) + " ;}";
	}

	/// Create the style sheet for a VipPenWidget.
	/// example:
	/// \code
	///		QWidget *w = fromStyleSheet(penButton("Pen",red,2.5,2,2,8));
	/// w2->show();
	/// \endcode
	static QString penButton(const QString& mode, const QString& brush, double width, int style, int cap, int join)
	{
		return "VipPenButton{  qproperty-mode:" + format(mode) + "; qproperty-brush:" + brush + " ;qproperty-width:" + QString::number(width) + " ;qproperty-style:" + QString::number(style) +
		       " ;qproperty-cap:" + QString::number(cap) + " ;qproperty-join:" + QString::number(join) + " ;}";
	}

	/// Create the style sheet for a VipFileName.
	/// example:
	/// \code
	///		QWidget *w = fromStyleSheet(fileName("Open","","Open image","Image files (*.png *.bmp *.jpeg)"));
	/// w2->show();
	/// \endcode
	static QString fileName(const QString& mode, const QString& filename, const QString& title, const QString& filters)
	{
		return "VipFileName{  qproperty-mode:" + format(mode) + "; qproperty-value:" + format(filename) + " ;qproperty-title:" + format(title) + " ;qproperty-filters:" + format(filters) +
		       " ;}";
	}
};

/// Reimplements QSpinBox to solve a strange bug with stylesheet (setting min or max values does nothing).
class VIP_GUI_EXPORT VipSpinBox : public QSpinBox
{
	Q_OBJECT
public:
	VipSpinBox(QWidget* parent = nullptr);

private Q_SLOTS:
	void valueHasChanged() { Q_EMIT genericValueChanged(value()); }

Q_SIGNALS:
	void genericValueChanged(const QVariant&);
};

/// A QCheckBox that defines the value property
class VIP_GUI_EXPORT VipBoolEdit : public QCheckBox
{
	Q_OBJECT
	Q_PROPERTY(bool value READ value WRITE setValue)

public:
	VipBoolEdit(QWidget* parent = nullptr);

	bool value() const;
	void setValue(bool);

Q_SIGNALS:
	void valueChanged(bool);
	void genericValueChanged(const QVariant&);

private Q_SLOTS:
	void changed();
};

/// Reimplements QDoubleSpinBox to solve a strange bug with stylesheet (setting min or max values does nothing).
/// VipDoubleSpinBox accepts both dot and comma as decimal separator.
class VIP_GUI_EXPORT VipDoubleSpinBox : public QDoubleSpinBox
{
	Q_OBJECT
public:
	VipDoubleSpinBox(QWidget* parent = nullptr);

	virtual QString textFromValue(double value) const;
	virtual double valueFromText(const QString& text) const;
	virtual QValidator::State validate(QString& text, int& pos) const;

private Q_SLOTS:
	void valueHasChanged() { Q_EMIT genericValueChanged(value()); }

Q_SIGNALS:
	void genericValueChanged(const QVariant&);
};

/// This class implements a standard QLineEdit that displays a fixed, non-editable prefix and/or suffix.  You can use it just like a QLineEdit, calling setText() and text() with the actual internal
/// text value.  Use setPrefix() and setSuffix() to change what is displayed in front and after this text.  Use fullText() to retrieve the complete string, including the prefix and the suffix.
class VIP_GUI_EXPORT VipPrefixSuffixLineEdit : public QLineEdit
{
	Q_OBJECT

public:
	VipPrefixSuffixLineEdit(const QString& prefix = QString(), const QString& suffix = QString(), QWidget* parent = 0);

	QString prefix() const;
	QString suffix() const;

	QLabel* prefixLabel() const;
	QLabel* suffixLabel() const;

public Q_SLOTS:
	void setPrefix(const QString& prefix);
	void setSuffix(const QString& suffix);

protected:
	void resetMargins();
	virtual void resizeEvent(QResizeEvent* evt);
	QLabel* _prefix;
	QLabel* _suffix;
};

class VIP_GUI_EXPORT VipDoubleEdit : public VipPrefixSuffixLineEdit
{
	Q_OBJECT
	Q_PROPERTY(double value READ value WRITE setValue)
	Q_PROPERTY(QString rightStyle READ rightStyle WRITE setRightStyle)
	Q_PROPERTY(QString wrongStyle READ wrongStyle WRITE setWrongStyle)
	Q_PROPERTY(QString format READ format WRITE setFormat)

public:
	VipDoubleEdit(QWidget* parent = nullptr);
	double value() const;
	const QString& rightStyle() const;
	const QString& wrongStyle() const;
	const QString& format() const;
	bool isValid() const;
	bool integerFormat() const;

	static double readValue(const QString& text, bool integer, bool* ok = nullptr);

public Q_SLOTS:
	void setValue(double value);
	void setRightStyle(const QString&);
	void setWrongStyle(const QString&);
	void setFormat(const QString&);
	void setIntegerFormat(bool integer);

Q_SIGNALS:
	void valueChanged(double);
	void genericValueChanged(const QVariant&);

private Q_SLOTS:
	void edited();
	void enterPressed();

private:
	QString m_rightStyle;
	QString m_wrongStyle;
	QString m_format;
	bool m_integer;
	double m_value;
};

class VIP_GUI_EXPORT VipMultiComponentDoubleEdit : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(VipNDDoubleCoordinate value READ value WRITE setValue)
	Q_PROPERTY(QString rightStyle READ rightStyle WRITE setRightStyle)
	Q_PROPERTY(QString wrongStyle READ wrongStyle WRITE setWrongStyle)
	Q_PROPERTY(QString format READ format WRITE setFormat)

	Q_PROPERTY(int fixedNumberOfComponents READ fixedNumberOfComponents WRITE setFixedNumberOfComponents)
	Q_PROPERTY(int maxNumberOfComponents READ maxNumberOfComponents WRITE setMaxNumberOfComponents)
	Q_PROPERTY(int integer READ integerFormat WRITE setIntegerFormat)
public:
	VipMultiComponentDoubleEdit(QWidget* parent = nullptr);
	~VipMultiComponentDoubleEdit();

	int fixedNumberOfComponents() const;
	int maxNumberOfComponents() const;
	QString separator() const;

	const QString& rightStyle() const;
	const QString& wrongStyle() const;
	const QString& format() const;
	bool isValid() const;
	bool integerFormat() const;

	VipNDDoubleCoordinate value() const;

public Q_SLOTS:
	void setRightStyle(const QString&);
	void setWrongStyle(const QString&);
	void setIntegerFormat(bool integer);
	void setFixedNumberOfComponents(int);
	void setMaxNumberOfComponents(int);

	void setSeparator(const QString& sep);
	void setFormat(const QString& format);

	void setValue(const VipNDDoubleCoordinate& value);

private Q_SLOTS:
	void edited();
	void enterPressed();

Q_SIGNALS:
	void valueChanged(const VipNDDoubleCoordinate&);
	void genericValueChanged(const QVariant&);

private:
	VipNDDoubleCoordinate readValue(bool* ok = nullptr) const;
	void applyStyle();
	void applyFormat();
	
	VIP_DECLARE_PRIVATE_DATA();
};

class QSlider;

/// A horizontal slider editing a double value.
/// If property 'showSpinBox' is true, a VipDoubleSpinBox is also displayed on the right
class VIP_GUI_EXPORT VipDoubleSliderEdit : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(double value READ value WRITE setValue)
	Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
	Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
	Q_PROPERTY(double step READ singleStep WRITE setSingleStep)
	Q_PROPERTY(bool showSpinBox READ showSpinBox WRITE setShowSpinBox)

public:
	VipDoubleSliderEdit(QWidget* parent = nullptr);

	double value() const;
	double minimum() const;
	double maximum() const;
	double singleStep() const;
	bool showSpinBox() const;

public Q_SLOTS:
	void setValue(double);
	void setMinimum(double);
	void setMaximum(double);
	void setSingleStep(double);
	void setShowSpinBox(bool);

private Q_SLOTS:
	void valueHasChanged();
	void setupSlider();

Q_SIGNALS:
	void valueChanged(double);
	void genericValueChanged(const QVariant&);

private:
	QSlider* m_slider;
	VipDoubleSpinBox* m_spin;
};

/// Reimplements QLineEdit to provide a 'value' property for the perspective tool.
class VIP_GUI_EXPORT VipLineEdit : public QLineEdit
{
	Q_OBJECT
	Q_PROPERTY(QString value READ text WRITE setText)

public:
	VipLineEdit(QWidget* parent = nullptr)
	  : QLineEdit(parent)
	{
		connect(this, SIGNAL(returnPressed()), this, SLOT(valueHasChanged()));
	}

private Q_SLOTS:
	void valueHasChanged()
	{
		Q_EMIT valueChanged(text());
		Q_EMIT genericValueChanged(text());
	}

Q_SIGNALS:
	void valueChanged(const QString& v);
	void genericValueChanged(const QVariant&);
};

class VipLineEditIcon : public VipLineEdit
{
	Q_OBJECT

public:
	VipLineEditIcon(QWidget* parent = Q_NULLPTR);
	~VipLineEditIcon();
public Q_SLOTS:
	void setIcon(const QIcon& icon);

protected:
	virtual void paintEvent(QPaintEvent* event);

private:
	QIcon m_icon;
};

/// Reimplement QComboBox to provide a 'value' property for the perspective tool.
class VIP_GUI_EXPORT VipComboBox : public QComboBox
{
	Q_OBJECT
	Q_PROPERTY(QString value READ currentText WRITE setCurrentText)
	Q_PROPERTY(QString choices READ choices WRITE setChoices)

public:
	VipComboBox(QWidget* parent = nullptr);

	QStringList items() const
	{
		QStringList res;
		for (int i = 0; i < count(); ++i)
			res << itemText(i);
		return res;
	}

	QString choices() const { return items().join(","); }

	virtual void showPopup()
	{
		emit openPopup();
		QComboBox::showPopup();
	}

	void setInnerDragDropEnabled(bool enable);
	bool innerDragDropEnabled() const;

protected:
	virtual bool eventFilter(QObject* obj, QEvent* evt);

public Q_SLOTS:
	// add the function setCurrentText, not provided by QComboBox
	void setCurrentText(const QString& text);
	void setChoices(const QString& choices)
	{
		clear();
		addItems(choices.split(",", VIP_SKIP_BEHAVIOR::SkipEmptyParts));
	}

private Q_SLOTS:
	void valueHasChanged();

Q_SIGNALS:
	void openPopup();
	void innerItemDropped(const QString& item);

	void valueChanged(const QString&);
	void genericValueChanged(const QVariant&);

private:
	bool m_innerDragDropEnabled;
};

/// A QComboBox used to edit an enumeration value.
/// Set the enumeration names with #setEnumNames.
/// By default, enumeration values start at 0 and are incremented by one for each enum name.
/// You can set custom enum values with #setEnumValues.
class VIP_GUI_EXPORT VipEnumEdit : public QComboBox
{
	Q_OBJECT
	Q_PROPERTY(QString value READ value WRITE setValue)
	Q_PROPERTY(QString enumNames READ enumNames WRITE setEnumNames)
	Q_PROPERTY(QString enumValues READ enumNames WRITE setEnumValues)

public:
	VipEnumEdit(QWidget* parent = nullptr);

	QString enumNames() const;
	QString enumValues() const;
	QString value() const;

public Q_SLOTS:
	/// Set the enum names as a string with comma separators
	void setEnumNames(const QString& choices);
	/// Set the enum values as a string with comma separators
	void setEnumValues(const QString& choices);
	/// Set the current enum value
	void setValue(const QString& value);

private Q_SLOTS:
	void valueHasChanged();

Q_SIGNALS:
	void valueChanged(const QString&);
	void genericValueChanged(const QVariant&);
};

class VIP_GUI_EXPORT VipColorWidget : public QToolButton
{
	Q_OBJECT

public:
	VipColorWidget(QWidget* parent = nullptr);
	const QColor& color() const { return m_color; }

public Q_SLOTS:
	void setColor(const QColor&);

Q_SIGNALS:
	void colorChanged(const QColor&);

private Q_SLOTS:
	void triggered();

private:
	QColor m_color;
};

/// Widget used to edit a QBrush object.
/// Provide a 'value' property for the perspective tool.
class VIP_GUI_EXPORT VipBrushWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(QBrush value READ brush WRITE setBrush)

	QBrush m_brush;
	QLabel m_image;
	QComboBox m_pattern;
	QToolButton m_color;

public:
	VipBrushWidget(const QBrush& brush = QBrush(), QWidget* parent = nullptr);

	void setBrush(const QBrush& brush);
	QBrush brush() const;

	void setColorOptionVisible(bool);
	bool colorOptionVisible() const;

private Q_SLOTS:

	void setPattern(int index);
	void setColor();
	void redraw();

Q_SIGNALS:

	void brushChanged(const QBrush&);
	void genericValueChanged(const QVariant&);
};

/// Widget used to edit a QPen object.
/// Provide a 'value' property for the perspective tool.
class VIP_GUI_EXPORT VipPenWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(QPen value READ pen WRITE setPen)

	QPen m_pen;

	VipBrushWidget m_brush;
	QComboBox m_style;
	QComboBox m_cap;
	QComboBox m_join;
	QDoubleSpinBox m_width;
	QLabel m_image;

public:
	VipPenWidget(const QPen& pen = QPen(), QWidget* parent = nullptr);

	QPen pen() const;

	void setColorOptionVisible(bool);
	bool colorOptionVisible() const;

public Q_SLOTS:

	void setPen(const QPen& pen);
	void setBrush(const QBrush&);
	void showFullOptions(bool sh);

private Q_SLOTS:

	void setWidth(double);
	void setStyle(int);
	void setCap(int);
	void setJoin(int);
	void redraw();

Q_SIGNALS:

	void penChanged(const QPen&);
	void genericValueChanged(const QVariant&);
};

/// Button used to edit a QPen object.
/// Provide a 'value' property for the perspective tool.
class VIP_GUI_EXPORT VipPenButton : public QToolButton
{
	Q_OBJECT
	Q_PROPERTY(QPen value READ pen WRITE setPen)
	Q_PROPERTY(Mode mode READ mode WRITE setMode)
	Q_ENUMS(Mode)

public:
	enum Mode
	{
		Color,
		Brush,
		Pen
	};

	VipPenButton(const QPen& pen = QPen(), QWidget* parent = nullptr);
	~VipPenButton();

	QPen pen() const;
	Mode mode() const;

	VipPenWidget* penWidget() const { return const_cast<VipPenWidget*>(m_pen); }

	void setColorOptionVisible(bool);
	bool colorOptionVisible() const;

public Q_SLOTS:

	void showFullPenOptions(bool sh);
	void setPen(const QPen& pen);
	void setBrush(const QBrush&);
	void setMode(Mode mode);

private Q_SLOTS:

	void triggered();
	void redraw();

Q_SIGNALS:

	void penChanged(const QPen&);
	void genericValueChanged(const QVariant&);

protected:
	void resizeEvent(QResizeEvent* evt);

	Mode m_mode;
	VipPenWidget* m_pen;
	QMenu* m_menu;
};



/// @brief Edit text font and color in a unique small tool button
class VipFontAndColor : public QLabel
{
	Q_OBJECT

	QLabel* d_font_button;
	QColor d_color;
	QFont d_font;
	int d_content = Font | Color;

public:
	enum EditableContent
	{
		Font = 2,
		Color = 4
	};

	VipFontAndColor(QWidget* parent = nullptr);

	QColor color() const;
	QFont font() const;

public Q_SLOTS:
	void setColor(const QColor& c);
	void setFont(const QFont& f);
	void setEditableContent(int);

Q_SIGNALS:
	void changed();

private Q_SLOTS:
	void editFont();
	void editColor();

	
protected:
	virtual void resizeEvent(QResizeEvent* evt);
	virtual bool eventFilter(QObject* watched, QEvent* event);
};


/// Widget used to edit a VipText object.
class VIP_GUI_EXPORT VipTextWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(VipText value READ getText WRITE setText)
public:
	enum EditableContent
	{
		String = 1,
		Font = 2,
		Color = 4
	};

	VipTextWidget(QWidget* parent = nullptr);
	~VipTextWidget(); 

	void setText(const VipText& t);
	VipText getText() const;

	QLineEdit* edit() const;

	int editableContent() const;
	void setEditableContent(int);

Q_SIGNALS:
	void changed(const VipText&);
	void genericValueChanged(const QVariant&);

private Q_SLOTS:
	void fontChoice();
	void textEdited();

private:
	VIP_DECLARE_PRIVATE_DATA();
};

class VIP_GUI_EXPORT VipBoxStyleWidget : public QWidget
{
	Q_OBJECT

public:
	VipBoxStyleWidget(QWidget* parent = nullptr);

	void setBoxStyle(const VipBoxStyle& box);
	VipBoxStyle getBoxStyle() const;

	VipPenButton* backgroundEditor() const { return const_cast<VipPenButton*>(&m_background); }
	VipPenButton* borderEditor() const { return const_cast<VipPenButton*>(&m_border); }

Q_SIGNALS:
	void boxStyleChanged(const VipBoxStyle&);

private Q_SLOTS:
	void emitBoxStyleChanged();

private:
	VipBoxStyle m_style;
	VipPenButton m_background;
	VipPenButton m_border;
	QSpinBox m_radius;
};

/// Widget used to edit a filename (QString).
class VIP_GUI_EXPORT VipFileName : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(QString value READ filename WRITE setFilename)
	Q_PROPERTY(QString filters READ filters WRITE setFilters)
	Q_PROPERTY(QString prefix READ prefix WRITE setPrefix)
	Q_PROPERTY(QString title READ title WRITE setTitle)
	Q_PROPERTY(QString default_path READ defaultPath WRITE setDefaultPath)
	Q_PROPERTY(Mode mode READ mode WRITE setMode)
	Q_ENUMS(Mode)

public:
	enum Mode
	{
		Open,
		OpenDir,
		Save
	};

	VipFileName(const QString& Filename, QWidget* parent = nullptr);
	VipFileName(QWidget* parent = nullptr);

	Mode mode() const;
	QString filename() const;
	
	QString filters() const;
	QString prefix() const;
	QString title() const;
	QString defaultPath() const;
	QString defaultOpenDir() const;
	QGridLayout* gridLayout();
	QLineEdit* edit() { return &m_edit; }

	void setDialogParent(QWidget* parent);

public Q_SLOTS:

	void setFilename(const QString& filename);
	void setFilters(const QString& filters);
	void setPrefix(const QString& prefix);
	void setTitle(const QString& title);
	void setMode(Mode mode);
	void setDefaultPath(const QString& path);
	void setDefaultOpenDir(const QString& dir);

private Q_SLOTS:

	void buttonTriggered();

Q_SIGNALS:

	void changed(const QString&);
	void genericValueChanged(const QVariant&);

protected:
	QGridLayout* m_layout;
	QLineEdit m_edit;
	QToolButton m_button;
	QString m_filters;
	QString m_title;
	QString m_prefix;
	QString m_default_path;
	QString m_default_open_dir;
	Mode m_mode;
	QWidget* m_dialog_parent{ nullptr };
	bool m_has_dialog_parent{ false };//TODO: use std::expected
};

class VipToolBar;
class VIP_GUI_EXPORT VipAdditionalToolBar : public QToolBar
{
	Q_OBJECT
public:
	VipAdditionalToolBar(VipToolBar* parent = nullptr);
	virtual bool eventFilter(QObject* watched, QEvent* event);

protected:
	virtual void showEvent(QShowEvent*);
	virtual void hideEvent(QHideEvent*);

private:
	VipToolBar* parent;
};

/// A QToolBar like class.
///
/// VipToolBar handle in a different way QAction that are not visible because the tool bar is not wide enough.
class VIP_GUI_EXPORT VipToolBar : public QToolBar
{
	Q_OBJECT

	friend class VipAdditionalToolBar;

public:
	// Q_OBJECT

	enum ShowAdditionals
	{
		ShowInMenu,
		ShowInToolBar
	};

	VipToolBar(QWidget* parent = nullptr);
	virtual ~VipToolBar();

	virtual QSize sizeHint() const;

	void setShowAdditionals(ShowAdditionals sh);
	ShowAdditionals showAdditionals() const;

	QToolButton* showButton() const;
	QMenu* showAdditionalMenu() const;
	QToolBar* showAdditionalToolBar() const;

	void setCustomBehaviorEnabled(bool);
	bool customBehaviorEnabled() const;

protected:
	virtual void showEvent(QShowEvent* evt);
	virtual void resizeEvent(QResizeEvent* evt);
	virtual void actionEvent(QActionEvent* evt);

private Q_SLOTS:
	void aboutToShow();
	void aboutToHide();
	void compute();
	void delayCompute();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};


/// @brief Edit a small 2D array
class VIP_GUI_EXPORT Vip2DArrayEditor : public QWidget
{
	Q_OBJECT
public:
	Vip2DArrayEditor();
	~Vip2DArrayEditor();

	VipNDArray array() const;
	void setText(const QString& text);
	void setArray(const VipNDArray& ar);

private Q_SLOTS:
	void textEntered();
	void finished();
Q_SIGNALS:
	void changed();

private:
	VIP_DECLARE_PRIVATE_DATA();
};



/// A \a QToolButton used to modify a time scale unit, based on a \a VipValueToTime object.
/// The \a VipValueToTime object returned by #currentValueToTime() should always be copied.
class VIP_GUI_EXPORT VipValueToTimeButton : public QToolButton
{
	Q_OBJECT
public:
	VipValueToTimeButton(QWidget* parent = nullptr);
	/// Returns the current \a VipValueToTime instance
	const VipValueToTime& currentValueToTime() const;
	/// Set the current time unit
	void setValueToTime(VipValueToTime::TimeType);
	VipValueToTime::TimeType valueToTime() const { return m_time.type; }

	void setDisplayType(VipValueToTime::DisplayType type);
	VipValueToTime::DisplayType displayType() const;

	void setDisplayTimeOffset(bool);
	bool displayTimeOffset() const;

	void setAutomaticUnit(bool auto_unit);
	bool automaticUnit() const;

Q_SIGNALS:
	void timeUnitChanged();

private Q_SLOTS:
	void timeUnitTriggered(QAction*);

private:
	QAction* findAction(const QString&) const;
	VipValueToTime m_time;
	bool m_auto_unit;
};

/// A tool bar that displays at its right extremity control buttons to close/minimize/maximize/restore a widget.
class VipCloseToolBar : public QToolBar
{
	Q_OBJECT

public:
	VipCloseToolBar(QWidget* widget = nullptr, QWidget* parent = nullptr);
	~VipCloseToolBar();

	void setWidget(QWidget* w);
	QWidget* widget() const;

private Q_SLOTS:
	void maximizeOrShowNormal();

protected:
	virtual bool eventFilter(QObject*, QEvent*);
	virtual void mouseDoubleClickEvent(QMouseEvent* event);
	virtual void mousePressEvent(QMouseEvent* evt);
	virtual void mouseReleaseEvent(QMouseEvent* evt);
	virtual void mouseMoveEvent(QMouseEvent* evt);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// \class VipGenericDialog
/// \brief VipGenericDialog is used to display a QWidget inside a QDialog containing an 'Ok' and 'Cancel' buttons (and optionally a Apply button)
class VIP_GUI_EXPORT VipGenericDialog : public QDialog
{
	Q_OBJECT
public:
	VipGenericDialog(QWidget* inner_panel, const QString& title = QString(), QWidget* parent = nullptr);
	~VipGenericDialog();

	QPushButton* applyButton();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

VIP_GUI_EXPORT QMessageBox::StandardButton vipInformation(const QString & title, const QString & text, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
VIP_GUI_EXPORT QMessageBox::StandardButton vipQuestion(const QString & title, const QString & text, QMessageBox::StandardButtons buttons = QMessageBox::Yes|QMessageBox::No , QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
VIP_GUI_EXPORT QMessageBox::StandardButton vipWarning(const QString & title, const QString & text, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

/// A QMenu whose actions might be dragable.
///
/// VipDragMenu is a QMenu with additional features.
///
/// The first feature is to display a widget instead of standard QAction. For that, use the #VipDragMenu::setWidget member.
///
/// The second feature is to provide dragable menu items. For make a QAction dragable, you need to set its property "QMimeData"
/// to a valid QMimeData pointer. Note that, once draged/dropped, the QMineData object is no longer valid and the menu content needs
/// to be regenerated. That's why the VipDragMenu should be called on the stack when using the drag feature.
class VIP_GUI_EXPORT VipDragMenu : public QMenu
{
	Q_OBJECT

public:
	VipDragMenu(QWidget* parent = 0);
	VipDragMenu(const QString& title, QWidget* parent = 0);
	~VipDragMenu();

	void setWidget(QWidget* w);
	QWidget* widget() const;

	void setResizable(bool enable);
	bool isResizable() const;

protected:
	virtual bool eventFilter(QObject* watched, QEvent* evt);
	virtual void mousePressEvent(QMouseEvent* evt);
	virtual void mouseReleaseEvent(QMouseEvent* evt);
	virtual void mouseMoveEvent(QMouseEvent* evt);
	virtual void resizeEvent(QResizeEvent* evt);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

Q_DECLARE_METATYPE(QMimeData*)

/// Small class used to show a widget when another one is hovered, and hide it when the mouse leave
/// either the hover widget or the shown one.
class VipShowWidgetOnHover : public QObject
{
	Q_OBJECT

public:
	VipShowWidgetOnHover(QObject* parent = nullptr);
	~VipShowWidgetOnHover();

	void setHoverWidget(QWidget* hover);
	void setHoverWidgets(const QList<QWidget*>& hovers);
	QWidget* hoverWidget() const;
	QList<QWidget*> hoverWidgets() const;

	void setShowWidget(QWidget* show);
	QWidget* showWidget() const;

	void setShowDelay(int msecs);
	int showDelay() const;

	void setHideDelay(int msecs);
	int hideDelay() const;

	void setHideAfter(int msecs);
	int hideAfter() const;

	void setEnabled(bool);
	bool isEnabled() const;

	virtual bool eventFilter(QObject* watched, QEvent* evt);

private Q_SLOTS:
	void startShow();
	void startHide();
	void resetStart();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

#include "VipFunctional.h"

VIP_REGISTER_QOBJECT_METATYPE(VipSpinBox*)
VIP_REGISTER_QOBJECT_METATYPE(VipDoubleSpinBox*)
VIP_REGISTER_QOBJECT_METATYPE(VipDoubleEdit*)
VIP_REGISTER_QOBJECT_METATYPE(VipMultiComponentDoubleEdit*)
VIP_REGISTER_QOBJECT_METATYPE(VipDoubleSliderEdit*)
VIP_REGISTER_QOBJECT_METATYPE(VipLineEdit*)
VIP_REGISTER_QOBJECT_METATYPE(VipComboBox*)
VIP_REGISTER_QOBJECT_METATYPE(VipEnumEdit*)
VIP_REGISTER_QOBJECT_METATYPE(VipBrushWidget*)
VIP_REGISTER_QOBJECT_METATYPE(VipPenWidget*)
VIP_REGISTER_QOBJECT_METATYPE(VipPenButton*)
VIP_REGISTER_QOBJECT_METATYPE(VipFileName*)

// Create a QWidget instance from any kind of object
// Signature: QWidget * (const QVariant&)
// It can be used to display an editor for any king of object, including VipProcessingObject instances and VipPlotItem instances, but
// also standard types like QPen and QBrush.
// The editor widget must have a property 'value' used to set and retrieve the edited object.
VIP_GUI_EXPORT VipFunctionDispatcher<1>& vipFDObjectEditor();
VIP_GUI_EXPORT QWidget* vipObjectEditor(const QVariant& obj);
VIP_GUI_EXPORT bool vipHasObjectEditor(const QVariant& obj);

/// @}
// end Gui

#endif
