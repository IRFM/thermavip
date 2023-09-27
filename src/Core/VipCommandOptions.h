#ifndef VIP_COMMANDOPTIONS_H
#define VIP_COMMANDOPTIONS_H

#include <QCoreApplication> // for Q_DECLARE_TR_FUNCTIONS
#include <QFlags>
#include <QMultiHash>
#include <QStringList>
#include <QVariant>

#include "VipConfig.h"

/// \addtogroup Core
/// @{

class CommandOptionsPrivate;
class QTextStream;
class QIODevice;

/// \class VipCommandOptions
///
/// \brief The VipCommandOptions class is a Parser for command-line options
///
/// It is provided for Qt < 6 versions, where QCommandLineParser is not available.
///
/// This class is used by applications that need to accept command-line arguments.  It can
/// also automatically generate help text, which keeps it from accidentally falling out of
/// sync with the supported options, and it can produce warnings for common errors.
///
/// It accepts Windows-style ("/option"), UNIX-style ("-option"), and GNU-style
/// ("--option") options. By default, VipCommandOptions uses Windows-style options
/// (VipCommandOptions::Slash) on Windows and GNU-style options (VipCommandOptions::DoubleDash)
/// on UNIX and Mac. When using GNU-style options, single-character option names only
/// require a single leading dash and can be grouped together, for example, "-abc".
///
/// VipAny parameter that does not start with the option prefix is considered a positional
/// parameter. Most applications treat positional parameters as filenames. When using
/// GNU- or UNIX-style options, use a double dash ("--") alone to force the remaining
/// parameters to be interpreted as positional parameters.
///
/// To use VipCommandOptions, first add the supported options using the add() and alias()
/// methods, then process the command line using the parse() method, and finally retrieve
/// the processed options using the positional(), count(), value() and/or parameters()
/// methods.
///
/// Mutually-exclusive options can be specified by using the \a group parameter to add().
/// Only one option in a group will be accepted on the command line; if multiple options
/// are provided, only the last one takes effect.
///
/// Some options may accept an optional or required parameter. Depending on the value
/// of the paramStyle() property, the parameter may be separated from the option by
/// an equals sign ("/option=value") or by a space ("-option value"). By default,
/// Windows uses an equals sign and UNIX and Mac accept both an equals sign and a
/// space. Optional parameters always require an equals sign. Note that, when using
/// GNU-style options, single-character options cannot have optional parameters.
///
/// A simple archiving application might use code similar to this:
/// \code
/// VipCommandOptions options;
/// options.add("compress", "create a new archive");
/// options.alias("compress", "c");
/// options.add("extract", "extract files from an archive");
/// options.alias("extract", "x");
/// options.add("level", "set the compression level (0-9)", VipCommandOptions::Required);
/// options.alias("level", "l");
/// options.add("verbose", "show more information about the process; specify twice for more detail", VipCommandOptions::AllowMultiple);
/// options.alias("verbose", "v");
/// options.add("help", "show this help text");
/// options.alias("help", "h");
/// options.parse(QCoreApplication::arguments());
/// if(options.count("help") || options.showUnrecognizedWarning()) {
/// options.showUsage();
/// return -1;
/// }
/// bool verbose = options.count("verbose");
/// int level = 5;
/// if(options.count("level")) {
/// level = options.value("level").toInt();
/// }
/// \endcode
///
/// \sa QCoreApplication::arguments()
class VIP_CORE_EXPORT VipCommandOptions
{
	Q_DECLARE_TR_FUNCTIONS(VipCommandOptions)

	/// Constructs a VipCommandOptions object.
	VipCommandOptions();

public:
	/// \enum VipCommandOptions::FlagStyle
	/// This enum type defines which type of option prefix is used.
	/// Slash is the default on Windows.
	/// DoubleDash is the default on all other platforms.
	enum FlagStyle
	{
		DoubleDash, ///< Two dashes (GNU-style)
		SingleDash, ///< One dash (UNIX-style)
		Slash	    ///< Forward slash (Windows-style)
	};
	/// \enum VipCommandOptions::ParamStyle
	/// This enum type defines what syntax is used for options that
	/// require parameters. Equals is the default on Windows.
	/// SpaceAndEquals is the default on all other platforms.
	enum ParamStyle
	{
		Space = 1,	   ///< Space ("-option value")
		Equals = 2,	   ///< Equals sign ("/option=value")
		SpaceAndEquals = 3 ///< Accept either
	};
	/// \enum VipCommandOptions::ParamType
	/// \flags VipCommandOptions::ParamTypes
	/// This enum type is used to specify flags that control the
	/// interpretation of an option.
	///
	/// The ParamTypes type is a typedef for QFlags<ParamType>. It stores
	/// an OR combination of ParamType values.
	enum ParamType
	{
		NoValue = 0,		  ///< The option does not accept a value.
		ValueOptional = 1,	  ///< The option may accept a value.
		ValueRequired = 2,	  ///< The option requires a value.
		Optional = ValueOptional, ///< The option may accept a value. Deprecated in favor of ValueOptional.
		Required = ValueRequired, ///< The option requires a value. Deprecated in favor of ValueRequired.
		AllowMultiple = 4,	  ///< The option may be passed multiple times.
		Undocumented = 8	  ///< The option is not output in the help text.
	};
	Q_DECLARE_FLAGS(ParamTypes, ParamType)

	/// Returns the application global VipCommandOptions in GNU style.
	static VipCommandOptions& instance();

	~VipCommandOptions();

	/// Sets which prefix is used to identify options. The default value is Slash on Windows
	/// and DoubleDash on all other platforms.
	///
	/// Note that Qt's built-in options (see QApplication) always use a single dash,
	/// regardless of this setting.
	void setFlagStyle(FlagStyle style);

	/// Gets which prefix is used to identify options.
	FlagStyle flagStyle() const;

	/// Sets which value separator is used for options that accept parameters.
	/// The default value is Equals on Windows and SpaceAndEquals on all other
	/// platforms.
	///
	/// Single-letter options with optional parameters in DoubleDash mode
	/// always use an equals sign, regardless of this setting.
	///
	/// Qt's built-in options always behave as SpaceAndEquals, regardless of
	/// this setting.
	void setParamStyle(ParamStyle style);

	/// Sets which value separator is used for options that accept parameters.
	ParamStyle paramStyle() const;

	/// Sets the width of the screen, in characters. This is used for word-wrapping
	/// the automatically-generated help text to the size of the screen. The default
	/// value is 80 characters. Pass 0 to disable word-wrapping.
	void setScreenWidth(quint16 width);

	/// Gets the width of the screen, in characters.
	quint16 screenWidth() const;

	/// Adds a section separator. Section separators are only used in generating
	/// help text, and can be used to visually separate related groups of
	/// options.
	bool addSection(const QString& name);

	/// Adds an option to the parser.
	///
	/// The name parameter defines the name that will be used by the alias(),
	/// count(), value(), and parameters() methods. Additional names for the
	/// same option can be defined using the alias() method.
	///
	/// The group parameter, if used, defines a set of mutually-exclusive options.
	/// If more than one option in the same group is passed on the command line,
	/// only the last one takes effect.
	bool add(const QString& name, const QString& desc = QString(), ParamTypes paramType = NoValue, int group = -1);

	/// Provides an alias for an option. An alias is another name for the option that can be
	/// given on the command line. Aliases cannot be used as parameters to alias(), count()
	/// or value() nor can single-letter aliases be created for options with an optional value.
	///
	/// The from parameter must be a name has previously been added with the add() method.
	bool alias(const QString& from, const QString& to);

	/// Returns the positional parameters from the command line, that is, the arguments that
	/// do not begin with the option prefix.
	///
	/// \sa flagStyle()
	QStringList positional() const;

	/// Returns the options that could not be parsed.
	///
	/// An argument is unrecognized if it begins with the option prefix but was never
	/// defined using the add() or alias() methods, or if it requires a value but the
	/// user did not provide one.
	QStringList unrecognized() const;

	/// Returns the number of times an option was passed on the command line.
	///
	/// This function will only return 0 or 1 for options that were not created with the
	/// VipCommandOptions::AllowMultiple flag set.
	int count(const QString& name) const;

	/// Returns the value or values for an option passed on the command line.
	///
	/// If the option was not passed on the command line, this function returns a null QVariant.
	/// If the option was created with the VipCommandOptions::AllowMultiple flag, and the option
	/// was passed more than once, this function returns a QStringList containing the values.
	/// Otherwise, this function returns the last (or only) value given to the option on the
	/// command line.  When an option allowing an optional value is provided on the command
	/// line and for which no value is provided, an empty but non-null QString will be returned
	/// in the QVariant.
	QVariant value(const QString& name) const;

	/// Returns all of the recognized options passed on the command line.
	QMultiHash<QString, QVariant> parameters() const;

	/// This is an overloaded member function, provided for convenience.
	///
	/// Process a set of command-line options. This overload accepts a number of
	/// arguments and a pointer to the list of arguments.
	///
	/// Note that parse() may be invoked multiple times to handle arguments from
	/// more than one source.
	void parse(int argc, char** argv);

	/// Process a set of command-line options. This overload accepts a QStringList
	/// containing the command-line options, such as the one returned by
	/// QCoreApplication::arguments().
	///
	/// Note that parse() may be invoked multiple times to handle arguments from
	/// more than one source.
	///
	/// \sa QCoreApplication::arguments()
	void parse(QStringList params);

	/// This is an overloaded member function, provided for convenience.
	///
	/// Outputs automatically-generated usage text for the accepted options to the provided
	/// device, or standard error by default. The device must already be opened for writing.
	///
	/// Pass true to showQtOptions to output usage text for the options recognized by
	/// QApplication.
	///
	/// \sa QApplication
	void showUsage(bool showQtOptions = false, QIODevice* device = 0) const;

	/// This is an overloaded member function, provided for convenience.
	///
	/// Outputs automatically-generated usage text for the accepted options to the provided
	/// stream.
	///
	/// Pass true to showQtOptions to output usage text for the options recognized by
	/// QApplication.
	///
	/// \sa QApplication
	void showUsage(bool showQtOptions, QTextStream& stream) const;

	/// Returns the automatically-generated usage text for the accepted options.
	QString getUsage(bool showQtOptions = false) const;

	/// This is an overloaded member function, provided for convenience.
	///
	/// Outputs a warning about any unrecognized options to the provided device, or
	/// standard error by default. The device must already be opened for writing.
	///
	/// This function returns true if any warnings were output, or false otherwise.
	///
	/// If a QCoreApplication or a subclass of QCoreApplication has been instantiated,
	/// this function uses QCoreApplication::applicationFilePath() to get the name
	/// of the executable to include in the message.
	///
	/// \sa QCoreApplication::applicationFilePath()
	bool showUnrecognizedWarning(QIODevice* device = 0) const;

	/// This is an overloaded member function, provided for convenience.
	///
	/// Outputs a warning about any unrecognized options to the provided stream.
	///
	/// This function returns true if any warnings were output, or false otherwise.
	///
	/// If a QCoreApplication or a subclass of QCoreApplication has been instantiated,
	/// this function uses QCoreApplication::applicationFilePath() to get the name
	/// of the executable to include in the message.
	///
	/// \sa QCoreApplication::applicationFilePath()
	bool showUnrecognizedWarning(QTextStream& stream) const;

	/// Returns the automatically-generated warning text about any unrecognized options.
	///
	/// If a QCoreApplication or a subclass of QCoreApplication has been instantiated,
	/// this function uses QCoreApplication::applicationFilePath() to get the name
	/// of the executable to include in the message.
	///
	/// \sa QCoreApplication::applicationFilePath()
	QString getUnrecognizedWarning() const;

private:
	CommandOptionsPrivate* m_private;
	CommandOptionsPrivate& qxt_d();
	const CommandOptionsPrivate& qxt_d() const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipCommandOptions::ParamTypes)

/// @}
// end Core

#endif // COMMANDOPTIONS_H
