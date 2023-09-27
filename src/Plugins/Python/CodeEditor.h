

#ifndef CODEEDITOR_H
 #define CODEEDITOR_H

 #include <QPlainTextEdit>
 #include <QFileInfo>
 #include <QObject>

#include "PyConfig.h"

 class QPaintEvent;
 class QResizeEvent;
 class QSize;
 class QWidget;

 class BaseHighlighter;
 class LineNumberArea;


 typedef QMap<QString, QString> StringMap;

 class PYTHON_EXPORT CodeEditor : public QPlainTextEdit
 {
    Q_OBJECT
	Q_PROPERTY(QString defaultStyle WRITE setDefaultStyle)
 public:
     CodeEditor(QWidget *parent = 0);
	 ~CodeEditor();

	 static QList<CodeEditor*> editors();

     bool OpenFile(const QString & filename);
     bool SaveToFile(const QString & filename);
     const QFileInfo & FileInfo() const;

	 /**
	 Returns true if the editor content is empty and if it is not saved to the disk.
	 This means that the editor can used to load another file's content.
	 */
	 bool isEmpty() const;

     void lineNumberAreaPaintEvent(QPaintEvent *event);
     int lineNumberAreaWidth();

	 QColor lineAreaBackground() const;
	 void setLineAreaBackground(const QColor &);

	 QColor lineAreaBorder() const;
	 void setLineAreaBorder(const QColor &);

	 QColor lineNumberColor() const;
	 void setLineNumberColor(const QColor &);

	 QFont lineNumberFont() const;
	 void setLineNumberFont(const QFont &);

	 void setCurrentLineColor(const QColor &);
	 QColor currentLineColor() const;

	 void setBackgroundColor(const QColor &);
	 QColor backgroundColor() const;

	 void setBorderColor(const QColor &);
	 QColor borderColor() const;

	 void setTextColor(const QColor &);
	 QColor textColor() const;

	 void setColorScheme(BaseHighlighter * h);
	 BaseHighlighter * colorScheme() const;

	 static QList<BaseHighlighter*> colorSchemes();
	 static QList<BaseHighlighter*> colorSchemes(const QString & extension);
	 static QStringList colorSchemesNames(const QString & type);
	 static BaseHighlighter * colorScheme(const QString & type, const QString & name);
	 static QString typeForExtension(const QString & ext);
	 static void registerColorScheme(BaseHighlighter*);

	 static void setStdColorSchemeForType(const QString & type, BaseHighlighter*);
	 static void setStdColorSchemeForType(const QString & type, const QString & name);
	 static BaseHighlighter* stdColorSchemeForType(const QString & type);
	 static BaseHighlighter* stdColorSchemeForExt(const QString & extension);

	 static StringMap stdColorSchemes();
	 static void setStdColorSchemes(const StringMap & map);
	 /**
	 Set a default style using concatenation of "type:name"
	 */
	 static void setDefaultStyle(const QString & type_and_name);
	 
public Q_SLOTS:
	void reload();

 protected:
     void resizeEvent(QResizeEvent *event);

 private Q_SLOTS:
     void updateLineNumberAreaWidth(int newBlockCount);
     void highlightCurrentLine();
     void updateLineNumberArea(const QRect &, int);

 Q_SIGNALS:
	 void saved(const QString&);

 private:
	 void formatStyleSheet();
     QWidget *m_lineNumberArea;
	 QColor m_lineAreaBackground;
	 QColor m_lineAreaBorder;
	 QColor m_lineNumberColor;
	 QFont m_lineNumberFont;
	 QColor m_currentLine;
	 QColor m_background;
	 QColor m_border;
	 QColor m_text;
	 QFileInfo m_info;
	 int m_line;
 };

 Q_DECLARE_METATYPE(StringMap)

 class LineNumberArea : public QWidget
 {
 public:
     LineNumberArea(CodeEditor *editor) : QWidget(editor) {
         codeEditor = editor;
     }

     QSize sizeHint() const {
         return QSize(codeEditor->lineNumberAreaWidth(), 0);
     }

 protected:
     void paintEvent(QPaintEvent *event) {
         codeEditor->lineNumberAreaPaintEvent(event);
     }

 private:
     CodeEditor *codeEditor;
 };





 #endif
