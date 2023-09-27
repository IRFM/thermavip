#ifndef VIP_AXIS_BASE_H
#define VIP_AXIS_BASE_H

#include "VipBorderItem.h"
#include "VipScaleDraw.h"
#include "VipScaleEngine.h"
#include "VipScaleMap.h"

/// \addtogroup Plotting
/// @{

class VipPlotItem;

/// Standard axis class for cartesian system
///
/// Defines additional keywords for stylesheets:
/// -	'title-inverted': equivalent to VipAxisBase::setTitleInverted()
/// -	'title-inside':  equivalent to VipAxisBase::setTitleInside()
/// -	'use-border-dist-hint-for-layout: equivalent to VipAxisBase::useBorderDistHintForLayout()
/// 
class VIP_PLOTTING_EXPORT VipAxisBase : public VipBorderItem
{
	Q_OBJECT

public:

	//! Layout flags of the title
	enum LayoutFlag
	{
		/// The title of vertical scales is painted from top to bottom.
		/// Otherwise it is painted from bottom to top.
		TitleInverted = 1
	};
	//! Layout flags of the title
	typedef QFlags<LayoutFlag> LayoutFlags;


	VipAxisBase(Alignment pos = Left, QGraphicsItem * parent = 0);
	~VipAxisBase();

	void setLayoutFlag( LayoutFlag, bool on );
	bool testLayoutFlag( LayoutFlag ) const;

	void setMapScaleToScene(bool);
	bool isMapScaleToScene() const;

	void setTitleInverted(bool);
	bool isTitleInverted() const;

	void setTitleInside(bool);
	bool titleInside() const;

	virtual void setScaleDraw( VipScaleDraw * );
	virtual const VipScaleDraw * constScaleDraw() const;
	virtual VipScaleDraw * scaleDraw();

	QSizeF minimumSizeHint() const;
	double minimumLengthHint() const;

	virtual void getBorderDistHint( double &start, double &end ) const;

	/// Tells if the border dist hint (function #getBorderDistHint()) are used to layout the scale.
	/// If yes, it is likely that the backbone will be drawn inside the bounding rect, with a margin to the borders.
	/// This margin is created by the labels that might, otherwise, draw themselves outside the bounding rect.
	/// Only use this function to avoid drawing labels outisde the bounding rect.
	/// False by default.
	void setUseBorderDistHintForLayout(bool);
	bool useBorderDistHintForLayout() const;

	double dimForLength( double length, const QFont &scaleFont ) const;

	void drawTitle( QPainter *painter, Alignment,const QRectF &rect ) const;

	virtual void setAlignment( Alignment );
	virtual void layoutScale();
	virtual void draw ( QPainter * painter, QWidget * widget = 0 ) ;
	virtual void computeScaleDiv();

	virtual QRectF boundingRect() const;

	QPointF scalePosition() const;
	QPointF scaleEndPosition() const;

protected:
	virtual double extentForLength(double length) const;

	//virtual QVariant itemChange ( GraphicsItemChange change, const QVariant & value );
	virtual void itemGeometryChanged(const QRectF &);
	virtual double additionalSpace() const {return 0.;}

	/// @brief Set property based on stylesheet
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());
	/// @brief Implement selectors 'title' and 'legend'
	virtual bool hasState(const QByteArray& state, bool enable) const; 
	class PrivateData;
	PrivateData * d_data;
};


/// Vertically stacked axes for cartesian system used by VipVMultiPlotArea2D
class VIP_PLOTTING_EXPORT VipMultiAxisBase : public VipBorderItem
{
	Q_OBJECT

public:

	//! Layout flags of the title
	enum LayoutFlag
	{
		/// The title of vertical scales is painted from top to bottom.
		/// Otherwise it is painted from bottom to top.
		TitleInverted = 1
	};
	//! Layout flags of the title
	typedef QFlags<LayoutFlag> LayoutFlags;

	VipMultiAxisBase(Alignment pos = Left, QGraphicsItem * parent = 0);
	~VipMultiAxisBase();

	static VipMultiAxisBase * fromScale(VipBorderItem * item);

	virtual void setAlignment(Alignment);
	virtual void layoutScale();
	virtual void getBorderDistHint(double &start, double &end) const;
	virtual void setItemIntervalFactor(double f);

	void setLayoutFlag(LayoutFlag, bool on);
	bool testLayoutFlag(LayoutFlag) const;

	void setTitleInverted(bool);
	bool isTitleInverted() const;

	void addScale(VipBorderItem * it);
	void insertScale(int index, VipBorderItem * it);
	void remove(VipBorderItem * it);
	VipBorderItem* takeItem(int index);
	int indexOf(const VipBorderItem * it) const;
	int count() const;
	VipBorderItem * at(int);
	const VipBorderItem * at(int) const;

	/// @brief Set the space between scales
	/// Default to 0
	void setScaleSpacing(double space);
	double scaleSpacing() const;

	virtual void draw(QPainter* painter, QWidget* widget = 0);
	void drawTitle(QPainter* painter, Alignment, const QRectF& rect) const;

protected:
	virtual double extentForLength(double length) const;
	virtual void itemGeometryChanged(const QRectF &);
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant & value);
	void updateParents();
private:
	double titleExtent() const; 

	class PrivateData;
	PrivateData * d_data;
};

/// @}
//end Plotting

#endif
