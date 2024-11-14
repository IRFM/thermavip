//include python first
#ifdef _DEBUG
#undef _DEBUG
#define VIP_HAS_DEBUG
extern "C" {
#include "Python.h"
}
#else
extern "C" {
#include "Python.h"
}
#endif

#ifdef VIP_HAS_DEBUG
#define _DEBUG
#ifdef _MSC_VER
// Bug https://github.com/microsoft/onnxruntime/issues/9735
#define _STL_CRT_SECURE_INVALID_PARAMETER(expr) _CRT_SECURE_INVALID_PARAMETER(expr)
#endif
#endif


#include "VipDragWidget.h"
#include "VipLogging.h"

#include "CustomizePlayer.h"
#include "PyOperation.h"
#include "IOOperationWidget.h"

class CustomizePlayer::PrivateData
{
public:
	QPointer<VipAbstractPlayer> player;
	QToolButton * button;
};

CustomizePlayer::CustomizePlayer(VipAbstractPlayer * player)
	:QObject(player)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->player = player;

	QMenu * menu = new QMenu();
	bool found = false;
	buildScriptsMenu(menu, &found);
	if (!found) {
		delete menu;
		return;
	}

	if (VipPlayer2D * pl = player2D()) {
		d_data->button = new QToolButton();
		d_data->button->setIcon(vipIcon("python.png"));
		d_data->button->setToolTip("Apply Python script for this player");
		d_data->button->setAutoRaise(true);
		d_data->button->setMenu(menu);
		d_data->button->setPopupMode(QToolButton::InstantPopup);
		pl->toolBar()->addWidget(d_data->button);
		connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(scriptSelected(QAction *)));
	}
}
CustomizePlayer::~CustomizePlayer()
{
}

VipAbstractPlayer * CustomizePlayer::player() const
{
	return d_data->player;
}




static QAction * findAction(QMenu * menu, const QString & name) {
	const QList<QAction*> acts = menu->actions();
	for (int i = 0; i < acts.size(); ++i)
		if (acts[i]->text() == name) return acts[i];
	return menu->addAction(name);;
}
static QAction *createAction(QMenu * menu, const QString & name, bool *found)
{
	QStringList lst = name.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	if (name.isEmpty() || lst.size() == 0)
		return nullptr;

	QAction * tmp = nullptr;
	if (lst.size() == 1) {
		QAction * tmp = menu->addAction(lst[0]);
		tmp->setProperty("path", name);
		if (found) *found = true;
	}
	else {
		QFont bold = menu->font();
		bold.setBold(true);

		QAction * start = findAction(menu, lst[0]);
		start->setFont(bold);
		start->setMenu(new QMenu());

		for (int i = 1; i < lst.size() - 1; ++i) {
			start = findAction(start->menu(), lst[i]);
			start->setFont(bold);
			start->setMenu(new QMenu());
		}
		tmp = start->menu()->addAction(lst.last());
		tmp->setProperty("path", name);
		if (found) *found = true;
	}
	return tmp;
}

void CustomizePlayer::buildScriptsMenu(QMenu * menu, bool * found)
{
	if (found) *found = false;
	//build menu from the content of the 'Emissivity' folder
	menu->clear();
	QStringList lst = vipGetPythonPlayerScripts();

	for (int i = 0; i < lst.size(); ++i) {
		createAction(menu, lst[i], found);
	}
}

void CustomizePlayer::scriptSelected(QAction * act)
{
	if (!d_data->player)
		return;

	VipBaseDragWidget * w = VipBaseDragWidget::fromChild(d_data->player);
	if (!w)
		return;

	int id = VipUniqueId::id(w);
	QString path = vipGetPythonScriptsPlayerDirectory() + act->property("path").toString();
	QFileInfo info(path);
	if (!info.exists())
		return;

	VipGILLocker lock;
	PyErr_Clear();
	//import module and launch apply(player_id) in the main thread

	QString module = info.baseName();
	QString mpath = info.canonicalPath();
	QString code =
		"try:\n"
		"  import sys; sys.path.find('" + mpath + "')\n"
		"except:\n"
		"  sys.path.append('" + mpath + "')\n"
		"try:\n"
		"  import importlib;importlib.reload(" + module + ")\n"
		"except:\n"
		"  import " + module + "\n" +
		module + ".apply(" + QString::number(id) + ")";


	int r = PyRun_SimpleString(code.toLatin1().data());
	if (r != 0) {
		VipPyError err(compute_error_t{});
		if (!err.traceback.isEmpty()) {
			vip_debug("err: %s\n", err.traceback.toLatin1().data());
			VIP_LOG_ERROR(err.traceback);
		}
		QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
	}
}
 


void customizePlayer(VipAbstractPlayer * player)
{
	bool has_c = player->property("_PyCustomizePlayer").toBool();
	if (has_c)
		return;
	new CustomizePlayer(player);
	player->setProperty("_PyCustomizePlayer", true);
}
