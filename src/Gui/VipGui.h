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

#ifndef VIP_GUI_H
#define VIP_GUI_H

#include <functional>

#include "VipDisplayObject.h"
#include "VipMapFileSystem.h"

class VipAbstractPlayer;

/// @brief Returns the first parent widget that can be casted to T
template<class T>
T* vipFindParent(QWidget* w)
{
	while (w) {
		if (T* p = qobject_cast<T*>(w))
			return p;
		w = w->parentWidget();
	}
	return nullptr;
}

/// Returns the standard foreground text brush for given widget.
/// The text color for all widgets is usually black. However, this might be changed by manually setting the palette or
/// setting a widget or application wide style sheet.
/// This function takes care of these cases to return the proper text color.
///
/// If \a w is nullptr, the function uses the application palette.
VIP_GUI_EXPORT QBrush vipWidgetTextBrush(QWidget* w);
/// Returns the default text error color used by QTextEdit derived classes.
/// The default error color is usually red, but it might change depending on the loaded skin.
VIP_GUI_EXPORT QColor vipDefaultTextErrorColor(QWidget* w);

/// Returns available skins
VIP_GUI_EXPORT QStringList vipAvailableSkins();
/// Load a specific skin.
/// A skin is a directory in the \a skin folder of name \a skin_name.
/// It contains at least a file \a stylesheet.css and an optional \a icons folder containing additional icons.
VIP_GUI_EXPORT bool vipLoadSkin(const QString& skin_name);

VIP_GUI_EXPORT bool vipIsDarkSkin();
VIP_GUI_EXPORT bool vipIsDarkColor(const QColor&);

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
VIP_GUI_EXPORT void vipSetQueryFunction(const std::function<QString(const QString&, const QString&)>& fun);
VIP_GUI_EXPORT std::function<QString(const QString&, const QString&)> vipQueryFunction();

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

	
	VIP_DECLARE_PRIVATE_DATA();

public:
	static VipFileSharedMemory& instance();
	bool addFilesToOpen(const QStringList& lst, bool new_workspace = false);
	/// Retrieve the file names to be opened in the shared memory "Thermavip_Files".
	/// These files are set with #VipFileSharedMemory::addFilesToOpen().
	/// This function returns the filenames (if any) and clear the shared memory.
	QStringList retrieveFilesToOpen(bool* new_workspace);
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

	VipGuiDisplayParamaters(VipMainWindow* win);

public:
	static VipGuiDisplayParamaters* instance(VipMainWindow* win = nullptr);

	~VipGuiDisplayParamaters();

	int itemPaletteFactor() const;
	bool videoPlayerShowAxes() const;
	Vip::PlayerLegendPosition legendPosition() const;
	QString playerColorScale() const;
	VipPlotArea2D* defaultPlotArea() const;
	VipPlotCurve* defaultCurve() const;
	void applyDefaultPlotArea(VipPlotArea2D* area);
	void applyDefaultCurve(VipPlotCurve* c);
	// VipPlotPlayer with time scale and date time since Epoch
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
	bool displayExactPixels() const;

	/// Returns the ROI border pen
	QPen shapeBorderPen();
	/// Returns the ROI background brush
	QBrush shapeBackgroundBrush();
	/// Returns the ROI drawn components
	VipPlotShape::DrawComponents shapeDrawComponents();

	QColor defaultPlayerTextColor() const;
	QColor defaultPlayerBackgroundColor() const;

	void setInSessionLoading(bool);
	bool inSessionLoading() const;

public Q_SLOTS:

	void setItemPaletteFactor(int);
	void setVideoPlayerShowAxes(bool);
	void setLegendPosition(Vip::PlayerLegendPosition pos);
	void setPlayerColorScale(const QString &);
	void setDisplayTimeOffset(bool);
	void setDisplayType(VipValueToTime::DisplayType);
	void setDefaultEditorFont(const QFont& font);
	void setAlwaysShowTimeMarker(bool);
	void setPlotTitleInside(bool);
	void setPlotGridVisible(bool);
	void setGlobalColorScale(bool);
	void setFlatHistogramStrength(int);
	void setTitleVisible(bool);
	void setTitleTextStyle(const VipTextStyle&);
	void setTitleTextStyle2(const VipText&);
	void setDefaultTextStyle(const VipTextStyle&);
	void setDefaultTextStyle2(const VipText&);
	void setVideoRenderingStrategy(int);
	void setPlotRenderingStrategy(int);
	void autoScaleAll();
	void setDisplayExactPixels(bool);

	/// Set the default border pen for all existing and future ROI
	void setShapeBorderPen(const QPen& pen);
	/// Set the default background brush for all existing and future ROI
	void setShapeBackgroundBrush(const QBrush& brush);
	/// Set the default drawn components for all existing and future ROI
	void setShapeDrawComponents(const VipPlotShape::DrawComponents& c);
	void reset();

	void apply(QWidget* w);

	bool save(VipArchive&);
	bool save(const QString& file = QString());

	bool restore(VipArchive&);
	bool restore(const QString& file = QString());

Q_SIGNALS:
	void changed();

private Q_SLOTS:
	void emitChanged();
	void delaySaveToFile();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

class VipAbstractPlayer;

/// Function dispatcher which create a VipDisplayObject object from a QVariant and a VipAbstractPlayer.
/// This dispatcher is called within vipCreateDisplayFromData to provide a custom behavior. Its goal is to create a new instance of VipDisplayObject
/// able to display given data into a specific player.
/// Its signature is:
/// \code
/// VipDisplayObject*(const QVariant &, VipAbstractPlayer *, const VipAnyData & any)
/// \endcode
VIP_GUI_EXPORT VipFunctionDispatcher<3>& vipFDCreateDisplayFromData();

/// Creates a VipDisplayObject able to display the data from \a any into \a player. The player might be nullptr.
VIP_GUI_EXPORT VipDisplayObject* vipCreateDisplayFromData(const VipAnyData& any, VipAbstractPlayer*);

/// Function dispatcher which create a list of VipAbstractPlayer instances that will display the given data.
/// This dispatcher is called within vipCreatePlayersFromData and vipCreatePlayersFromProcessing to provide a custom behavior.
/// If a non null player is given, this function will try to display the processing outputs into the player, and a list containing only the player is returned on success. An empty list will be
/// returned in case of failure.
///
///
/// Its signature is:
/// \code
/// QList<VipAbstractPlayer *> (const QVariant &, VipAbstractPlayer *, const VipAnyData & any, QObject * target)
/// \endcode
VIP_GUI_EXPORT VipFunctionDispatcher<4>& vipFDCreatePlayersFromData();
/// Creates a list of VipAbstractPlayer instances that will display the given data.
/// If a non null player is given, this function will try to display the data into the player, and a list containing only the player is returned on success. An empty list will be returned in case of
/// failure.
///
/// \a target is a QObject used to define the target in drop situations (in case of dropping, it is usually a #VipPlotItem).
VIP_GUI_EXPORT QList<VipAbstractPlayer*> vipCreatePlayersFromData(const VipAnyData& any,
								  VipAbstractPlayer* pl,
								  VipOutput* src = nullptr,
								  QObject* target = nullptr,
								  QList<VipDisplayObject*>* outputs = nullptr);

/// Function dispatcher which create a list of VipAbstractPlayer instances that will display the output(s) of a VipProcessingObject.
/// This dispatcher is called within vipCreatePlayersFromProcessing to provide a custom behavior.
/// If a non null player is given, this function will try to display the processing outputs into the player, and a list containing only the player is returned on success. An empty list will be
/// returned in case of failure. Its signature is: \code QList<VipAbstractPlayer *> (VipProcessingObject *, VipAbstractPlayer*,VipOutput*, QObject * target ); \endcode
VIP_GUI_EXPORT VipFunctionDispatcher<4>& vipFDCreatePlayersFromProcessing();

/// Creates a list of VipAbstractPlayer instances that will display the output(s) of a VipProcessingObject.
/// If a non null player is given, this function will try to display the processing outputs into the player, and a list containing only the player is returned on success. An empty list will be
/// returned in case of failure. If a non null VipOutput is given, only the data of this output will be displayed in the player.
///
/// This function try to minimize the number of returned players. Therefore, if the processing has multiple outputs that can be displayed into the same player
/// (like for instance multiple floating point values), only one player is created for all of them (displaying for instance multiple curves).
///
/// \a target is a QObject used to define the target in drop situations (in case of dropping, it is usually a #VipPlotItem).
VIP_GUI_EXPORT QList<VipAbstractPlayer*> vipCreatePlayersFromProcessing(VipProcessingObject* proc,
									VipAbstractPlayer* player,
									VipOutput* src = nullptr,
									QObject* target = nullptr,
									QList<VipDisplayObject*>* outputs = nullptr);

/// Creates a list of VipAbstractPlayer instances that will display the output(s) of a several VipProcessingObject instances.
/// This function try to minimize the number of returned players by inserting several display objects into the same player.
VIP_GUI_EXPORT QList<VipAbstractPlayer*> vipCreatePlayersFromProcessings(const QList<VipProcessingObject*>&,
									 VipAbstractPlayer*,
									 QObject* target = nullptr,
									 QList<VipDisplayObject*>* outputs = nullptr);

/// Creates a list of VipAbstractPlayer instances that will display the output(s) of a several VipProcessingObject instances.
/// This function try to minimize the number of returned players by inserting several display objects into the same player.
template<class T>
QList<VipAbstractPlayer*> vipCreatePlayersFromProcessings(const QList<T*>& lst, VipAbstractPlayer* player, QObject* target = nullptr, QList<VipDisplayObject*>* outputs = nullptr)
{
	return vipCreatePlayersFromProcessings(vipListCast<VipProcessingObject*>(lst), player, target, outputs);
}

/// Creates a list of VipAbstractPlayer instances that will display the output(s) of the VipIODevice objects created from all the given strings.
/// This function try to minimize the number of returned players by inserting several display objects into the same player.
VIP_GUI_EXPORT QList<VipAbstractPlayer*> vipCreatePlayersFromStringList(const QStringList& lst, VipAbstractPlayer* player, QObject* target = nullptr, QList<VipDisplayObject*>* outputs = nullptr);

/// Creates a list of VipAbstractPlayer instances that will display the output(s) of the VipIODevice objects created from all the given paths and QIODevice objetcs.
/// This function try to minimize the number of returned players by inserting several display objects into the same player.
VIP_GUI_EXPORT QList<VipAbstractPlayer*> vipCreatePlayersFromPaths(const VipPathList& paths, VipAbstractPlayer* player, QObject* target = nullptr, QList<VipDisplayObject*>* outputs = nullptr);

/// Helper function.
/// Create a new processing object (\a info) based on given VipOutput.
/// A VipProcessingList is added just after the processing.
VIP_GUI_EXPORT VipProcessingObject* vipCreateProcessing(VipOutput* output, const VipProcessingObject::Info& info);

/// Helper function.
/// Create a new data fusion processing (\a info) based on given VipOutput.
/// A VipProcessingList is added just after the data fusion processing.
VIP_GUI_EXPORT VipProcessingObject* vipCreateDataFusionProcessing(const QList<VipOutput*>& outputs, const VipProcessingObject::Info& info);
VIP_GUI_EXPORT VipProcessingObject* vipCreateDataFusionProcessing(const QList<VipPlotItem*>& items, const VipProcessingObject::Info& info);

/// @}
// end Gui

#endif
