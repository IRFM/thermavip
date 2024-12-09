#ifndef VIP_EDITOR_FILTER_H
#define VIP_EDITOR_FILTER_H

#include <QObject>
#include <QKeyEvent>

#include "VipConfig.h"

class VipTextEditor;

/// @brief Base class for VipTextEditor event filtering
///
class VIP_GUI_EXPORT VipEditorFilter : public QObject
{
	Q_OBJECT
public:
	VipEditorFilter(VipTextEditor* parent);
	~VipEditorFilter();

	/// @brief Returns the parent editor
	VipTextEditor* editor() const;

	/// @brief Filter events
	/// The default implementation defines the following shortcuts:
	/// -	TAB: indent selection
	/// -	SHIFT+TAB: unindent selection
	/// -	CTRL+K: comment selected lines
	/// -	CTRL+SHIFT+K: uncomment selected lines
	/// -	CTRL+S: emit saveTriggered()
	/// -	CTRL+F: emit searchTriggered
	virtual bool eventFilter(QObject* watched, QEvent* event);

	/// @brief Indent lines
	/// Default implementation adds a leading tabulation.
	virtual void indent(int fromline, int toline);

	/// @brief Unindent lines
	/// Default implementation removes a leading tabulation (if any).
	virtual void unindent(int fromline, int toline);

	/// @brief Comment lines
	virtual void comment(int fromline, int toline) {}

	/// @brief Uncomment lines
	virtual void uncomment(int fromline, int toline) {}

	void indentSelection();
	void unindentSelection();
	void commentSelection();
	void uncommentSelection();

public Q_SLOTS:
	void emitSaveTriggered() { Q_EMIT saveTriggered(); }
	void emitSearchTriggered() { Q_EMIT searchTriggered(); }

Q_SIGNALS:

	void saveTriggered();
	void searchTriggered();

private:
	VipTextEditor* m_editor;
};

/// @brief Filter class for Python file editor.
///
/// Indent lines with 4 spaces and comment lines with leading '#'.
///
class VIP_GUI_EXPORT VipPyEditorFilter : public VipEditorFilter
{
	Q_OBJECT

public:
	VipPyEditorFilter(VipTextEditor* parent);
	~VipPyEditorFilter();

	virtual void indent(int fromline, int toline);
	virtual void unindent(int fromline, int toline);
	virtual void comment(int fromline, int toline);
	virtual void uncomment(int fromline, int toline);
};

#endif
