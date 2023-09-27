#ifndef VIP_PLOT_HISTOGRAM_H
#define VIP_PLOT_HISTOGRAM_H

#include "VipPlotItem.h"
#include "VipInterval.h"
#include "VipDataType.h"

/// \addtogroup Plotting
/// @{


/// \brief VipPlotHistogram represents a series of samples, where an interval
///      is associated with a value ( \f$y = f([x1,x2])\f$ ).
///
/// The representation depends on the style() value (HistogramStyle enum).
///
/// \note The term "histogram" is used in a different way in the areas of
///     digital image processing and statistics. Wikipedia introduces the
///     terms "image histogram" and "color histogram" to avoid confusions.
///     While "image histograms" can be displayed by a VipPlotCurve there
///     is no applicable plot item for a "color histogram" yet.
///
/// 
/// VipPlotHistogram supports stylesheets and defines the following attributes:
/// -	'text-alignment' : see VipPlotHistogram::setTextAlignment(), combination of 'left|right|top|bottom|center|vcenter|hcenter'
/// -	'text-position': see VipPlotHistogram::setTextPosition(), combination of 'outside|xinside|yinside|xautomatic|yautomatic|automatic'
/// -	'text-distance': see VipPlotHistogram::setTextDistance()
/// -	'style': histogram style, one of 'lines', 'columns', 'outline'
/// -	'border-radius': border radius for the columns
/// 
/// In addition, VipPlotHistogram supports the selectors 'lines', 'columns', 'outline'.
/// 
/// Usage:
/// 
/// \code{cpp}
/// 
/// #include <cmath>
/// #include <qapplication.h>
/// #include "VipPlotHistogram.h"
/// #include "VipPlotWidget2D.h"
/// 
/// 
/// double norm_pdf(double x, double mu, double sigma)
/// {
/// 	return 1.0 / (sigma * sqrt(2.0 * M_PI)) * exp(-(pow((x - mu) / sigma, 2) / 2.0));
/// }
///
/// VipIntervalSampleVector offset(const VipIntervalSampleVector& hist, double y)
/// {
/// 	VipIntervalSampleVector res = hist;
/// 	for (int i = 0; i < res.size(); ++i)
/// 		res[i].value += y;
/// 	return res;
/// }
/// 
/// int main(int argc, char** argv)
/// {
///		QApplication app(argc, argv);
///	
///		VipPlotWidget2D w;
///		w.area()->setMouseWheelZoom(true);
///		w.area()->setMousePanning(Qt::RightButton);
///		w.area()->setMargins(VipMargins(10, 10, 10, 10));
///		w.area()->titleAxis()->setVisible(true);
///		w.area()->setTitle("<b>Various histogram styles</b>");
///	
///		// generate histogram
///		VipIntervalSampleVector hist;
///		for (int i = -10; i < 10; ++i) {
///			VipIntervalSample s;
///			s.interval = VipInterval(i, i + 1);
///			s.value = norm_pdf(i, 0, 2) * 5;
///			hist.push_back(s);
///		}
///	
///		double yoffset = 0;
///		{
///			VipPlotHistogram* h = new VipPlotHistogram("Columns with text");
///			h->setRawData(hist);
///			h->setStyle(VipPlotHistogram::Columns);
///			h->boxStyle().setBackgroundBrush(QBrush(QColor(0x0178BB)));
///			h->boxStyle().setBorderPen(QPen(QColor(0x0178BB).lighter()));
///			h->setText("#value%.2f");
///			h->setTextPosition(Vip::XInside);
///			h->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
///		}
///		yoffset += 1.5;
///		{
///			VipPlotHistogram* h = new VipPlotHistogram("Outline");
///			h->setRawData(offset(hist, yoffset));
///			h->setBaseline(yoffset);
///			h->setStyle(VipPlotHistogram::Outline);
///			h->boxStyle().setBackgroundBrush(QBrush(QColor(0x0178BB)));
///			h->boxStyle().setBorderPen(QPen(QColor(0x0178BB).lighter()));
///			h->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
///		}
///		yoffset += 1.5;
///		{
///			VipPlotHistogram* h = new VipPlotHistogram("Lines");
///			h->setRawData(offset(hist, yoffset));
///			h->setBaseline(yoffset);
///			h->setStyle(VipPlotHistogram::Lines);
///			h->boxStyle().setBackgroundBrush(QBrush(QColor(0x0178BB)));
///			h->boxStyle().setBorderPen(QPen(QColor(0x0178BB).lighter()));
///			h->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
///		}
///	
///		w.show();
///		return app.exec();
///	}
/// 
/// \endcode
/// 
	class VIP_PLOTTING_EXPORT VipPlotHistogram: public VipPlotItemDataType< VipIntervalSampleVector>
{
	Q_OBJECT

public:
    /// Histogram styles.
    /// The default style is VipPlotHistogram::Columns.
	///
    /// \sa setStyle(), style(), setBaseline()
    enum HistogramStyle
    {
        /// Draw an outline around the area, that is build by all intervals
        /// using the pen() and fill it with the brush(). The outline style
        /// requires, that the intervals are in increasing order and
        /// not overlapping.
        Outline,

        /// Draw a column for each interval.
        Columns,

        /// Draw a simple line using the pen() for each interval.
        Lines,

        /// Styles >= UserStyle are reserved for derived
        /// classes that overload drawSeries() with
        /// additional application specific ways to display a histogram.
        UserStyle = 100
    };

    /// @brief Construct from histogram title
    explicit VipPlotHistogram( const VipText &title = VipText() );
    virtual ~VipPlotHistogram();

    /// @brief Set/get the box style used to render the histogram
    void setBoxStyle(const VipBoxStyle & );
	const VipBoxStyle & boxStyle() const;
	VipBoxStyle & boxStyle();

	/// @brief Reimplemented from VipPlotItem, returns the pen color if defined, or the background brush color
	virtual QColor majorColor() const {
		if (boxStyle().borderPen().style() == Qt::NoPen || boxStyle().borderPen().color().alpha() == 0)
			return boxStyle().backgroundBrush().color();
		return boxStyle().borderPen().color();
	}
	/// @brief Reimplemented from VipPlotItem, set the color of the border pen and background brush
	virtual void setMajorColor(const QColor & c) {
		QPen p = boxStyle().borderPen();
		p.setColor(c);
		QBrush b = boxStyle().backgroundBrush();
		b.setColor(c);
		boxStyle().setBorderPen(p);
		boxStyle().setBackgroundBrush(b);
	}
	/// @brief Reimplemented from VipPlotItem, set the border pen
	virtual void setPen(const QPen & p) {
		boxStyle().setBorderPen(p);
	}
	virtual QPen pen() const {
		return boxStyle().borderPen();
	}
	/// @brief Reimplemented from VipPlotItem, set the background brush
	virtual void setBrush(const QBrush & b) {
		boxStyle().setBackgroundBrush(b);
	}
	virtual QBrush brush() const {
		return boxStyle().backgroundBrush();
	}

	/// @brief Reimplemented from VipPlotItem in order to be stylesheet aware
	virtual void setTextStyle(const VipTextStyle& st);
	virtual VipTextStyle textStyle() const { return text().textStyle(); }

	/// @brief Reimplemented from VipPlotItemData, set the data as a QVariant containing a VipIntervalSampleVector
    virtual void setData( const QVariant & );
	/// @brief Reimplemented from VipPlotItem
    virtual VipInterval plotInterval(const VipInterval & interval = Vip::InfinitInterval) const;

	/// @brief Set the bar text alignment within its bar based on the text position
	void setTextAlignment(Qt::Alignment align);
	Qt::Alignment textAlignment() const;

	 /// @brief Set the bar text position: inside or outside the bar
	void setTextPosition(Vip::RegionPositions pos);
	Vip::RegionPositions textPosition() const;

	/// @brief Additional custom text transform.
	/// By default, the transform is applied from the top left corner of the text rectangle.
	/// You can specify a different origin using the ref parameter, which is a relative x and y distance from the rectangle dimensions.
	/// For Instance, to apply a rotation around the text center, use QPointF(0.5,0.5).
	void setTextTransform(const QTransform& tr, const QPointF& ref = QPointF(0, 0));
	const QTransform& textTransform() const;
	const QPointF& textTransformReference() const;

	/// @brief Set the distance (in item's coordinate) between a bar border and its text
	/// @param distance 
	void setTextDistance(double distance);
	double textDistance() const;

	/// @brief Set the text to be drawn within each bar of the histogram.
	/// Each occurrence of the content '#value' will be replaced by the bar value.
	/// Each occurrence of the content '#min' will be replaced by the bar minimum X value, and '#max' by the bar maximum X value.
	void setText(const VipText& text);
	const VipText & text() const;
	VipText & text();

	/// @brief Set the value of the baseline
	///
	/// Each column representing an VipIntervalSample is defined by its
	/// interval and the interval between baseline and the value of the sample.
	///
	/// The default value of the baseline is 0.0.
	///
	/// @param value Value of the baseline
    void setBaseline(vip_double reference );
	vip_double baseline() const;

    /// Set the histogram's drawing style
    ///
    /// @param style Histogram style
    /// @sa HistogramStyle, style()
    void setStyle( HistogramStyle style );
	/// @return Style of the histogram
	/// @sa HistogramStyle, setStyle()
    HistogramStyle style() const;


	virtual bool areaOfInterest(const QPointF & pos, int axis, double maxDistance, VipPointVector & out_pos, VipBoxStyle & style, int & legend) const;
	virtual QString formatText(const QString & str, const QPointF & pos) const;
    virtual void draw(QPainter *, const VipCoordinateSystemPtr &) const ;
    virtual QList<VipText> legendNames() const;
    virtual QRectF drawLegend(QPainter *, const QRectF &, int index) const;
	virtual QList<VipInterval> plotBoundingIntervals() const;

	static QList<VipInterval> dataBoundingIntervals(const VipIntervalSampleVector &, vip_double);

protected:

	virtual bool hasState(const QByteArray& state, bool enable) const;
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

    virtual VipPointVector columnRect( const VipIntervalSample &, QRectF * = NULL) const;

    void drawColumns( QPainter *, const VipCoordinateSystemPtr & m) const;

    void drawOutline( QPainter *, const VipCoordinateSystemPtr & m) const;

    void drawLines( QPainter *, const VipCoordinateSystemPtr & m) const;

	QString formatSampleText(const QString & str, const VipIntervalSample & s) const;

private:

   // void flushPolygon( QPainter *,const VipCoordinateSystemPtr & , double baseLine, QPolygonF & ,QPolygonF &) const;

    class PrivateData;
    PrivateData *d_data;
};

/// @}
//end Plotting

#endif
