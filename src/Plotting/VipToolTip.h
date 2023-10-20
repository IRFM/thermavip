#ifndef VIP_TOOL_TIP_H
#define VIP_TOOL_TIP_H


#include "VipBoxStyle.h"
#include "VipText.h"

#include <QMargins>
#include <qlabel.h>

class VipAbstractPlotArea;
class VipAbstractScale;
class VipPlotItem;

/// \addtogroup Plotting
/// @{

class VIP_PLOTTING_EXPORT VipToolTip : public QObject
{
	Q_OBJECT

	public:

	enum DisplayFlag
	{
		PlotArea = 0x0001, // VipAbstractPlotArea tool tip text as returned by #VipAbstractPlotArea::formatToolTip()
		Axes = 0x0002, //  Axes titles and values (only if axis has a title)
		ItemsTitles = 0x0004, //Title of each VipPlotItem
		ItemsLegends = 0x0008, //Legends of each VipPlotItem (if any)
		ItemsPos = 0x0020, // Item's position from #VipPlotItem::areaOfInterest() (if different from mouse position)
		ItemsProperties = 0x0040, //Item's dynamic properties (if any)
		ItemsToolTips = 0x0080, //Item's custom tool tip text, as returned by #VipPlotItem::formatToolTip() (replace all the previous item's tooltips)
		SearchXAxis = 0x0100,
		SearchYAxis = 0x0200,
		Hidden = 0x0400,
		All = PlotArea|Axes|ItemsTitles|ItemsLegends|ItemsPos|ItemsProperties|ItemsToolTips| SearchXAxis| SearchYAxis
	};
	Q_DECLARE_FLAGS(DisplayFlags, DisplayFlag);

	VipToolTip(QObject * parent = nullptr);
	virtual ~VipToolTip();

	void setPlotArea(VipAbstractPlotArea *);
	VipAbstractPlotArea * plotArea() const;

	void setMinRefreshTime(int milli);
	int minRefreshTime() const;

	void setMaxLines(int max_lines);
	int maxLines() const;

	void setMaxLineMessage(const QString & line_msg);
	QString maxLineMessage() const;

	void setMargins(const QMargins&);
	void setMargins(double);
	const QMargins & margins() const;
	QString attributeMargins() const;

	void setDelayTime(int msec);
	int delayTime() const;

	void setDisplayFlags(DisplayFlags);
	void setDisplayFlag( DisplayFlag, bool on = true );
	bool testDisplayFlag( DisplayFlag ) const;
	DisplayFlags displayFlags() const;

	///Set the scales that should appear in the tool tip.
	/// Note that the scales are ignored if 'Axes' flag is not set.
	void setScales(const QList<VipAbstractScale*> & scales);
	const QList<VipAbstractScale*> & scales() const;

	/// Set the tool tip position. Default is Automatic (the tool tip follow the mouse position).
	/// If the region position is not automatic, the tool tip position will be fixed (inside or outside the plot area).
	/// Use #VipToolTip::setAlignment to have a better control on the tool tip position.
	void setRegionPositions(Vip::RegionPositions pos);
	Vip::RegionPositions regionPositions() const;

	/// Set the tool tip alignment. The result will depend on the region position.
	/// This alignment is ignored if the region position is set to automatic.
	void setAlignment(Qt::Alignment);
	Qt::Alignment alignment() const;

	/// Set the tool tip top left corner offset based on the mouse position.
	/// If you set this option, the tool tip will always have a fixed position depending the mouse one,
	/// no matter what are the values of regionPositions() and alignment().
	/// Use removeToolTipOffset() to revert back to the standard tool tip position managment (based on regionPositions() and alignment()).
	void setToolTipOffset(const QPoint &);
	QPoint toolTipOffset() const;
	void removeToolTipOffset();
	bool hasToolTipOffset() const;


	/// If the region position is not automatic and enable is true, the area used to compute the tool tip position is defined by the plot area scales.
	/// Otherwise, the area is defined by the plot area bounding rect (which is usually around the scales area).
	void setDisplayInsideScales(bool enable);
	bool displayInsideScales() const;

	void setStickDistance(double);
	double stickDistance() const;

	void setDistanceToPointer(double);
	double distanceToPointer() const;

	void setOverlayPen(const QPen &);
	QPen overlayPen() const;

	void setOverlayBrush(const QBrush &);
	QBrush overlayBrush() const;

	void setIgnoreProperties(const QStringList & names);
	void addIgnoreProperty(const QString & name);
	QStringList ignoreProperties() const;
	bool isPropertyIgnored(const QString & name) const;

	virtual void setPlotAreaPos(const QPointF & pos);

public Q_SLOTS:
	void refresh();

private:

	QPoint toolTipPosition( VipText & text, const QPointF & pos, Vip::RegionPositions position, Qt::Alignment align);

	struct PrivateData;
	PrivateData * d_data;

};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipToolTip::DisplayFlags)


/// @}
//end Plotting

#endif
