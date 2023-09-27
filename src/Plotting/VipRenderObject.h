#ifndef VIP_RENDER_OBJECT_H
#define VIP_RENDER_OBJECT_H

#include <QList>
#include <QRectF>
#include <QMetaType>
#include <QMap>
#include <qpointer.h>

#include "VipGlobals.h"

/// \addtogroup Plotting
/// @{


class QObject;
class QGraphicsScene;
class VipRenderObject;

/// Render state used in VipRenderObject::startRender() and VipRenderObject::endRender().
class VIP_PLOTTING_EXPORT VipRenderState
{
public:
	VipRenderState() {}
	QVariantMap & state(VipRenderObject * item) {return m_map[item];}

private:
	QMap<VipRenderObject*,QVariantMap> m_map;
};

/// @brief Base class for all object implementing drawing features within the Plotting library
/// 
/// Class inheriting VipRenderObject might reimplement the following functions:
/// -	VipRenderObject::startRender(): the object is about to be rendered.
///		Use this function to save the current object state, and hide all unnecessary widgets/controls/item 
///		that you wish to exclude from the rendering. Use VipRenderObject::endRender() to restore the object state.
/// -	VipRenderObject::endRender(): the object has finished rendering and should restore its state prior to VipRenderObject::startRender().
/// -	VipRenderObject::renderObject(): render object in a QPainter. Default implementation work for all QWidget based objects
/// 
/// VipRenderObject also provides functions to save the content of a VipRenderObject object to an image/pdf/ps/eps/svg file.
/// 
/// Note that ps and eps formats are supported if the tool 'pdftops' is available within your PATH.
/// Use VipRenderObject::supportedVectorFormats() to check which vectorial formats are supported.
/// 
/// An example of image saving is provided in the StyleSheet example code.
/// 
class VIP_PLOTTING_EXPORT VipRenderObject
{
public:
	/// @brief Supported vectorial formats
	enum VectorFormats
	{
		PDF = 0x001,
		PS = 0x002,
		EPS = 0x004,
		SVG = 0x008
	};

	/// @brief Create VipRenderObject from QObject
	VipRenderObject(QObject * this_object);
	virtual ~VipRenderObject();

	/// @brief Returns the render object as a QObject
	QObject * thisObject() const { return m_object; }

	///Start rendering a QGraphicsScene
	static void startRender(QGraphicsScene * scene, VipRenderState&);
	///Stop rendering a QGraphicsScene
	static void endRender(QGraphicsScene * scene, VipRenderState&);
	///Start rendering a QObject
	static void startRender(QObject * obj, VipRenderState&);
	///Stop rendering a QObject
	static void endRender(QObject * obj, VipRenderState&);

	/// @brief Render a VipRenderObject into given painter at given position.
	/// If draw_children is true, only children actually ARE VipRenderObject objects are rendered.
	/// If draw_background is true, children QWidget (inheriting VipRenderObject) will draw their backgrounds.
	static void renderObject(VipRenderObject * obj, QPainter * p, const QPoint & pos, bool draw_children = true, bool draw_background = true);

	/// @brief Returns the VipRenderObject object bounding rectangle
	static QRectF renderRect(VipRenderObject* obj);
	
	/// @brief Returns supported vectorial file formats
	static int supportedVectorFormats();

	/// @brief Save VipRenderObject object int SVG file while using render current size
	/// @param render VipRenderObject object
	/// @param filename output SVG filename
	/// @param title SVG title
	/// @param description SVG description
	/// @return true on success, false otherwise
	static bool saveAsSvg(VipRenderObject* render, const QString& filename, const QString& title = QString(), const QString& description = QString());

	/// @brief Save VipRenderObject in PS or EPS file while using render current size
	/// @param render VipRenderObject object
	/// @param filename output filename
	/// @return true on success, false otherwise
	static bool saveAsPs(VipRenderObject* render, const QString& filename);

	/// @brief Save VipRenderObject object in PDF file while using render current size
	/// @param render VipRenderObject object
	/// @param filename output filename
	/// @param out_point_size is non null, the output point size is written to this variable
	/// @return true on success, false otherwise
	static bool saveAsPdf(VipRenderObject* render, const QString& filename, QSizeF* out_point_size = nullptr);

	/// @brief Save VipRenderObject object in image file while using render current size
	/// @param render VipRenderObject object
	/// @param filename output filename. See QImage documentation to know supported formats.
	/// @param in_background if null, the render will draw its background. Otherwise, the background will be set to provided color and VipRenderObject objects won't draw their backgrounds.
	/// @return true on success, false otherwise
	static bool saveAsImage(VipRenderObject* render, const QString& filename, QColor* in_background = nullptr);

protected
	  :
	/// Prepare the rendering of this object.
	/// Use this function to hide unwanted widgets before rendering.
	virtual void startRender(VipRenderState &) {}
	/// Stop the rendering.
	/// Use this function to restore the object state that might have been altered with startRender().
	virtual void endRender(VipRenderState &) {}
	/// Render this object, and this object only (not its children).
	/// If this function returns false, this object's children should not be drawn.
	virtual bool renderObject(QPainter * p, const QPointF & pos, bool draw_background = true) ;


private:
	QPointer<QObject> m_object;
};

Q_DECLARE_METATYPE(VipRenderState)
Q_DECLARE_METATYPE(VipRenderObject*)


/// @}
//end Plotting

#endif
