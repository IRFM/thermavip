#ifndef VIS_GUI_H
#define VIS_GUI_H

#include "VipArchive.h"
#include "VipFunctional.h"
#include "VipPlotItem.h"
#include "VipPlotCurve.h"
#include "VipSymbol.h"
#include "VipPlotHistogram.h"
#include "VipPlotGrid.h"
#include "VipPlotMarker.h"
#include "VipPlotRasterData.h"
#include "VipPlotSpectrogram.h"
#include "VipPlotShape.h"
#include "VipAxisBase.h"
#include "VipAxisColorMap.h"
#include "VipColorMap.h"
#include "VipPlotWidget2D.h"
///\defgroup Gui Gui
///
///
///

/// \addtogroup Gui
/// @{

VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE(VipPlotItem*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotItemData*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotCurve*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotHistogram*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotGrid*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotCanvas*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotMarker*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotRasterData*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotSpectrogram*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotShape*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotSceneModel*);
VIP_REGISTER_QOBJECT_METATYPE(VipAbstractScale*);
VIP_REGISTER_QOBJECT_METATYPE(VipAxisBase*);
VIP_REGISTER_QOBJECT_METATYPE(VipAxisColorMap*);
VIP_REGISTER_QOBJECT_METATYPE(VipColorMap*);
VIP_REGISTER_QOBJECT_METATYPE(VipLinearColorMap*);
VIP_REGISTER_QOBJECT_METATYPE(VipAlphaColorMap*);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotItem * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotItem * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotItemData * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotItemData * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotCurve * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotCurve * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotHistogram * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotHistogram * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotGrid * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotGrid * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotCanvas * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotCanvas * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotMarker * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotMarker * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotRasterData * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotRasterData * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotSpectrogram * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotSpectrogram * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotShape * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotShape * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotSceneModel * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotSceneModel * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipAbstractScale * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipAbstractScale * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipAxisBase * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipAxisBase * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipColorMap * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipColorMap * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipLinearColorMap * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipLinearColorMap * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipAlphaColorMap * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipAlphaColorMap * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipAxisColorMap * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipAxisColorMap * value);

VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotArea2D * value);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotArea2D * value);

/// Returns a copy of given item.
/// This function uses the serialize/deserialize mechanism to produce a copy of input VipPlotItem.
/// You should always use this function to copy an item, as it will take care of internal IDs used to identify each item.
/// Note that the output item will NOT share the input item axes and will have a different ID (as in #VipUniqueId::id()).
VIP_GUI_EXPORT VipPlotItem * vipCopyPlotItem(const VipPlotItem* item);

/// Save the current item state, except its ID (as in #VipUniqueId::id()) and its axises.
VIP_GUI_EXPORT QByteArray vipSavePlotItemState(const VipPlotItem* item);
/// Restore an item state previously saved with #vipSavePlotItemState.
VIP_GUI_EXPORT bool vipRestorePlotItemState(VipPlotItem* item, const QByteArray & state);

/// Returns the standard foreground text brush for given widget.
/// The text color for all widgets is usually black. However, this might be changed by manually setting the palette or
/// setting a widget or application wide style sheet.
/// This function takes care of these cases to return the proper text color.
///
/// If \a w is NULL, the function uses the application palette.
VIP_GUI_EXPORT QBrush vipWidgetTextBrush(QWidget * w);
/// Returns the default text error color used by QTextEdit derived classes.
/// The default error color is usually red, but it might change depending on the loaded skin.
VIP_GUI_EXPORT QColor vipDefaultTextErrorColor(QWidget * w);

/// Returns available skins
VIP_GUI_EXPORT QStringList vipAvailableSkins();
/// Load a specific skin.
/// A skin is a directory in the \a skin folder of name \a skin_name.
/// It contains at least a file \a stylesheet.css and an optional \a icons folder containing additional icons.
VIP_GUI_EXPORT bool vipLoadSkin(const QString & skin_name);

/// Restart Thermavip after \a delay_ms milliseconds after exiting thermavip.
/// This function only works if viptools is present.
VIP_GUI_EXPORT void vipSetRestartEnabled(int delay_ms);
VIP_GUI_EXPORT void vipDisableRestart();
VIP_GUI_EXPORT bool vipIsRestartEnabled();
VIP_GUI_EXPORT int vipRestartMSecs();

/// Set the global query pulse or date function.
/// This function should display a small dialog box to select a pulse or data time (valid for WEST, ITER and W7-X plugin),
/// and returns the result as a QString that can be used to open device.
/// The first string parameter is the dialog box title.
/// The second string parameter is the default value.
///
/// Currently, this feature is only used by the Python plugin.
VIP_GUI_EXPORT void vipSetQueryFunction(const std::function<QString(const QString &, const QString & )> & fun);
VIP_GUI_EXPORT std::function<QString(const QString &, const QString &)> vipQueryFunction();


/// Helper function that removes the borders of an image while keeping at least border pixels
VIP_GUI_EXPORT QImage vipRemoveColoredBorder(const QImage& img, const QColor& c, int border = 10);
VIP_GUI_EXPORT QPixmap vipRemoveColoredBorder(const QPixmap& pix, const QColor& c, int border = 10);



/// Thermavip use a shared memory called "Thermavip_Files" containing the filenames to open.
/// If you add filenames in this shared memory (serialized with QDataStream), one instance of Thermavip (you don't know which one)
/// will open them using #vipRetrieveFilesToOpen.
///
/// This is used when trying to open multiple files from the command line or from a Windows/Linux directory browser.
/// This class is used to manipulate this shared memory and is intended for internal use only.
class VIP_GUI_EXPORT VipFileSharedMemory
{
	VipFileSharedMemory();
	~VipFileSharedMemory();

	class PrivateData;
	PrivateData * m_data;

public:
	static VipFileSharedMemory & instance();
	bool addFilesToOpen(const QStringList & lst, bool new_workspace = false);
	/// Retrieve the file names to be opened in the shared memory "Thermavip_Files".
	/// These files are set with #VipFileSharedMemory::addFilesToOpen().
	/// This function returns the filenames (if any) and clear the shared memory.
	QStringList retrieveFilesToOpen(bool *new_workspace);
	/// This function tells if there is at least one Thermavip display instance opened
	bool hasThermavipInstance();
};

namespace Vip
{
	enum PlayerLegendPosition
	{
		LegendHidden,
		LegendBottom,
		LegendInnerTopLeft,
		LegendInnerTopRight,
		LegendInnerBottomLeft,
		LegendInnerBottomRight,
	};
}


class VipPlayer2D;
class VipMainWindow;

/// Values set in the property "_vip_customDisplay" of a VipPlotItem
/// to set a custom property (color, text font, ...) that won't be override
/// by the default settings of VipGuiDisplayParamaters.
///
/// #define VIP_CUSTOM_PLOT_PEN 0x001
/// #define VIP_CUSTOM_PLOT_BRUSH 0x002
/// #define VIP_CUSTOM_PLOT_FONT 0x004
/// #define VIP_CUSTOM_PLOT_TEXT_COLOR 0x008
/// #define VIP_CUSTOM_PLOT_SYMBOL 0x01

/// Parameters to control general appearance
class VIP_GUI_EXPORT VipGuiDisplayParamaters : public QObject
{
	Q_OBJECT

	VipGuiDisplayParamaters(VipMainWindow * win);

public:

	/// Rendering strategy for videos and plots
	enum RenderStrategy
	{
		PureOpenGL,			//! use QOpenGLWidget inside a QGraphicsView
		OffscreenOpenGL,	//! use offscreen opengl rendering with standard QWidget
		AutoRendering,		//! try to select the fastest between offscreen opengl and direct rendering
		DirectRendering		//! CPU based rendering (default, usually the fastest)
	};

	static VipGuiDisplayParamaters * instance(VipMainWindow * win = NULL);

	~VipGuiDisplayParamaters();

	int itemPaletteFactor() const;
	bool videoPlayerShowAxes() const;
	Vip::PlayerLegendPosition legendPosition() const;
	VipLinearColorMap::StandardColorMap playerColorScale() const;
	VipPlotArea2D * defaultPlotArea() const;
	VipPlotCurve * defaultCurve() const;
	void applyDefaultPlotArea(VipPlotArea2D * area);
	void applyDefaultCurve(VipPlotCurve * c);
	//VipPlotPlayer with time scale and date time since Epoch
	bool displayTimeOffset() const;
	VipValueToTime::DisplayType displayType() const;
	QFont defaultEditorFont() const;
	bool alwaysShowTimeMarker() const;
	bool globalColorScale() const;
	bool titleVisible() const;
	VipTextStyle titleTextStyle() const;
	VipTextStyle defaultTextStyle() const;
	bool hasTitleTextStyle() const;
	bool hasDefaultTextStyle() const;
	int flatHistogramStrength() const;
	int videoRenderingStrategy() const;
	int plotRenderingStrategy() const;
	int renderingThreads() const;
	bool displayExactPixels() const;
	
	///Returns the ROI border pen
	QPen shapeBorderPen();
	///Returns the ROI background brush
	QBrush shapeBackgroundBrush();
	///Returns the ROI drawn components
	VipPlotShape::DrawComponents shapeDrawComponents();

	QColor defaultPlayerTextColor() const;
	QColor defaultPlayerBackgroundColor() const;

public Q_SLOTS:

	void setItemPaletteFactor(int);
	void setVideoPlayerShowAxes(bool);
	void setLegendPosition(Vip::PlayerLegendPosition pos);
	void setPlayerColorScale(VipLinearColorMap::StandardColorMap);
	void setDisplayTimeOffset(bool);
	void setDisplayType(VipValueToTime::DisplayType);
	void setDefaultEditorFont(const QFont & font);
	void setAlwaysShowTimeMarker(bool);
	void setPlotTitleInside(bool);
	void setPlotGridVisible(bool);
	void setGlobalColorScale(bool);
	void setFlatHistogramStrength(int);
	void setTitleVisible(bool);
	void setTitleTextStyle(const VipTextStyle & );
	void setTitleTextStyle2(const VipText &);
	void setDefaultTextStyle(const VipTextStyle &);
	void setDefaultTextStyle2(const VipText &);
	void setVideoRenderingStrategy(int);
	void setPlotRenderingStrategy(int);
	void setRenderingThreads(int);
	void autoScaleAll();
	void setDisplayExactPixels(bool);

	///Set the default border pen for all existing and future ROI
	void setShapeBorderPen(const QPen & pen);
	///Set the default background brush for all existing and future ROI
	void setShapeBackgroundBrush(const QBrush & brush);
	///Set the default drawn components for all existing and future ROI
	void setShapeDrawComponents(const VipPlotShape::DrawComponents & c);
	void reset();

	void apply(QWidget * w);

	bool save(VipArchive &);
	bool save(const QString & file = QString());

	bool restore(VipArchive &);
	bool restore(const QString & file = QString());

Q_SIGNALS:
	void changed();

private Q_SLOTS:
	void emitChanged();
	void delaySaveToFile();
private:
	
	class PrivateData;
	PrivateData * m_data;
};


typedef QList<vip_double> DoubleList;
Q_DECLARE_METATYPE(DoubleList)
typedef QVector<vip_double> DoubleVector;
Q_DECLARE_METATYPE(DoubleVector)


/// @}
//end Gui

#endif
