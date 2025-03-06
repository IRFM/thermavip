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

#include "VipCommandOptions.h"

#include <QDir>
#include <QIODevice>
#include <QTextStream>
#include <QtDebug>

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#define ENDL Qt::endl
#else
#define ENDL endl
#endif

static const char* qxt_qt_options[] = { "=style",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the application GUI style"),
					"=stylesheet",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the application stylesheet"),
					"=session",
					QT_TRANSLATE_NOOP("VipCommandOptions", "restores the application from an earlier session"),
					"widgetcount",
					QT_TRANSLATE_NOOP("VipCommandOptions", "displays debugging information about widgets"),
					"reverse",
					QT_TRANSLATE_NOOP("VipCommandOptions", "use right-to-left layout"),
#ifdef QT_DEBUG
					"nograb",
					QT_TRANSLATE_NOOP("VipCommandOptions", "never grab the mouse or keyboard"),
#endif
#if defined(QT_DEBUG) && defined(Q_WS_X11)
					"dograb",
					QT_TRANSLATE_NOOP("VipCommandOptions", "grab the mouse/keyboard even in a debugger"),
					"sync",
					QT_TRANSLATE_NOOP("VipCommandOptions", "run in synchronous mode for debugging"),
#endif
#ifdef Q_WS_WIN
					"direct3d",
					QT_TRANSLATE_NOOP("VipCommandOptions", "use Direct3D by default"),
#endif
#ifdef Q_WS_X11
					"=display",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the X11 display"),
					"=geometry",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the geometry of the first window"),
					"=font",
					"",
					"=fn",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the default font"),
					"=background",
					"",
					"=bg",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the default background color"),
					"=foreground",
					"",
					"=fg",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the default foreground color"),
					"=button",
					"",
					"=btn",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the default button color"),
					"=name",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the application name"),
					"=title",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the application title"),
					"=visual",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the X11 visual type"),
					"=ncols",
					QT_TRANSLATE_NOOP("VipCommandOptions", "limit the number of colors on an 8-bit display"),
					"cmap",
					QT_TRANSLATE_NOOP("VipCommandOptions", "use a private color map"),
					"=im",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the input method server"),
					"noxim",
					QT_TRANSLATE_NOOP("VipCommandOptions", "disable the X VipInput Method"),
					"=inputstyle",
					QT_TRANSLATE_NOOP("VipCommandOptions", "sets the style used by the input method"),
#endif
					0,
					0 };

// This function is used to check to see if a parameter
// is used by Qt.
static int isQtOption(const QString& param)
{
	// Qt options all start with a single dash regardless of platform
	if (param.length() < 2)
		return 0;
	if (param[0] != '-')
		return 0;
	if (param[1] == '-')
		return 0;
#ifdef Q_OS_MAC
	if (param.left(5) == "-psn_")
		return 1;
#endif
	QString name = param.mid(1), value;
	// bool hasEquals;

	// Separate the option and the value, if present
	if (name.indexOf('=') != -1) {
		value = param.section('=', 1);
		name = param.section('=', 0, 0);
		// hasEquals = true;
	}
	else {
		value = "";
		// hasEquals = false;
	}

	const char* option;
	bool optionHasValue;
	for (int i = 0; qxt_qt_options[i]; i += 2) {
		option = qxt_qt_options[i];
		// In the table above, options that require parameters start with =
		if (option[0] == '=') {
			optionHasValue = true;
			option = option + 1; // pointer math to skip =
		}
		else {
			optionHasValue = false;
		}

		// The return value indicates how many parameters to skip
		if (name == option) {
			if (optionHasValue)
				return 2;
			return 1;
		}
	}

	return 0;
}

// Storage structure for option data
struct CommandOption
{
	QStringList names;			 // aliases accepted at the command line
	QString canonicalName;			 // name used for alias()/count()/value()
	QString desc;				 // documentation string
	QStringList values;			 // values passed on command line
	VipCommandOptions::ParamTypes paramType; // flags
	quint16 group;				 // mutual exclusion group
};

class CommandOptionsPrivate
{
	Q_DECLARE_TR_FUNCTIONS(VipCommandOptions)
public:
	QList<CommandOption> options;
	QHash<QString, CommandOption*> lookup;	  // cache structure to simplify processing
	QHash<int, QList<CommandOption*>> groups; // cache structure to simplify processing
	VipCommandOptions::FlagStyle flagStyle;
	VipCommandOptions::ParamStyle paramStyle;
	QStringList positional;	   // prefixless parameters
	QStringList unrecognized;  // prefixed parameters not in recognized options
	QStringList missingParams; // parameters with required values missing
	int screenWidth;
	bool parsed;

	CommandOption* findOption(const QString& name);
	const CommandOption* findOption(const QString& name) const;
	void setOption(CommandOption* option, const QString& value = QString());
	void parse(const QStringList& params);
};

// Looks up an option in qxt_d().options by canonical name
CommandOption* CommandOptionsPrivate::findOption(const QString& name)
{
	// The backwards loop will find what we're looking for more quickly in the
	// typical use case, where you add aliases immediately after adding the option.
	for (int i = options.count() - 1; i >= 0; --i) {
		if (options[i].canonicalName == name)
			return &options[i];
	}
	qWarning() << qPrintable(QString("VipCommandOptions: ") + tr("option \"%1\" not found").arg(name));
	return 0;
}

// Looks up an option in qxt_d().options by canonical name
// This is a const overload for const functions
const CommandOption* CommandOptionsPrivate::findOption(const QString& name) const
{
	// The backwards loop will find what we're looking for more quickly in the
	// typical use case, where you add aliases immediately after adding the option.
	for (int i = options.count() - 1; i >= 0; --i) {
		if (options[i].canonicalName == name)
			return &(options.at(i));
	}
	// qWarning() << qPrintable(QString("VipCommandOptions: ") + tr("option \"%1\" not found").arg(name));
	return 0;
}

VipCommandOptions::VipCommandOptions()
  : m_private(new CommandOptionsPrivate())
{
	qxt_d().screenWidth = 80;
	qxt_d().parsed = false;
#ifdef Q_OS_WIN
	setFlagStyle(Slash);
	setParamStyle(Equals);
#else
	setFlagStyle(DoubleDash);
	setParamStyle(SpaceAndEquals);
#endif
}

VipCommandOptions& VipCommandOptions::instance()
{
	static VipCommandOptions instance;
	instance.setFlagStyle(DoubleDash);
	return instance;
}

// add the help entry
bool register_help = VipCommandOptions::instance().add("help", "show all available options");
bool register_help_alias = VipCommandOptions::instance().alias("help", "h");

VipCommandOptions::~VipCommandOptions()
{
	delete m_private;
}

CommandOptionsPrivate& VipCommandOptions::qxt_d()
{
	return *m_private;
}

const CommandOptionsPrivate& VipCommandOptions::qxt_d() const
{
	return *m_private;
}

void VipCommandOptions::setFlagStyle(FlagStyle style)
{
	qxt_d().flagStyle = style;
}

VipCommandOptions::FlagStyle VipCommandOptions::flagStyle() const
{
	return qxt_d().flagStyle;
}

void VipCommandOptions::setParamStyle(ParamStyle style)
{
	qxt_d().paramStyle = style;
}

VipCommandOptions::ParamStyle VipCommandOptions::paramStyle() const
{
	return qxt_d().paramStyle;
}

void VipCommandOptions::setScreenWidth(quint16 width)
{
	qxt_d().screenWidth = width;
}

quint16 VipCommandOptions::screenWidth() const
{
	return qxt_d().screenWidth;
}

bool VipCommandOptions::addSection(const QString& name)
{
	CommandOption option;
	option.canonicalName.clear();
	option.desc = name;
	qxt_d().options.append(option);
	return true;
}

bool VipCommandOptions::add(const QString& name, const QString& desc, ParamTypes paramType, int group)
{
	CommandOption option;
	option.canonicalName = name;
	option.desc = desc;
	option.paramType = paramType;
	option.group = group;
	qxt_d().options.append(option);
	if (group != -1)
		qxt_d().groups[group].append(&(qxt_d().options.last()));
	// Connect the canonical name to a usable name
	return alias(name, name);
}

bool VipCommandOptions::alias(const QString& from, const QString& to)
{
	CommandOption* option = qxt_d().findOption(from);
	if (!option)
		return false; // findOption outputs the warning
	option->names.append(to);
	qxt_d().lookup[to] = option;
	if (option->paramType & ValueOptional && qxt_d().flagStyle == DoubleDash && to.length() == 1)
		qWarning() << qPrintable(QString("VipCommandOptions: ") + tr("Short options cannot have optional parameters"));
	return true;
}

QStringList VipCommandOptions::positional() const
{
	if (!qxt_d().parsed)
		qWarning() << qPrintable(QString("VipCommandOptions: ") + tr("positional() called before parse()"));
	return qxt_d().positional;
}

QStringList VipCommandOptions::unrecognized() const
{
	if (!qxt_d().parsed)
		qWarning() << qPrintable(QString("VipCommandOptions: ") + tr("unrecognized() called before parse()"));
	return qxt_d().unrecognized + qxt_d().missingParams;
}

int VipCommandOptions::count(const QString& name) const
{
	if (!qxt_d().parsed)
		qWarning() << qPrintable(QString("VipCommandOptions: ") + tr("count() called before parse()"));
	const CommandOption* option = qxt_d().findOption(name);
	if (!option)
		return 0; // findOption outputs the warning
	return option->values.count();
}

QVariant VipCommandOptions::value(const QString& name) const
{
	if (!qxt_d().parsed)
		qWarning() << qPrintable(QString("VipCommandOptions: ") + tr("value() called before parse()"));
	const CommandOption* option = qxt_d().findOption(name);
	if (!option)
		return QVariant(); // findOption outputs the warning
	int ct = option->values.count();
	if (ct == 0)
		return QVariant();
	if (ct == 1)
		return option->values.first();
	return option->values;
}

QMultiHash<QString, QVariant> VipCommandOptions::parameters() const
{
	if (!qxt_d().parsed)
		qWarning() << qPrintable(QString("VipCommandOptions: ") + tr("parameters() called before parse()"));
	QMultiHash<QString, QVariant> params;
	int ct;
	foreach (const CommandOption& option, qxt_d().options) {
		ct = option.values.count();
		if (!ct) {
			continue;
		}
		else if (!(option.paramType & (ValueOptional | ValueRequired))) {
			// Valueless options are really a true/false flag
			params.insert(option.canonicalName, true);
		}
		else {
			for (const QString& value : option.values)
				params.insert(option.canonicalName,QVariant::fromValue( value));
		}
	}
	return params;
}

void VipCommandOptions::parse(int argc, char** argv)
{
	QStringList args;
	for (int i = 0; i < argc; i++)
		args << argv[i];
	parse(args);
}

void VipCommandOptions::parse(QStringList params)
{
	qxt_d().parse(params);
	qxt_d().parsed = true;
}

// Update the internal data structures with an option from the command line.
void CommandOptionsPrivate::setOption(CommandOption* option, const QString& value)
{
	if (groups.contains(option->group)) {
		// Clear mutually-exclusive options
		QList<CommandOption*>& others = groups[option->group];
		for (CommandOption* other : others) {
			if (other != option)
				other->values.clear();
		}
	}
	// Clear all previous values if multiples are not accepted
	if (!(option->paramType & VipCommandOptions::AllowMultiple))
		option->values.clear();
	option->values.append(value);
}

// Do the work of parsing the command line
void CommandOptionsPrivate::parse(const QStringList& params)
{
	int pos = 1; // 0 is the application name
	int ct = params.count();
	int skip = 0;
	bool endFlags = false;
	// bool notUnrecognized =false;
	bool hasEquals = false;
	QString name, param, value;
	// add by victor:
	positional.clear();
	unrecognized.clear();
	missingParams.clear();

	while (pos < ct) {
		// Ignore Qt built-in options
		while ((skip = isQtOption(params[pos]))) {
			pos += skip;
			if (pos >= ct)
				return;
		}

		param = params[pos];
		pos++;

		if (!endFlags && ((flagStyle == VipCommandOptions::Slash && param[0] == '/') || (flagStyle != VipCommandOptions::Slash && param[0] == '-'))) {
			// tagged argument
			if (param.length() == 1) {
				// "-" or "/" alone can't possibly match a flag, so use positional.
				positional.append(param);
				continue;
			}
			// notUnrecognized = false;

			if (flagStyle != VipCommandOptions::Slash && param == "--") {
				// End of parameters flag
				endFlags = true;
			}
			else if (flagStyle == VipCommandOptions::DoubleDash && param[1] != '-') {
				// Handle short-form options
				int len = param.length();
				CommandOption* option;
				for (int i = 1; i < len; i++) {
					QString ch(param[i]);
					if (ch == "-") {
						endFlags = true;
					}
					else {
						option = lookup.value(ch, 0);
						if (!option) {
							// single-letter flag has no known equivalent
							unrecognized.append(QString("-") + param[i]);
						}
						else {
							if (option->paramType & VipCommandOptions::ValueRequired) {
								// Check for required parameters
								// Short options can't have optional parameters
								if (pos >= params.count()) {
									missingParams.append(param);
									break;
								}
								value = params[pos];
							}
							else {
								value = "";
							}
							setOption(option, value);
						}
					}
				}
			}
			else {
				// Break apart a value
				if (param.indexOf('=') != -1) {
					value = param.section('=', 1);
					param = param.section('=', 0, 0);
					hasEquals = true;
				}
				else {
					value = "";
					hasEquals = false;
				}

				if (flagStyle == VipCommandOptions::DoubleDash)
					name = param.mid(2);
				else
					name = param.mid(1);

				CommandOption* option = lookup.value(name, 0);
				if (!option) {
					unrecognized.append(param);
				}
				else {
					if (option->paramType & VipCommandOptions::ValueRequired && !hasEquals) {
						// Check for parameters
						if (pos >= params.count()) {
							missingParams.append(param);
							break;
						}
						value = params[pos];
						pos++;
					}
					else if ((paramStyle & VipCommandOptions::Space) && (option->paramType & VipCommandOptions::ValueOptional) && !hasEquals) {
						if (pos < params.count()) {
							if (!((flagStyle == VipCommandOptions::Slash && params.at(pos)[0] == '/') ||
							      (flagStyle != VipCommandOptions::Slash && params.at(pos)[0] == '-'))) {
								value = params[pos];
								pos++;
							}
						}
					}

					setOption(option, value);
				}
			}
		}
		else {
			// positional argument
			positional.append(param);
		}
	}
}

bool VipCommandOptions::showUnrecognizedWarning(QIODevice* device) const
{
	if (!device) {
		QTextStream stream(stderr);
		return showUnrecognizedWarning(*&stream);
	}
	else {
		QTextStream stream(device);
		return showUnrecognizedWarning(*&stream);
	}
}

QString VipCommandOptions::getUnrecognizedWarning() const
{
	QString usage;
	QTextStream stream(&usage);
	showUnrecognizedWarning(*&stream);
	return usage;
}

bool VipCommandOptions::showUnrecognizedWarning(QTextStream& stream) const
{
	if (!qxt_d().unrecognized.count() && !qxt_d().missingParams.count())
		return false;

	QString name;
	if (QCoreApplication::instance())
		name = QDir(QCoreApplication::applicationFilePath()).dirName();
	if (name.isEmpty())
		name = "VipCommandOptions";

	if (qxt_d().unrecognized.count())
		stream << name << ": " << tr("unrecognized parameters: ") << qxt_d().unrecognized.join(" ") << ENDL;

	for (const QString& param : qxt_d().missingParams)
		stream << name << ": " << tr("%1 requires a parameter").arg(param) << ENDL;

	return true;
}

void VipCommandOptions::showUsage(bool showQtOptions, QIODevice* device) const
{
	if (!device) {
		QTextStream stream(stdout);
		showUsage(showQtOptions, *&stream);
	}
	else {
		QTextStream stream(device);
		showUsage(showQtOptions, *&stream);
	}
}

QString VipCommandOptions::getUsage(bool showQtOptions) const
{
	QString usage;
	QTextStream stream(&usage);
	showUsage(showQtOptions, *&stream);
	return usage;
}

void VipCommandOptions::showUsage(bool showQtOptions, QTextStream& stream) const
{
	QStringList names;
	QStringList descs;
	int maxNameLength = 0;
	QString name;

	for (const CommandOption& option : qxt_d().options) {
		// Don't generate usage for undocumented parameters
		if (option.paramType & Undocumented)
			continue;

		for (const QString& n : option.names) {
			if (name.length())
				name += ", ";
			if (qxt_d().flagStyle == Slash)
				name += '/';
			else if (qxt_d().flagStyle == DoubleDash && n.length() > 1)
				name += "--";
			else
				name += '-';
			name += n;
			if (option.paramType & (ValueOptional | ValueRequired)) {
				if (option.paramType & ValueOptional)
					name += "[=x]";
				else if (qxt_d().paramStyle == SpaceAndEquals)
					name += "[=]x";
				else if (qxt_d().paramStyle == Equals)
					name += "=x";
				else
					name += " x";
			}
		}

		// The maximum name length is used for formatting the output columns
		if (name.length() > maxNameLength)
			maxNameLength = name.length();
		names.append(name);
		descs.append(option.desc);
		name = "";
	}

	if (showQtOptions) {
		// Add a section header
		names.append(QString());
		descs.append("Common Qt Options");

		// Parse through qxt_qt_options
		const char* option;
		bool optionHasValue;
		for (int i = 0; qxt_qt_options[i]; i += 2) {
			option = qxt_qt_options[i];

			if (option[0] == '=') {
				// The option takes a parameter
				optionHasValue = true;
				option = option + 1; // pointer math to skip the =
			}
			else {
				optionHasValue = false;
			}

			// Concatenate on the option alias
			if (!name.isEmpty())
				name += ", ";
			name += '-';
			name += option;
			if (optionHasValue)
				name += "[=]x";

			if (qxt_qt_options[i + 1][0] != 0) {
				// The last alias for the option has the description
				if (name.length() > maxNameLength)
					maxNameLength = name.length();
				names.append(name);
				descs.append(qxt_qt_options[i + 1]);
				name = "";
			}
		}
	}

	int ct = names.count();
	QString line, wrap(maxNameLength + 3, ' ');
	for (int i = 0; i < ct; i++) {
		if (names[i].isEmpty()) {
			// Section headers have no name entry
			stream << ENDL << descs[i] << ":" << ENDL;
			continue;
		}
		line = ' ' + names[i] + QString(maxNameLength - names[i].length() + 2, ' ');

		for (const QString& word : descs[i].split(' ', VIP_SKIP_BEHAVIOR::SkipEmptyParts)) {
			if (qxt_d().screenWidth > 0 && line.length() + word.length() >= qxt_d().screenWidth) {
				stream << line << ENDL;
				line = wrap;
			}
			line += word + ' ';
		}
		stream << line << ENDL;
	}
}
