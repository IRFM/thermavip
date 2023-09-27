#include "VipGui.h"
#include "VipUniqueId.h"
#include "VipCore.h"
#include "VipDisplayArea.h"
#include "VipXmlArchive.h"
#include "VipLegendItem.h"
#include "VipEnvironment.h"
#include "VipPlotWidget2D.h"

VipPlotItem * vipCopyPlotItem(const VipPlotItem* item)
{
	VipXOStringArchive arch;
	arch.content("item", QVariant::fromValue(const_cast<VipPlotItem*>(item)));

	VipXIStringArchive iarch(arch.toString());
	iarch.setProperty("_vip_no_id_or_scale", true);
	return iarch.read("item").value<VipPlotItem*>();
}

QByteArray vipSavePlotItemState(const VipPlotItem* item)
{
	VipXOStringArchive arch; 
	arch.content("item", QVariant::fromValue(const_cast<VipPlotItem*>(item)));
	return arch.toString().toLatin1();
}

bool vipRestorePlotItemState(VipPlotItem* item, const QByteArray & state)
{
	VipXIStringArchive iarch(state);
	iarch.setProperty("_vip_no_id_or_scale", true);
	return iarch.content("item", item);
}

VipArchive & operator<<(VipArchive & arch, const VipPlotItem * value)
{
	arch.content("id", VipUniqueId::id(value))
		.content("title", value->title())
		.content("attributes", (int)value->itemAttributes())
		.content("renderHints", (int)value->renderHints())
		.content("compositionMode", (int)value->compositionMode())
		.content("selectedPen", value->selectedPen())
		.content("axisUnits", value->axisUnits())
		.content("visible", value->isVisible());

	//save the color map
	if (value->colorMap())
		//new in 2.2.17: save id as a VipAbstractScale instead of VipAxisColorMap
		arch.content("colorMap", VipUniqueId::id<VipAbstractScale>(value->colorMap()));
	else
		arch.content("colorMap", 0);

	//save the axes
	arch.content("coordinateSystem", (int)value->coordinateSystemType());
	QList<VipAbstractScale*> scales = value->axes();
	arch.content("axisCount", scales.size());
	for (int i = 0; i < scales.size(); ++i)
		arch.content("axisId", VipUniqueId::id(scales[i]));

	//save the properties
	QList<QByteArray> names = value->dynamicPropertyNames();
	QVariantMap properties;
	for (int i = 0; i < names.size(); ++i)
		if (!names[i].startsWith("_q_"))
		{
			QVariant v = value->property(names[i]);
			if (v.userType() > 0 && v.userType() < QMetaType::User)
			{
				properties[names[i]] = v;
			}
		}

	arch.content("properties", properties);

	//save the additional texts
	const QMap<int, VipPlotItem::ItemText> texts = value->texts();

	arch.content("textCount", texts.size());
	arch.start("texts");
	for (QMap<int, VipPlotItem::ItemText>::const_iterator it = texts.begin(); it != texts.end(); ++it)
	{
		arch.content("text", it.value().text);
		arch.content("position", (int)it.value().position);
		arch.content("alignment", (int)it.value().alignment);
	}
	arch.end();

	
	arch.content("styleSheet", value->styleSheetString());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotItem * value)
{
	int id = arch.read("id").value<int>();
	if (!arch.property("_vip_no_id_or_scale").toBool())
		VipUniqueId::setId(value, id);
	value->setTitle(arch.read("title").value<VipText>());
	value->setItemAttributes(VipPlotItem::ItemAttributes(arch.read("attributes").value<int>()));
	value->setRenderHints(QPainter::RenderHints(arch.read("renderHints").value<int>()));
	value->setCompositionMode(QPainter::CompositionMode(arch.read("compositionMode").value<int>()));
	value->setSelectedPen(arch.read("selectedPen").value<QPen>());
	QList<VipText> units = arch.read("axisUnits").value<QList<VipText> >();
	for (int i = 0; i < units.size(); ++i)
		value->setAxisUnit(i, units[i]);
	value->setVisible(arch.read("visible").toBool());

	//load the color map
	id = arch.read("colorMap").toInt();
	if (id && !arch.property("_vip_no_id_or_scale").toBool())
	{
		//new in 2.2.17: interpret id as a VipAbstractScale instead of VipAxisColorMap
		VipAxisColorMap * axis = qobject_cast<VipAxisColorMap*>(VipUniqueId::find<VipAbstractScale>(id));
		if (!axis)
			axis = VipUniqueId::find<VipAxisColorMap>(id);
		if (axis)
			value->setColorMap(axis);
	}

	//try to set the axes
	int coordinateSystem = arch.read("coordinateSystem").toInt();
	int count = arch.read("axisCount").toInt();
	if (count)
	{
		QList<VipAbstractScale*> scales;
		for (int i = 0; i < count; ++i)
		{
			VipAbstractScale * scale = VipUniqueId::find<VipAbstractScale>(arch.read("axisId").toInt());
			scales.append(scale);
		}
		if (!arch.property("_vip_no_id_or_scale").toBool())
			value->setAxes(scales, (VipCoordinateSystem::Type)coordinateSystem);
	}

	arch.save();
	QVariantMap properties;
	if (arch.content("properties", properties))
	{
		for (QVariantMap::const_iterator it = properties.begin(); it != properties.end(); ++it)
			value->setProperty(it.key().toLatin1().data(), it.value());
	}
	else
		arch.restore();

	//read additional texts
	count = arch.read("textCount").toInt();
	if (count && (bool)arch.start("texts"))
	{
		while (arch) {
			VipText text = arch.read("text").value<VipText>();
			Vip::RegionPositions position = (Vip::RegionPositions)arch.read("position").toInt();
			Qt::Alignment alignment = (Qt::Alignment)arch.read("alignment").toInt();

			if (arch) {
				value->addText(text, position, alignment);
			}
		}
		arch.end();
	}
	arch.resetError();

	

	arch.save();
	QString st;
	if (arch.content("styleSheet", st))
		value->setStyleSheet(st);
	else
		arch.restore();

	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotItemData * value)
{
	//arch.content("maxSampleCount", value->maxSampleCount());
	QVariant v = value->data();
	if (v.userType() == qMetaTypeId<VipPointVector>()) {
		//for VipPointVector only, downsample to 100 points to avoid having too big session files
		const VipPointVector pts = v.value<VipPointVector>();
		if (pts.size() > 100) {
			double step = pts.size() / 100.0;
			VipPointVector tmp;
			for (double s = 0; s < pts.size(); s += step) {
				int index = (int)s;
				tmp.push_back(pts[index]);
			}
			v = vipToVariant(tmp);
		}
	}
	arch.content("data", v);
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotItemData * value)
{
	//value->setMaxSampleCount(arch.read("maxSampleCount").toInt());
	value->setData(arch.read("data"));
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotCurve * value)
{
	arch.content("legendAttributes", (int)value->legendAttributes());
	arch.content("curveAttributes", (int)value->curveAttributes());
	arch.content("boxStyle", value->boxStyle());
	arch.content("baseline", value->baseline());
	arch.content("curveStyle", (int)value->style());
	if (value->symbol())
		arch.content("symbol", *value->symbol());
	else
		arch.content("symbol", VipSymbol());
	arch.content("symbolVisible", value->symbolVisible());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotCurve * value)
{
	value->setLegendAttributes(VipPlotCurve::LegendAttributes(arch.read("legendAttributes").value<int>()));
	value->setCurveAttributes(VipPlotCurve::CurveAttributes(arch.read("curveAttributes").value<int>()));
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setBaseline(arch.read("baseline").value<double>());
	value->setStyle(VipPlotCurve::CurveStyle(arch.read("curveStyle").value<int>()));
	value->setSymbol(new VipSymbol(arch.read("symbol").value<VipSymbol>()));
	value->setSymbolVisible(arch.read("symbolVisible").toBool());
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotHistogram * value)
{
	return arch.content("boxStyle", value->boxStyle())
		.content("textPosition", (int)value->textPosition())
		//.content("textRotation", value->textRotation())
		.content("textDistance", value->textDistance())
		.content("text", value->text())
		.content("baseline", value->baseline())
		.content("style", (int)value->style());
}

VipArchive & operator>>(VipArchive & arch, VipPlotHistogram * value)
{
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());
	//value->setTextRotation(arch.read("textRotation").value<double>());
	value->setTextDistance(arch.read("textDistance").value<double>());
	value->setText(arch.read("text").value<VipText>());
	value->setBaseline(arch.read("baseline").value<double>());
	value->setStyle((VipPlotHistogram::HistogramStyle)arch.read("style").value<int>());
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotGrid * value)
{
	arch.content("minorPen", value->minorPen());
	arch.content("majorPen", value->majorPen());
	arch.content("_vip_customDisplay", value->property("_vip_customDisplay").toInt());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotGrid * value)
{
	value->setMinorPen(arch.read("minorPen").value<QPen>());
	value->setMajorPen(arch.read("majorPen").value<QPen>());
	//new in 2.2.18
	int _vip_customDisplay;
	if (arch.content("_vip_customDisplay", _vip_customDisplay))
		value->setProperty("_vip_customDisplay", _vip_customDisplay);
	else
		arch.restore();
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotCanvas * value)
{
	arch.content("boxStyle", value->boxStyle());
	//new in 2.2.18
	arch.content("_vip_customDisplay", value->property("_vip_customDisplay").toInt());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotCanvas * value)
{
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	//new in 2.2.18
	int _vip_customDisplay;
	if (arch.content("_vip_customDisplay", _vip_customDisplay))
		value->setProperty("_vip_customDisplay", _vip_customDisplay);
	else
		arch.restore();
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotMarker * value)
{
	arch.content("lineStyle", (int)value->lineStyle())
		.content("linePen", value->linePen())
		.content("label", value->label())
		.content("labelAlignment", (int)value->labelAlignment())
		.content("labelOrientation", (int)value->labelOrientation())
		.content("spacing", value->spacing());
	if (value->symbol())
		return arch.content("symbol", *value->symbol());
	else
		return arch.content("symbol", VipSymbol());
}

VipArchive & operator>>(VipArchive & arch, VipPlotMarker * value)
{
	value->setLineStyle((VipPlotMarker::LineStyle)arch.read("lineStyle").value<int>());
	value->setLinePen(arch.read("linePen").value<QPen>());
	value->setLabel(arch.read("label").value<VipText>());
	value->setLabelAlignment((Qt::AlignmentFlag)arch.read("labelAlignment").value<int>());
	value->setLabelOrientation((Qt::Orientation)arch.read("labelOrientation").value<int>());
	value->setSpacing(arch.read("spacing").value<double>());
	value->setSymbol(new VipSymbol(arch.read("symbol").value<VipSymbol>()));
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotRasterData *)
{
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotRasterData *)
{
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotSpectrogram * value)
{
	arch.content("defaultContourPen", value->defaultContourPen());
	arch.content("ignoreAllVerticesOnLevel", value->ignoreAllVerticesOnLevel());
	QList< vip_double > levels = value->contourLevels();
	for (int i = 0; i < levels.size(); ++i)
		arch.content("level", levels[i]);
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotSpectrogram * value)
{
	value->setDefaultContourPen(arch.read("defaultContourPen").value<QPen>());
	value->setIgnoreAllVerticesOnLevel(arch.read("ignoreAllVerticesOnLevel").value<bool>());
	QList< vip_double > levels;
	while (true)
	{
		QVariant tmp = arch.read();
		if (tmp.userType() == 0)
			break;
		levels.append(tmp.toDouble());
	}
	value->setContourLevels(levels);
	arch.resetError();
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotShape * value)
{
	arch.content("dawComponents", (int)value->dawComponents());
	arch.content("textStyle", value->textStyle());
	arch.content("textPosition", (int)value->textPosition());
	arch.content("textAlignment", (int)value->textAlignment());
	arch.content("adjustTextColor", (int)value->adjustTextColor());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotShape * value)
{
	value->setDrawComponents((VipPlotShape::DrawComponents)arch.read("dawComponents").value<int>());
	value->setTextStyle(arch.read("textStyle").value<VipTextStyle>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());
	value->setTextAlignment((Qt::AlignmentFlag)arch.read("textAlignment").value<int>());
	arch.save();
	value->setAdjustTextColor(arch.read("adjustTextColor").value<bool>());
	if (!arch)
		arch.restore();
	arch.resetError();
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotSceneModel * value)
{
	//mark internal shapes as non serializable, they will recreated when reloading the VipPlotSceneModel
	for (int i = 0; i < value->count(); ++i) {
		if (VipPlotShape * sh = qobject_cast<VipPlotShape*>(value->at(i))) {
			sh->setProperty("_vip_no_serialize", true);
			if (VipResizeItem * re = (sh->property("VipResizeItem").value<VipResizeItemPtr>()))
				re->setProperty("_vip_no_serialize", true);
		}
	}

	return arch.content("mode", (int)value->mode()).content("sceneModel", value->sceneModel());
}

VipArchive & operator>>(VipArchive & arch, VipPlotSceneModel * value)
{
	value->setMode((VipPlotSceneModel::Mode)arch.read("mode").toInt());
	value->setSceneModel(arch.read("sceneModel").value<VipSceneModel>());
	return arch;
}

static DoubleVector toDoubleVector(const DoubleList & lst)
{
	DoubleVector res;
	res.resize(lst.size());
	for (int i = 0; i < lst.size(); ++i)
		res[i] = lst[i];
	return res;
}

static int registerDoubleList = qRegisterMetaType<DoubleList>("DoubleList");
static int registerDoubleListStream = qRegisterMetaTypeStreamOperators<DoubleList>();
static int registerDoubleVector = qRegisterMetaType<DoubleVector>("DoubleVector");
static int registerDoubleVectorStream = qRegisterMetaTypeStreamOperators<DoubleVector>();
static bool convert = QMetaType::registerConverter<DoubleList, DoubleVector>(toDoubleVector);

VipArchive & operator<<(VipArchive & arch, const VipScaleDiv & value)
{
	arch.content("MinorTicks", value.ticks(VipScaleDiv::MinorTick));
	arch.content("MediumTick", value.ticks(VipScaleDiv::MediumTick));
	arch.content("MajorTick", value.ticks(VipScaleDiv::MajorTick));
	arch.content("lowerBound", value.lowerBound());
	arch.content("upperBound", value.upperBound());
	return arch;
}

VipArchive & operator >> (VipArchive & arch, VipScaleDiv & value)
{
	value.setTicks(VipScaleDiv::MinorTick, arch.read("MinorTicks").value<DoubleVector>());
	value.setTicks(VipScaleDiv::MediumTick, arch.read("MediumTick").value<DoubleVector>());
	value.setTicks(VipScaleDiv::MajorTick, arch.read("MajorTick").value<DoubleVector>());
	value.setLowerBound(arch.read("lowerBound").toDouble());
	value.setUpperBound(arch.read("upperBound").toDouble());
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipAbstractScale * value)
{
	arch.content("id", VipUniqueId::id(value));
	arch.content("boxStyle", value->boxStyle());
	arch.content("isAutoScale", value->isAutoScale());
	arch.content("title", value->title());
	arch.content("majorTextStyle", value->textStyle(VipScaleDiv::MajorTick));
	arch.content("mediumTextStyle", value->textStyle(VipScaleDiv::MediumTick));
	arch.content("minorTextStyle", value->textStyle(VipScaleDiv::MinorTick));
	arch.content("majorTransform", value->labelTransform(VipScaleDiv::MajorTick));
	arch.content("mediumTransform", value->labelTransform(VipScaleDiv::MediumTick));
	arch.content("minorTransform", value->labelTransform(VipScaleDiv::MinorTick));
	arch.content("isDrawTitleEnabled", value->isDrawTitleEnabled());
	arch.content("startBorderDist", value->startBorderDist());
	arch.content("endBorderDist", value->endBorderDist());
	arch.content("startMinBorderDist", value->startMinBorderDist());
	arch.content("endMinBorderDist", value->endMinBorderDist());
	arch.content("startMaxBorderDist", value->startMaxBorderDist());
	arch.content("endMaxBorderDist", value->endMaxBorderDist());
	arch.content("margin", value->margin());
	arch.content("spacing", value->spacing());
	arch.content("isScaleInverted", value->isScaleInverted());
	arch.content("maxMajor", value->maxMajor());
	arch.content("maxMinor", value->maxMinor());
	//new in 3.0.1
	arch.content("autoExponent", value->constScaleDraw()->valueToText()->automaticExponent());
	arch.content("minLabelSize", value->constScaleDraw()->valueToText()->maxLabelSize());
	arch.content("exponent", value->constScaleDraw()->valueToText()->exponent());

	arch.content("scaleDiv", value->scaleDiv());
	arch.content("renderHints", (int)value->renderHints());
	arch.content("visible", (int)value->isVisible());
	//save the y scale engine type
	arch.content("yScaleEngine", value->scaleEngine() ? (int)value->scaleEngine()->scaleType() : 0);

	arch.content("styleSheet", value->styleSheetString());

	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipAbstractScale * value)
{
	VipUniqueId::setId(value, arch.read("id").toInt());
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setAutoScale(arch.read("isAutoScale").value<bool>());
	value->setTitle(arch.read("title").value<VipText>());
	value->setTextStyle(arch.read("majorTextStyle").value<VipTextStyle>(), VipScaleDiv::MajorTick);
	value->setTextStyle(arch.read("mediumTextStyle").value<VipTextStyle>(), VipScaleDiv::MediumTick);
	value->setTextStyle(arch.read("minorTextStyle").value<VipTextStyle>(), VipScaleDiv::MinorTick);
	value->setLabelTransform(arch.read("majorTransform").value<QTransform>(), VipScaleDiv::MajorTick);
	value->setLabelTransform(arch.read("mediumTransform").value<QTransform>(), VipScaleDiv::MediumTick);
	value->setLabelTransform(arch.read("minorTransform").value<QTransform>(), VipScaleDiv::MinorTick);
	value->enableDrawTitle(arch.read("isDrawTitleEnabled").value<bool>());
	double startBorderDist = arch.read("startBorderDist").value<double>();
	double endBorderDist = arch.read("endBorderDist").value<double>();
	value->setBorderDist(startBorderDist, endBorderDist);
	double startMinBorderDist = arch.read("startMinBorderDist").value<double>();
	double endMinBorderDist = arch.read("endMinBorderDist").value<double>();
	value->setMinBorderDist(startMinBorderDist, endMinBorderDist);
	double startMaxBorderDist = arch.read("startMaxBorderDist").value<double>();
	double endMaxBorderDist = arch.read("endMaxBorderDist").value<double>();
	value->setMaxBorderDist(startMaxBorderDist, endMaxBorderDist);
	value->setMargin(arch.read("margin").value<double>());
	value->setSpacing(arch.read("spacing").value<double>());
	value->setScaleInverted(arch.read("isScaleInverted").value<bool>());
	value->setMaxMajor(arch.read("maxMajor").value<int>());
	value->setMaxMinor(arch.read("maxMinor").value<int>());

	//new in 3.0.1
	arch.save();
	bool autoExponent = false;
	int minLabelSize = 0, exponent=0;
	if (arch.content("autoExponent", autoExponent)) {
		arch.content("minLabelSize", minLabelSize);
		arch.content("exponent", exponent);
		value->scaleDraw()->valueToText()->setAutomaticExponent(autoExponent);
		value->scaleDraw()->valueToText()->setMaxLabelSize(minLabelSize);
		value->scaleDraw()->valueToText()->setExponent(exponent);
	}
	else
		arch.restore();

	value->setScaleDiv(arch.read("scaleDiv").value<VipScaleDiv>());
	value->setRenderHints((QPainter::RenderHints)arch.read("renderHints").value<int>());
	value->setVisible(arch.read("visible").toBool());
	int engine = arch.read("yScaleEngine").toInt();
	if (!value->scaleEngine() || engine != value->scaleEngine()->scaleType()) {
		if (engine == VipScaleEngine::Linear) value->setScaleEngine(new VipLinearScaleEngine());
		else if (engine == VipScaleEngine::Log10) value->setScaleEngine(new VipLog10ScaleEngine());
	}
	
	arch.resetError();

	
	arch.save();
	QString st;
	if (arch.content("styleSheet", st)) {
		if (!st.isEmpty())
			value->setStyleSheet(st);
	}
	else
		arch.restore();

	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipAxisBase * value)
{
	arch.content("isMapScaleToScene", value->isMapScaleToScene());
	arch.content("isTitleInverted", value->isTitleInverted());
	arch.content("titleInside", value->titleInside());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipAxisBase * value)
{
	value->setMapScaleToScene(arch.read("isMapScaleToScene").value<bool>());
	value->setTitleInverted(arch.read("isTitleInverted").value<bool>());
	arch.save();
	//since 2.2.18
	bool titleInside;
	if (arch.content("titleInside", titleInside))
		value->setTitleInside(titleInside);
	else
		arch.restore();
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipColorMap * value)
{
	arch.content("format", (int)value->format());
	arch.content("externalValue", (int)value->externalValue());
	arch.content("externalColor", value->externalColor());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipColorMap * value)
{
	value->setFormat((VipColorMap::Format)arch.read("format").value<int>());
	VipColorMap::ExternalValue ext_value = (VipColorMap::ExternalValue)arch.read("externalValue").value<int>();
	QRgb ext_color = arch.read("externalColor").value<int>();
	value->setExternalValue(ext_value, ext_color);
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipLinearColorMap * value)
{
	arch.content("type", (int)value->type());
	return arch.content("gradientStops", value->gradientStops());
}

VipArchive & operator>>(VipArchive & arch, VipLinearColorMap * value)
{
	value->setType((VipLinearColorMap::StandardColorMap)arch.read("type").value<int>());
	value->setGradientStops(arch.read("gradientStops").value<QGradientStops>());
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipAlphaColorMap * value)
{
	return arch.content("color", value->color());
}

VipArchive & operator>>(VipArchive & arch, VipAlphaColorMap * value)
{
	value->setColor(arch.read("color").value<QColor>());
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipAxisColorMap * value)
{
	arch.content("gripInterval", value->gripInterval());
	arch.content("colorMap", value->colorMap());
	arch.content("isColorBarEnabled", value->isColorBarEnabled());
	arch.content("colorBarWidth", value->colorBarWidth());
	arch.content("colorMapInterval", value->colorMapInterval());

	//since 2.2.18
	arch.content("hasAutoScaleMax", value->hasAutoScaleMax());
	arch.content("autoScaleMax", value->autoScaleMax());
	arch.content("hasAutoScaleMin", value->hasAutoScaleMin());
	arch.content("autoScaleMin", value->autoScaleMin());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipAxisColorMap * value)
{
	VipInterval inter = arch.read("gripInterval").value<VipInterval>();
	value->setColorMap(inter, arch.read("colorMap").value<VipColorMap*>());
	value->setGripInterval(inter);
	value->setColorBarEnabled(arch.read("isColorBarEnabled").value<bool>());
	value->setColorBarWidth(arch.read("colorBarWidth").value<double>());
	value->setColorMapInterval(arch.read("colorMapInterval").value<VipInterval>());

	//since 2.2.18
	bool hasAutoScaleMax, hasAutoScaleMin;
	vip_double autoScaleMax, autoScaleMin;
	arch.save();
	if (arch.content("hasAutoScaleMax", hasAutoScaleMax)) {
		arch.content("autoScaleMax", autoScaleMax);
		arch.content("hasAutoScaleMin", hasAutoScaleMin);
		arch.content("autoScaleMin", autoScaleMin);
		value->setHasAutoScaleMax(hasAutoScaleMax);
		value->setHasAutoScaleMin(hasAutoScaleMin);
		value->setAutoScaleMax(autoScaleMax);
		value->setAutoScaleMin(autoScaleMin);
	}
	else
		arch.restore();

	return arch;
}



VipArchive & operator<<(VipArchive & arch, const VipPlotArea2D * value)
{
	arch.content("leftAxis", value->leftAxis());
	arch.content("rightAxis", value->rightAxis());
	arch.content("topAxis", value->topAxis());
	arch.content("bottomAxis", value->bottomAxis());
	arch.content("leftAxisVisible", value->leftAxis()->isVisible());
	arch.content("rightAxisVisible", value->rightAxis()->isVisible());
	arch.content("topAxisVisible", value->topAxis()->isVisible());
	arch.content("bottomAxisVisible", value->bottomAxis()->isVisible());
	arch.content("grid", value->grid());
	arch.content("canvas", value->canvas());
	//since 2.2.18
	arch.content("title", value->titleAxis());
	return arch;
}
VipArchive & operator>>(VipArchive & arch, VipPlotArea2D * value)
{
	arch.content("leftAxis", value->leftAxis());
	arch.content("rightAxis", value->rightAxis());
	arch.content("topAxis", value->topAxis());
	arch.content("bottomAxis", value->bottomAxis());
	value->leftAxis()->setVisible(arch.read("leftAxisVisible").toBool());
	value->rightAxis()->setVisible(arch.read("rightAxisVisible").toBool());
	value->topAxis()->setVisible(arch.read("topAxisVisible").toBool());
	value->bottomAxis()->setVisible(arch.read("bottomAxisVisible").toBool());
	arch.content("grid", value->grid());
	arch.content("canvas", value->canvas());
	//since 2.2.18
	arch.save();
	if (!arch.content("title", value->titleAxis()))
		arch.restore();

	return arch;
}


#include "VipCommandOptions.h"
#include <qapplication.h>
#include <qdir.h>

static int registerStreamOperators()
{
	vipRegisterArchiveStreamOperators<VipScaleDiv>();
	vipRegisterArchiveStreamOperators<VipPlotItem*>();
	vipRegisterArchiveStreamOperators<VipPlotItemData*>();
	vipRegisterArchiveStreamOperators<VipPlotCurve*>();
	vipRegisterArchiveStreamOperators<VipPlotHistogram*>();
	vipRegisterArchiveStreamOperators<VipPlotGrid*>();
	vipRegisterArchiveStreamOperators<VipPlotCanvas*>();
	vipRegisterArchiveStreamOperators<VipPlotMarker*>();
	vipRegisterArchiveStreamOperators<VipPlotRasterData*>();
	vipRegisterArchiveStreamOperators<VipPlotSpectrogram*>();
	vipRegisterArchiveStreamOperators<VipPlotShape*>();
	vipRegisterArchiveStreamOperators<VipPlotSceneModel*>();
	vipRegisterArchiveStreamOperators<VipAbstractScale*>();
	vipRegisterArchiveStreamOperators<VipAxisBase*>();
	vipRegisterArchiveStreamOperators<VipColorMap*>();
	vipRegisterArchiveStreamOperators<VipLinearColorMap*>();
	vipRegisterArchiveStreamOperators<VipAlphaColorMap*>();
	vipRegisterArchiveStreamOperators<VipAxisColorMap*>();
	vipRegisterArchiveStreamOperators<VipPlotArea2D*>();

	//load the skin
	if (VipCommandOptions::instance().count("skin"))
	{
		QString skin = VipCommandOptions::instance().value("skin").toString();
		vipLoadSkin(skin);
	}
	else
	{
		//load the standard skin if it exists
		QString skin = "skins/" + VipCoreSettings::instance()->skin();
		if (QDir(skin).exists() && !VipCoreSettings::instance()->skin().isEmpty())
			vipLoadSkin(VipCoreSettings::instance()->skin());
		else if (QDir("skins/dark").exists())
			vipLoadSkin("dark");
	}

	return 0;
}

static int _registerStreamOperators = vipPrependInitializationFunction(registerStreamOperators);

//make the the types declared with Q_DECLARE_METATYPE are registered
static int reg1 = qMetaTypeId<VipPlotShape*>();
static int reg2 = qMetaTypeId<VipPlotShape*>();
static QVariant v = vipCreateVariant("VipLinearColorMap*");
static VipLinearColorMap * pl = v.value<VipLinearColorMap*>();
static int reg3 = qMetaTypeId<VipPlotShape*>();





QBrush vipWidgetTextBrush(QWidget * w)
{
	if (w)
		return w->palette().text();
	else
		return qApp->palette().text();
}

QColor vipDefaultTextErrorColor(QWidget * w)
{
	QColor c = vipWidgetTextBrush(w).color();
	if (c == Qt::black)
		return Qt::red;
	else if (c == Qt::white)
		return QColor(0xFF3D3D);//QColor(Qt::red).lighter(120);
	else
		return Qt::red;
}

QStringList vipAvailableSkins()
{
	QString skin = "skins";
	if (!QDir(skin).exists())
		skin = "../" + skin;
	if (QDir(skin).exists())
	{
		QStringList res = QDir(skin).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
		res.removeAll("standard_skin");
		return res;
	}
	return QStringList();
}

#include <QDesktopWidget>
#include <QApplication>

bool vipLoadSkin(const QString & skin_name)
{
	vip_debug("skin: %s\n", skin_name.toLatin1().data());
	QString skin = "skins/" + skin_name;
	if (!QDir(skin).exists())
	{
		skin = "../" + skin;
		vip_debug("cannot read skin dir, set dir to '%s'\n", skin.toLatin1().data());
	}
	if (!QDir(skin).exists())
	{
		skin = QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/skins/" + skin_name;
		vip_debug("cannot read skin dir, set dir to '%s'\n", skin.toLatin1().data());
	}

	if (QDir(skin).exists())
	{

		QFile file(skin + "/stylesheet.css");
		if (file.open(QFile::ReadOnly))
		{
			vip_debug("skin file: '%s'\n", QFileInfo(skin + "/stylesheet.css").canonicalFilePath().toLatin1().data());

			//read skin
			QString sk = file.readAll();
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
			//on linux, we might need to change the fonts sizes based on the screen size
			//QRect desktop = QApplication::desktop()->screenGeometry();
//
			// //compute the new font size for text editors
			// int size = (1080.0/desktop.height()) * 9.5;
			// if( size > 9) size =size -1;

			//int size = 12;
			// sk += "\nQPlainTextEdit{font-size: "+ QString::number(size) + "pt;}"
			// "\nQTextEdit{font-size: "+ QString::number(size) + "pt;}";
#endif

			vipAddFrontIconPath(skin + "/icons");
			qApp->setStyleSheet(sk);
			
			
			{
				// Now, read the plot_stylesheet.css file
				QFile file(skin + "/plot_stylesheet.css");
				if (file.open(QFile::ReadOnly)) {
					vip_debug("plot skin file: '%s'\n", QFileInfo(skin + "/plot_stylesheet.css").canonicalFilePath().toLatin1().data());

					// read skin
					QString sk = file.readAll();
					VipGlobalStyleSheet::setStyleSheet(sk);

					return true;
				}
				else
					vip_debug("cannot open skin file '%s'\n", (skin + "/plot_stylesheet.css").toLatin1().data());
			}
		}
		else
		{
			vip_debug("cannot open skin file '%s'\n", (skin + "/stylesheet.css").toLatin1().data());
		}

		
	}
	else
	{
		vip_debug("cannot read '%s'\n", skin.toLatin1().data());
	}

	return false;
}


static int _restart_delay = -1;

void vipSetRestartEnabled(int delay_ms)
{
	_restart_delay = delay_ms;
}
void vipDisableRestart()
{
	_restart_delay = -1;
}
bool vipIsRestartEnabled()
{
	return _restart_delay >= 0;
}
int vipRestartMSecs()
{
	return _restart_delay;
}


std::function<QString(const QString&, const QString &)> _query;
void vipSetQueryFunction(const std::function<QString(const QString&, const QString &)> & fun)
{
	_query = fun;
}
std::function<QString(const QString&, const QString &)> vipQueryFunction()
{
	return _query;
}




QImage vipRemoveColoredBorder(const QImage& img, const QColor& c, int border )
{
	// Helper function that removes the transparent border of an image while keeping at least border pixels

	const QImage im = (img.format() != QImage::Format_ARGB32) ? img.convertToFormat(QImage::Format_ARGB32) : img;
	const uint* pix = (const uint*)im.constBits();

	int minx = im.width();
	int miny = im.height();
	int maxx = -1;
	int maxy = -1;
	int h = im.height();
	int w = im.width();
	uint color = c.rgba();

	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) {
			uint p = pix[x + y * w];
			if (p != color) {
				minx = std::min(minx, x);
				maxx = std::max(maxx, x);
				miny = std::min(miny, y);
				maxy = std::max(maxy, y);
			}
		}

	minx -= border;
	miny -= border;
	maxx += border + 1;
	maxy += border + 1;


	if (minx < 0) minx = 0;
	if (miny < 0) miny = 0;
	if (maxx > w) maxx = w;
	if (maxy > h) maxy = h;

	QRect r(minx, miny, maxx - minx, maxy - miny);
	if (!r.isValid())
		return img;

	return img.copy(r);
}
QPixmap vipRemoveColoredBorder(const QPixmap& img, const QColor& c, int border )
{
	return QPixmap::fromImage(vipRemoveColoredBorder(img.toImage(), c, border));
}





#include <qsharedmemory.h>
#include <qbuffer.h>
class VipFileSharedMemory::PrivateData
{
public:
	QSharedMemory file_memory;
};

VipFileSharedMemory::VipFileSharedMemory()
{
	m_data = new PrivateData();
}

VipFileSharedMemory::~VipFileSharedMemory()
{
	delete m_data;
}

VipFileSharedMemory & VipFileSharedMemory::instance()
{
	static VipFileSharedMemory inst;
	return inst;
}

bool VipFileSharedMemory::addFilesToOpen(const QStringList & lst, bool new_workspace)
{
	if (!m_data->file_memory.isAttached())
	{
		m_data->file_memory.setKey("Thermavip_Files");
		if (!m_data->file_memory.attach())
		{
			if (!m_data->file_memory.create(200000))
				return false;
			m_data->file_memory.lock();
			memset(m_data->file_memory.data(), 0, m_data->file_memory.size());
			m_data->file_memory.unlock();
		}
	}

	QByteArray ar;
	QDataStream stream(&ar, QIODevice::WriteOnly);
	stream << new_workspace;
	stream << lst;

	if (!m_data->file_memory.lock())
		return false;

	int size = ar.size();
	memcpy(m_data->file_memory.data(), &size, sizeof(int));
	memcpy((char*)m_data->file_memory.data() + sizeof(int), ar.data(), ar.size());
	m_data->file_memory.unlock();
	return true;
}


QStringList VipFileSharedMemory::retrieveFilesToOpen(bool * new_workspace)
{
	if (!m_data->file_memory.isAttached())
	{
		m_data->file_memory.setKey("Thermavip_Files");
		if (!m_data->file_memory.attach())
		{
			if (!m_data->file_memory.create(200000))
				return QStringList();
			m_data->file_memory.lock();
			memset(m_data->file_memory.data(), 0, m_data->file_memory.size());
			m_data->file_memory.unlock();
		}
	}

	if (!m_data->file_memory.lock())
		return QStringList();

	int size = 0;
	memcpy(&size, m_data->file_memory.data(), sizeof(int));
	if (!size)
	{
		m_data->file_memory.unlock();
		return QStringList();
	}

	QByteArray ar = QByteArray::fromRawData((char*)m_data->file_memory.data() + sizeof(int), m_data->file_memory.size() - sizeof(int));
	QDataStream stream(ar);
	QStringList res;
	bool nw = false;
	stream >> nw;
	stream >> res;
	memset(m_data->file_memory.data(), 0, m_data->file_memory.size());

	m_data->file_memory.unlock();

	if (new_workspace)
		*new_workspace = nw;
	return res;
}


bool VipFileSharedMemory::hasThermavipInstance()
{
	if (!m_data->file_memory.isAttached())
	{
		m_data->file_memory.setKey("Thermavip_Files");
		return m_data->file_memory.attach();
	}
	return true;
}


#include "VipPlayer.h"
#include "VipColorMap.h"

static bool copyPath(QString src, QString dst)
{
	QDir dir(src);
	if (!dir.exists())
		return false;

	foreach(QString d, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		QString dst_path = dst + "/" + d;
		QDir().mkpath(dst_path);
		if (!copyPath(src + "/" + d, dst_path))
			return false;
	}

	foreach(QString f, dir.entryList(QDir::Files)) {
		QString srcfile = src + "/" + f;
		QString dstfile = dst + "/" + f;
		if (QFileInfo(dstfile).exists())
			if (!QFile::remove(dstfile))
				return false;

		if (!QFile::copy(srcfile, dstfile))
			return false;
	}
	return true;
}


class VipGuiDisplayParamaters::PrivateData
{
public:
	QFont editorFont;
	int itemPaletteFactor;
	bool videoPlayerShowAxis;
	bool displayTimeOffset;
	bool showPlayerToolBar;
	bool showTimeMarkerAlways;
	bool globalColorScale;
	int flatHistogramStrength;
	int videoRenderingStrategy;
	int plotRenderingStrategy;
	int renderingThreads;
	bool displayExactPixels;
	bool dirty;
	bool setAndApply;
	VipLinearColorMap::StandardColorMap playerColorScale;
	Vip::PlayerLegendPosition legendPosition;
	VipValueToTime::DisplayType displayType;
	QPointer<VipPlotWidget2D> resetPlotWidget;
	QPointer<VipPlotWidget2D> defaultPlotWidget;
	QPointer<VipPlotArea2D> defaultArea;
	QPointer<VipPlotCurve> defaultCurve;

	QSharedPointer<VipTextStyle> titleTextStyle;
	QSharedPointer<VipTextStyle> defaultTextStyle;

	QPen shapePen;
	QBrush shapeBrush;
	VipPlotShape::DrawComponents shapeComponents;
};

VipGuiDisplayParamaters::VipGuiDisplayParamaters(VipMainWindow * win)
{
	m_data = new PrivateData();
	m_data->setAndApply = true;

	m_data->shapePen = QPen(QColor(Qt::black),1);
	m_data->shapeBrush = QBrush(QColor(255, 0, 0, 70));
	m_data->shapeComponents = VipPlotShape::Background | VipPlotShape::Border | VipPlotShape::Id;

	m_data->itemPaletteFactor = 0;
	m_data->playerColorScale = VipLinearColorMap::Jet;
	m_data->videoPlayerShowAxis = true;
	m_data->displayTimeOffset = false;
	m_data->legendPosition = Vip::LegendBottom;
	m_data->showPlayerToolBar = true;
	m_data->showTimeMarkerAlways = false;
	m_data->globalColorScale = false;
	m_data->flatHistogramStrength = 1;
	m_data->dirty = false;
	m_data->displayType = VipValueToTime::Double;

	m_data->videoRenderingStrategy = DirectRendering;
	m_data->plotRenderingStrategy = DirectRendering;
	m_data->renderingThreads = 1;
	m_data->displayExactPixels = false;

	m_data->defaultPlotWidget = new VipPlotWidget2D(win);
	m_data->defaultPlotWidget->hide();
	m_data->resetPlotWidget = new VipPlotWidget2D(win);
	m_data->resetPlotWidget->hide();
	m_data->defaultArea = m_data->defaultPlotWidget->area();//new VipPlotArea2D();
	m_data->defaultArea->setVisible(true);
	m_data->defaultArea->grid()->setVisible(false);
	//m_data->defaultArea->titleAxis()->setVisible(true);
	m_data->defaultCurve = new VipPlotCurve();
	m_data->defaultCurve->setPen(QPen(Qt::blue,1.5));
	m_data->defaultCurve->setBrush(QBrush(QColor(0, 0, 255, 200), Qt::NoBrush));
	m_data->defaultCurve->setRawData(VipPointVector() << QPointF(3, 3) << QPointF(6, 6) << QPointF(9, 4) << QPointF(12, 7));
	VipSymbol * s = new VipSymbol();
	s->setSize(QSizeF(9, 9));
	s->setStyle(VipSymbol::Ellipse);
	s->setBrush(QBrush(Qt::blue));
	s->setPen(QPen(QColor(Qt::blue).darker(120)));
	m_data->defaultCurve->setSymbol(s);

#ifdef Q_OS_WIN
	m_data->editorFont.setFixedPitch(true);
	m_data->editorFont.setFamily("Consolas");
	m_data->editorFont.setPointSize(10);
#else
	//Use a font embeded within Thermavip
	m_data->editorFont.setFixedPitch(true);
	m_data->editorFont.setFamily("Inconsolata");
	m_data->editorFont.setPointSize(13);
#endif

	connect(this, SIGNAL(changed()), this, SLOT(delaySaveToFile()), Qt::QueuedConnection);

	//use the one in Thermavip installation if more recent
	QFileInfo current = vipGetDataDirectory() + "gui_settings.xml";
	QString apppath = QFileInfo(vipAppCanonicalPath()).canonicalPath();
	apppath.replace("\\", "/");
	if (!apppath.endsWith("/")) apppath += "/";
	QFileInfo thermavip = apppath + "gui_settings.xml";
	//printf("current: %s\n", current.canonicalFilePath().toLatin1().data());
	//printf("thermavip: %s, %s\n", thermavip.canonicalFilePath().toLatin1().data(), (apppath + "gui_settings.xml").toLatin1().data());
	if (thermavip.exists() && (!current.exists() || current.lastModified() < thermavip.lastModified())) {
		if (current.exists()) {
			if (!QFile::remove(current.canonicalFilePath()))
				return;
		}
		QFile::copy(thermavip.canonicalFilePath(), current.canonicalFilePath());
	}

	m_data->setAndApply = false;
	restore();
	m_data->setAndApply = true;
}

VipGuiDisplayParamaters::~VipGuiDisplayParamaters()
{
	if (m_data->defaultArea)
		delete m_data->defaultArea.data();
	if (m_data->defaultCurve)
		delete m_data->defaultCurve.data();
	delete m_data;
}

VipGuiDisplayParamaters * VipGuiDisplayParamaters::instance(VipMainWindow * win)
{
	static VipGuiDisplayParamaters *inst = new VipGuiDisplayParamaters(win);
	return inst;
}

QPen VipGuiDisplayParamaters::shapeBorderPen()
{
	return m_data->shapePen;
}
QBrush VipGuiDisplayParamaters::shapeBackgroundBrush()
{
	return m_data->shapeBrush;
}
VipPlotShape::DrawComponents VipGuiDisplayParamaters::shapeDrawComponents()
{
	return m_data->shapeComponents;
}

void VipGuiDisplayParamaters::setShapeBorderPen(const QPen & pen)
{
	m_data->shapePen = pen;
	if (m_data->setAndApply) {

		QList<VipPlayer2D*> players = VipUniqueId::objects<VipPlayer2D>();
		for (int i = 0; i < players.size(); ++i) {
			QList<VipPlotSceneModel*> models = players[i]->plotSceneModels();
			for (int j = 0; j < models.size(); ++j) {
				models[j]->setPen("All", pen);
			}
		}
	}
	emitChanged();
}
void VipGuiDisplayParamaters::setShapeBackgroundBrush(const QBrush & brush)
{
	m_data->shapeBrush = brush;
	if (m_data->setAndApply) {
		QList<VipPlayer2D*> players = VipUniqueId::objects<VipPlayer2D>();
		for (int i = 0; i < players.size(); ++i) {
			QList<VipPlotSceneModel*> models = players[i]->plotSceneModels();
			for (int j = 0; j < models.size(); ++j) {
				models[j]->setBrush("All", brush);
			}
		}
	}
	emitChanged();
}
void VipGuiDisplayParamaters::setShapeDrawComponents(const VipPlotShape::DrawComponents & c)
{
	m_data->shapeComponents = c;
	if (m_data->setAndApply) {
		QList<VipPlayer2D*> players = VipUniqueId::objects<VipPlayer2D>();
		for (int i = 0; i < players.size(); ++i) {
			QList<VipPlotSceneModel*> models = players[i]->plotSceneModels();
			for (int j = 0; j < models.size(); ++j) {
				models[j]->setDrawComponents("All", c);
			}
		}
	}
	emitChanged();
}

int VipGuiDisplayParamaters::itemPaletteFactor() const
{
	return m_data->itemPaletteFactor;
}
void VipGuiDisplayParamaters::setItemPaletteFactor(int factor)
{
	m_data->itemPaletteFactor = factor;
	
	// retrieve the color palette from the global style sheet
	QVariant palette = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractPlotArea", "colorpalette");
	if (palette.isNull())
		return;

	if (palette.userType() == QMetaType::Int) {
		// enum value
		VipLinearColorMap::StandardColorMap map = (VipLinearColorMap::StandardColorMap)palette.toInt();
		QByteArray name = VipLinearColorMap::colorMapToName(map);
		name += ":" + QByteArray::number(factor);
		VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractPlotArea", "colorpalette", QVariant::fromValue(name));
	}
	else {
		QByteArray name = palette.toByteArray();
		if (!name.contains(":"))
			name += ":" + QByteArray::number(factor);
		else {
			QList<QByteArray> lst = name.split(':');
			name = lst[0] + ":" + QByteArray::number(factor);
		}
		VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractPlotArea", "colorpalette", QVariant::fromValue(name));
	}
		
	if (m_data->setAndApply) {
		// apply palette
		QList<VipAbstractPlayer*> players = VipUniqueId::objects<VipAbstractPlayer>();
		for (int i = 0; i < players.size(); ++i)
			players[i]->update();
	}
	emitChanged();
}

bool VipGuiDisplayParamaters::videoPlayerShowAxes() const
{
	return m_data->videoPlayerShowAxis;
}
void VipGuiDisplayParamaters::setVideoPlayerShowAxes(bool enable)
{
	if (enable != m_data->videoPlayerShowAxis)
	{
		m_data->videoPlayerShowAxis = enable;
		if (m_data->setAndApply) {
			QList<VipVideoPlayer*> players = VipUniqueId::objects<VipVideoPlayer>();
			for (int i = 0; i < players.size(); ++i) {
				players[i]->showAxes(enable);
			}
		}
		emitChanged();
	}
}

Vip::PlayerLegendPosition VipGuiDisplayParamaters::legendPosition() const
{
	return m_data->legendPosition;
}

void VipGuiDisplayParamaters::setLegendPosition(Vip::PlayerLegendPosition  pos)
{
	if (pos != m_data->legendPosition)
	{
		m_data->legendPosition = pos;
		if (m_data->setAndApply) {
			QList<VipPlotPlayer*> players = VipUniqueId::objects<VipPlotPlayer>();
			for (int i = 0; i < players.size(); ++i)
				players[i]->setLegendPosition(pos);
		}
		emitChanged();
	}
}

void VipGuiDisplayParamaters::setAlwaysShowTimeMarker(bool enable)
{
	if (enable != m_data->showTimeMarkerAlways) {
		m_data->showTimeMarkerAlways = enable;
		if (m_data->setAndApply) {
			QList<VipPlotPlayer*> players = VipUniqueId::objects<VipPlotPlayer>();
			for (int i = 0; i < players.size(); ++i)
				players[i]->setTimeMarkerAlwaysVisible(enable);
		}
	}
	emitChanged();
}

void VipGuiDisplayParamaters::setPlotTitleInside(bool enable)
{
	if (m_data->defaultArea->titleAxis()->titleInside() != enable) {
		m_data->defaultArea->titleAxis()->setTitleInside(enable);
		if (m_data->setAndApply) {
			QList<VipPlotPlayer*> players = VipUniqueId::objects<VipPlotPlayer>();
			for (int i = 0; i < players.size(); ++i)
				players[i]->plotWidget2D()->area()->titleAxis()->setTitleInside(enable);
		}
	}

	emitChanged();
}
void VipGuiDisplayParamaters::setPlotGridVisible(bool visible)
{
	if (m_data->defaultArea->grid()->isVisible() != visible) {
		m_data->defaultArea->grid()->setVisible(visible);
		if (m_data->setAndApply) {
			QList<VipPlotPlayer*> players = VipUniqueId::objects<VipPlotPlayer>();
			for (int i = 0; i < players.size(); ++i)
				players[i]->showGrid(visible);
		}
	}

	emitChanged();
}

void VipGuiDisplayParamaters::setGlobalColorScale(bool enable)
{
	if (m_data->globalColorScale != enable) {
		m_data->globalColorScale = enable;
		if (m_data->setAndApply) {
			VipDisplayArea* a = vipGetMainWindow()->displayArea();
			for (int i = 0; i < a->count(); ++i)
				a->widget(i)->setUseGlobalColorMap(enable);
		}
	}

	emitChanged();
}

void VipGuiDisplayParamaters::setFlatHistogramStrength(int strength)
{
	if (m_data->flatHistogramStrength != strength) {
		m_data->flatHistogramStrength = strength;
		QList<VipVideoPlayer*> players = VipUniqueId::objects<VipVideoPlayer>();
		if (m_data->setAndApply) {
			for (int i = 0; i < players.size(); ++i)
				players[i]->setFlatHistogramStrength(strength);

			VipDisplayArea* a = vipGetMainWindow()->displayArea();
			for (int i = 0; i < a->count(); ++i)
				a->widget(i)->colorMapAxis()->setFlatHistogramStrength(strength);
		}
	}

	emitChanged();
}

void VipGuiDisplayParamaters::autoScaleAll()
{
	if (m_data->setAndApply) {
		QList<VipPlotPlayer*> lst = vipListCast<VipPlotPlayer*>(VipPlayerLifeTime::instance()->players());
		for (int i = 0; i < lst.size(); ++i) {
			lst[i]->setAutoScale(true);
		}
	}
}

VipPlotArea2D * VipGuiDisplayParamaters::defaultPlotArea() const
{
	if (!m_data->defaultArea->isVisible())
		m_data->defaultArea->setVisible(true);
	return m_data->defaultArea;
}
VipPlotCurve * VipGuiDisplayParamaters::defaultCurve() const
{
	if (!m_data->defaultCurve->isVisible())
		m_data->defaultCurve->setVisible(true);
	return m_data->defaultCurve;
}

#include "VipMultiPlotWidget2D.h"

void VipGuiDisplayParamaters::applyDefaultPlotArea(VipPlotArea2D * area)
{
	if (!area)
		return;
	if (VipVMultiPlotArea2D * a = qobject_cast<VipVMultiPlotArea2D*>(area)) {
		a->leftMultiAxis()->setVisible(defaultPlotArea()->leftAxis()->isVisible());
		a->rightMultiAxis()->setVisible(defaultPlotArea()->rightAxis()->isVisible());
	}
	else {
		area->leftAxis()->setVisible(defaultPlotArea()->leftAxis()->isVisible());
		area->rightAxis()->setVisible(defaultPlotArea()->rightAxis()->isVisible());
	}

	area->topAxis()->setVisible(defaultPlotArea()->topAxis()->isVisible());
	area->titleAxis()->setTitleInside(defaultPlotArea()->titleAxis()->titleInside());
	area->titleAxis()->setVisible(defaultPlotArea()->titleAxis()->isVisible());
	area->bottomAxis()->setVisible(defaultPlotArea()->bottomAxis()->isVisible());

	QList<VipPlotGrid*> grids = area->findItems<VipPlotGrid*>();
	for (int i = 0; i < grids.size(); ++i) {
		grids[i]->enableAxis(0, defaultPlotArea()->grid()->axisEnabled(0));
		grids[i]->enableAxis(1, defaultPlotArea()->grid()->axisEnabled(1));
		grids[i]->enableAxisMin(0, defaultPlotArea()->grid()->axisMinEnabled(0));
		grids[i]->enableAxisMin(1, defaultPlotArea()->grid()->axisMinEnabled(1));
		grids[i]->setMajorPen(defaultPlotArea()->grid()->majorPen());
		grids[i]->setMinorPen(defaultPlotArea()->grid()->minorPen());
		grids[i]->setVisible(defaultPlotArea()->grid()->isVisible());
	}
	QList<VipPlotCanvas*> canvas = area->findItems<VipPlotCanvas*>();
	for (int i = 0; i < canvas.size(); ++i)
		canvas[i]->boxStyle().setBackgroundBrush(defaultPlotArea()->canvas()->boxStyle().backgroundBrush());

	QList<VipPlotCurve*> curves = area->findItems<VipPlotCurve*>();
	for (int i = 0; i < curves.size(); ++i)
		applyDefaultCurve(curves[i]);
}

void VipGuiDisplayParamaters::applyDefaultCurve(VipPlotCurve * c)
{
	//apply the curve parameters, but keep the pen and brush color unchanged, as well as the symbol colors
	QColor border = c->pen().color();
	QColor brush = c->brush().color();

	QColor s_border = c->symbol() ? c->symbol()->pen().color() : QColor();
	QColor s_brush = c->symbol() ? c->symbol()->brush().color() : QColor();

	c->setRenderHints(defaultCurve()->renderHints());
	c->setStyle(defaultCurve()->style());
	c->setPen(defaultCurve()->pen());
	c->setBrush(defaultCurve()->brush());
	c->setSymbolVisible(defaultCurve()->symbolVisible());
	//c->setBaseline(defaultCurve()->baseline());
	if (defaultCurve()->symbol())
		c->setSymbol(new VipSymbol(*defaultCurve()->symbol()));

	//reset colors
	c->setPenColor(border);
	c->setBrushColor(brush);
	if (c->symbol()) {
		c->symbol()->setPenColor(s_border);
		c->symbol()->setBrushColor(s_brush);
	}

	//TODO: reapply stylesheet
	
}

bool VipGuiDisplayParamaters::displayTimeOffset() const
{
	return m_data->displayTimeOffset;
}
void VipGuiDisplayParamaters::setDisplayTimeOffset(bool enable)
{
	m_data->displayTimeOffset = enable;
	emitChanged();
}

VipValueToTime::DisplayType VipGuiDisplayParamaters::displayType() const
{
	return m_data->displayType;
}
void VipGuiDisplayParamaters::setDisplayType(VipValueToTime::DisplayType type)
{
	m_data->displayType = type;
	emitChanged();
}

QFont VipGuiDisplayParamaters::defaultEditorFont() const { return m_data->editorFont; }
bool VipGuiDisplayParamaters::alwaysShowTimeMarker() const { return m_data->showTimeMarkerAlways; }
bool VipGuiDisplayParamaters::globalColorScale() const { return m_data->globalColorScale; }

void VipGuiDisplayParamaters::setDefaultEditorFont(const QFont & font)
{
	m_data->editorFont = font;
	emitChanged();
}

bool VipGuiDisplayParamaters::titleVisible() const
{
	return m_data->defaultArea->titleAxis()->isVisible();
}

void VipGuiDisplayParamaters::setTitleVisible(bool vis)
{
	m_data->defaultArea->titleAxis()->setVisible(vis);
	if (m_data->setAndApply) {
		QList<VipPlayer2D*> pls = vipListCast<VipPlayer2D*>(VipPlayerLifeTime::instance()->players());
		for (int i = 0; i < pls.size(); ++i) {
			pls[i]->plotWidget2D()->area()->titleAxis()->setVisible(vis);
		}
	}
	emitChanged();
}



VipTextStyle VipGuiDisplayParamaters::titleTextStyle() const
{
	if (!m_data->titleTextStyle) {

		m_data->titleTextStyle.reset(new VipTextStyle(m_data->resetPlotWidget->area()->titleAxis()->title().textStyle()));
		// use global style sheet
		QVariant color = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractPlotArea", "title-color");
		if (!color.isNull())
			m_data->titleTextStyle->setTextPen(QPen(color.value<QColor>()));
		QVariant font = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractPlotArea", "title-font");
		if (!font.isNull())
			m_data->titleTextStyle->setFont((font.value<QFont>()));
	}
	return *m_data->titleTextStyle;
}

void VipGuiDisplayParamaters::setTitleTextStyle(const VipTextStyle & style)
{
	m_data->titleTextStyle.reset(new VipTextStyle(style));

	
	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractPlotArea", "title-color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractPlotArea", "title-font", QVariant::fromValue(style.font()));

	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractScale", "title-color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractScale", "title-font", QVariant::fromValue(style.font()));

	VipGlobalStyleSheet::styleSheet().setProperty("VipPlotItem", "title-color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipPlotItem", "title-font", QVariant::fromValue(style.font()));

	if (m_data->setAndApply) 
		vipGetMainWindow()->update();
	

	emitChanged();
}

void VipGuiDisplayParamaters::setTitleTextStyle2(const VipText & text)
{
	setTitleTextStyle(text.textStyle());
}

void VipGuiDisplayParamaters::setDefaultTextStyle(const VipTextStyle & style)
{
	m_data->defaultTextStyle.reset(new VipTextStyle(style));

	VipGlobalStyleSheet::styleSheet().setProperty("VipLegend", "color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipLegend", "font", QVariant::fromValue(style.font()));

	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractScale", "label-color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractScale", "label-font", QVariant::fromValue(style.font()));

	VipGlobalStyleSheet::styleSheet().setProperty("VipPlotItem", "color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipPlotItem", "font", QVariant::fromValue(style.font()));

	if (m_data->setAndApply) 
		vipGetMainWindow()->update();

	emitChanged();
}
void VipGuiDisplayParamaters::setDefaultTextStyle2(const VipText & t)
{
	setDefaultTextStyle(t.textStyle());
}
VipTextStyle VipGuiDisplayParamaters::defaultTextStyle() const
{
	if (!m_data->defaultTextStyle) {

		m_data->defaultTextStyle.reset(new VipTextStyle(m_data->resetPlotWidget->area()->titleAxis()->title().textStyle()));
		// use global style sheet
		QVariant color = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractScale", "label-color");
		if (!color.isNull())
			m_data->defaultTextStyle->setTextPen(QPen(color.value<QColor>()));
		QVariant font = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractPlotArea", "label-font");
		if (!font.isNull())
			m_data->defaultTextStyle->setFont((font.value<QFont>()));
	}
	return *m_data->defaultTextStyle;
}

QColor VipGuiDisplayParamaters::defaultPlayerTextColor() const
{
	QVariant v = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractPlotArea", "title-color");
	if (v.isNull())
		return Qt::black;
	return v.value<QColor>();
	//return m_data->resetPlotWidget->titlePen().color();
}

QColor VipGuiDisplayParamaters::defaultPlayerBackgroundColor() const
{
	return m_data->resetPlotWidget->backgroundColor();
}

bool VipGuiDisplayParamaters::hasTitleTextStyle() const
{
	return m_data->titleTextStyle;
}
bool VipGuiDisplayParamaters::hasDefaultTextStyle() const
{
	return m_data->defaultTextStyle;
}

int VipGuiDisplayParamaters::flatHistogramStrength() const
{
	return m_data->flatHistogramStrength;
}

VipLinearColorMap::StandardColorMap VipGuiDisplayParamaters::playerColorScale() const
{
	return m_data->playerColorScale;
}
void VipGuiDisplayParamaters::setPlayerColorScale(VipLinearColorMap::StandardColorMap map)
{
	if (m_data->setAndApply) {
		// apply color scale to video players
		QList<VipVideoPlayer*> vplayers = VipUniqueId::objects<VipVideoPlayer>();
		for (int i = 0; i < vplayers.size(); ++i) {
			if (vplayers[i]->spectrogram()->colorMap() && qobject_cast<VipLinearColorMap*>(vplayers[i]->spectrogram()->colorMap()->colorMap()) &&
			    static_cast<VipLinearColorMap*>(vplayers[i]->spectrogram()->colorMap()->colorMap())->type() == m_data->playerColorScale) {
				bool flat_hist = vplayers[i]->spectrogram()->colorMap()->useFlatHistogram();
				int str = vplayers[i]->spectrogram()->colorMap()->flatHistogramStrength();
				vplayers[i]->spectrogram()->colorMap()->setColorMap(vplayers[i]->spectrogram()->colorMap()->gripInterval(), VipLinearColorMap::createColorMap(map));
				vplayers[i]->spectrogram()->colorMap()->setUseFlatHistogram(flat_hist);
				vplayers[i]->spectrogram()->colorMap()->setFlatHistogramStrength(str);
			}
		}

		// apply to workspace
		VipDisplayArea* d = vipGetMainWindow()->displayArea();
		for (int i = 0; i < d->count(); ++i) {
			d->widget(i)->setColorMap(map);
			d->widget(i)->colorMapAxis()->setFlatHistogramStrength(flatHistogramStrength());
		}
	}
	m_data->playerColorScale = map;
	emitChanged();
}


int VipGuiDisplayParamaters::videoRenderingStrategy() const
{
	return m_data->videoRenderingStrategy;
}
int VipGuiDisplayParamaters::plotRenderingStrategy() const
{
	return m_data->plotRenderingStrategy;
}
int VipGuiDisplayParamaters::renderingThreads() const
{
	return m_data->renderingThreads;
}

void VipGuiDisplayParamaters::setVideoRenderingStrategy(int st)
{
	if (st != m_data->videoRenderingStrategy)
	{
		m_data->videoRenderingStrategy = st;
		if (m_data->setAndApply) {
			QList<VipVideoPlayer*> pls = vipListCast<VipVideoPlayer*>(VipPlayerLifeTime::instance()->players());
			for (int i = 0; i < pls.size(); ++i) {
				pls[i]->plotWidget2D()->setOpenGLRendering(st == PureOpenGL);
				if (st == DirectRendering || st == PureOpenGL)
					pls[i]->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::Default);
				else if (st == OffscreenOpenGL)
					pls[i]->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::OpenGLOffscreen);
				else
					pls[i]->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::AutoStrategy);
			}
		}
		emitChanged();
	}
}
void VipGuiDisplayParamaters::setPlotRenderingStrategy(int st)
{
	if (st != m_data->plotRenderingStrategy)
	{
		m_data->plotRenderingStrategy = st;
		if (m_data->setAndApply) {
			QList<VipPlotPlayer*> pls = vipListCast<VipPlotPlayer*>(VipPlayerLifeTime::instance()->players());
			for (int i = 0; i < pls.size(); ++i) {
				pls[i]->plotWidget2D()->setOpenGLRendering(st == PureOpenGL);
				if (st == DirectRendering || st == PureOpenGL)
					pls[i]->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::Default);
				else if (st == OffscreenOpenGL)
					pls[i]->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::OpenGLOffscreen);
				else
					pls[i]->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::AutoStrategy);
			}
		}
		emitChanged();
	}
}
void VipGuiDisplayParamaters::setRenderingThreads(int threads)
{
	if (threads != m_data->renderingThreads)
	{
		m_data->renderingThreads = threads;
		VipAbstractPlotArea::setRenderingThreads(threads);
		emitChanged();
	}
}

void VipGuiDisplayParamaters::setDisplayExactPixels(bool enable)
{
	if (enable != m_data->displayExactPixels)
	{
		m_data->displayExactPixels = enable;
		emitChanged();
	}
}
bool VipGuiDisplayParamaters::displayExactPixels() const
{
	return m_data->displayExactPixels;
}

void VipGuiDisplayParamaters::apply(QWidget * w)
{
	if (VipAbstractPlayer * pl = qobject_cast<VipAbstractPlayer*>(w)) {
		
		
		if (VipVideoPlayer * v = qobject_cast<VipVideoPlayer*>(pl)) {
			if (v->spectrogram()->colorMap() && qobject_cast<VipLinearColorMap*>(v->spectrogram()->colorMap()->colorMap())
				&& static_cast<VipLinearColorMap*>(v->spectrogram()->colorMap()->colorMap())->type() == m_data->playerColorScale) {
				bool flat_hist = v->spectrogram()->colorMap()->useFlatHistogram();
				v->spectrogram()->colorMap()->setColorMap(v->spectrogram()->colorMap()->gripInterval(), VipLinearColorMap::createColorMap(playerColorScale()));
				v->spectrogram()->colorMap()->setUseFlatHistogram(flat_hist);
			}

			v->showAxes(videoPlayerShowAxes());
			v->plotWidget2D()->area()->titleAxis()->setVisible(defaultPlotArea()->titleAxis()->isVisible());
			v->setFlatHistogramStrength(flatHistogramStrength());

			v->plotWidget2D()->setOpenGLRendering(m_data->videoRenderingStrategy == PureOpenGL);
			if (m_data->videoRenderingStrategy == DirectRendering || m_data->videoRenderingStrategy == PureOpenGL)
				v->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::Default);
			else if (m_data->videoRenderingStrategy == OffscreenOpenGL)
				v->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::OpenGLOffscreen);
			else
				v->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::AutoStrategy);
		}

		if (VipPlotPlayer * p = qobject_cast<VipPlotPlayer*>(pl)) {
			p->setLegendPosition(this->legendPosition());
			//p->valueToTimeButton()->setDisplayTimeOffset(m_data->displayTimeOffset);
			p->setTimeMarkerAlwaysVisible(alwaysShowTimeMarker());
			p->plotWidget2D()->setOpenGLRendering(m_data->plotRenderingStrategy == PureOpenGL);
			if (m_data->plotRenderingStrategy == DirectRendering || m_data->plotRenderingStrategy == PureOpenGL)
				p->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::Default);
			else if (m_data->plotRenderingStrategy == OffscreenOpenGL)
				p->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::OpenGLOffscreen);
			else
				p->plotWidget2D()->area()->setRenderStrategy(VipAbstractPlotArea::AutoStrategy);
			applyDefaultPlotArea(qobject_cast<VipPlotArea2D*>(p->plotWidget2D()->area()));
		}
	}

}

void VipGuiDisplayParamaters::reset()
{
	blockSignals(true);
	setItemPaletteFactor(0);
	setPlayerColorScale(VipLinearColorMap::Jet);
	setLegendPosition(Vip::LegendBottom);
	setVideoPlayerShowAxes(true);
	setDisplayTimeOffset(false);
	setDisplayType(VipValueToTime::Double);
	setAlwaysShowTimeMarker(false);
	setGlobalColorScale(false);
	setPlotGridVisible(true);
	setPlotTitleInside(false);
	setFlatHistogramStrength(1);
	setVideoRenderingStrategy(DirectRendering);
	setPlotRenderingStrategy(DirectRendering);
	setRenderingThreads(1);
	//setTitleTextStyle(VipTextStyle());
	//setDefaultTextStyle(VipTextStyle());
	m_data->titleTextStyle.reset();
	m_data->defaultTextStyle.reset();
	m_data->defaultPlotWidget->setStyleSheet(QString());
	QList<VipPlayer2D*> pls = vipListCast<VipPlayer2D*>(VipPlayerLifeTime::instance()->players());
	for (int i = 0; i < pls.size(); ++i)
		pls[i]->setStyleSheet(QString());

	setShapeBorderPen( QPen(QColor(Qt::black), 1));
	setShapeBackgroundBrush( QBrush(QColor(255, 0, 0, 70)));
	setShapeDrawComponents( VipPlotShape::Background | VipPlotShape::Border | VipPlotShape::Id);


	blockSignals(false);
	emitChanged();
}


bool  VipGuiDisplayParamaters::save(const QString & file)
{
	QString fname = file;
	if (fname.isEmpty())
		fname = vipGetDataDirectory() + "gui_settings.xml";
	VipXOfArchive ar;
	if (!ar.open(fname))
		return false;
	return save(ar);
}

bool  VipGuiDisplayParamaters::restore(const QString & file)
{
	QString fname = file;
	if (fname.isEmpty())
		fname = vipGetDataDirectory() + "gui_settings.xml";
	VipXIfArchive ar;
	if (!ar.open(fname))
		return false;
	return restore(ar);
}

void VipGuiDisplayParamaters::emitChanged()
{
	m_data->dirty = true;
	Q_EMIT changed();
}
void VipGuiDisplayParamaters::delaySaveToFile()
{
	if (m_data->dirty) {
		m_data->dirty = false;
		save();
	}
}




static QString _skin;

static void serialize_VipGuiDisplayParamaters(VipGuiDisplayParamaters * inst, VipArchive & arch)
{
	if (arch.mode() == VipArchive::Read)
	{
		arch.save();
		if (arch.start("VipGuiDisplayParamaters"))
		{
			arch.save();
			QString version = arch.read("version").toString();
			if (version.isEmpty())
				arch.restore();

			//check version
			QStringList vers = version.split(".", QString::SkipEmptyParts);
			QList<int> ivers;
			for (int i = 0; i < vers.size(); ++i) ivers.append(vers[i].toInt());

			arch.save();
			QString skin;
			bool diff_skin = true;
			if (arch.content("skin", skin)) {
				//printf("current: %s, read: %s\n", VipCoreSettings::instance()->skin().toLatin1().data(), skin.toLatin1().data());
				diff_skin = VipCoreSettings::instance()->skin() != skin;
				_skin = VipCoreSettings::instance()->skin();
			}
			else
				arch.restore();

			inst->setItemPaletteFactor(arch.read("itemPaletteFactor").value<int>());
			if (ivers < (QList<int>() << 3 << 3 << 4)) {
				inst->setPlayerColorScale(VipLinearColorMap::Jet);
			}
			else
				inst->setPlayerColorScale(VipLinearColorMap::StandardColorMap(arch.read("playerColorScale").value<int>()));

			bool show_axes = arch.read("video_player_axes").toBool();
			if (arch)
				inst->setVideoPlayerShowAxes(show_axes);
			else
				arch.resetError();

			//new in 2.2.17
			arch.save();
			int legendPosition = 1;
			arch.content("legendPosition", legendPosition);
			if (!arch)
				arch.restore();
			else {
				inst->setLegendPosition((Vip::PlayerLegendPosition)legendPosition);
			}

			arch.content("defaultPlotArea", inst->defaultPlotArea());
			arch.content("defaultCurve", inst->defaultCurve());
			inst->setDisplayTimeOffset(arch.read("displayTimeOffset").toBool());
			inst->setDisplayType((VipValueToTime::DisplayType)arch.read("displayType").toInt());

			if (ivers < (QList<int>() << 3 << 3 << 5)) {
				//starting version 3.3.5, the default pen width for curves is 1.5
				QPen p = inst->defaultCurve()->pen();
				p.setWidthF(1.5);
				inst->defaultCurve()->setPen(p);
				//hide grid
				inst->defaultPlotArea()->grid()->setVisible(false);
			}

			//new in 2.2.18
			arch.save();
			bool showTimeMarkerAlways;
			if (arch.content("showTimeMarkerAlways", showTimeMarkerAlways))
				inst->setAlwaysShowTimeMarker(showTimeMarkerAlways);
			else {
				arch.restore();
				showTimeMarkerAlways = false;
				inst->setAlwaysShowTimeMarker(false);
			}
			//printf("read showTimeMarkerAlways: %i\n", (int)showTimeMarkerAlways);
			arch.save();
			bool globalColorScale;
			if (arch.content("globalColorScale", globalColorScale))
				inst->setGlobalColorScale(globalColorScale);
			else {
				arch.restore();
				globalColorScale = false;
				inst->setGlobalColorScale(false);
			}
			//printf("read globalColorScale: %i\n", (int)globalColorScale);


			//new in 3.3.3
			arch.save();
			int flatHistogramStrength;
			if (arch.content("flatHistogramStrength", flatHistogramStrength))
				inst->setFlatHistogramStrength(flatHistogramStrength);
			else {
				arch.restore();
				inst->setFlatHistogramStrength(1);
			}

			//new in 3.4.7
			arch.save();
			int videoRenderingStrategy,plotRenderingStrategy,renderingThreads;
			arch.content("videoRenderingStrategy", videoRenderingStrategy);
			arch.content("plotRenderingStrategy", plotRenderingStrategy);
			arch.content("renderingThreads", renderingThreads);
			if (arch) {
				inst->setVideoRenderingStrategy(videoRenderingStrategy);
				inst->setPlotRenderingStrategy(plotRenderingStrategy);
				inst->setRenderingThreads(renderingThreads);
			}
			else {
				arch.restore();
				inst->setVideoRenderingStrategy(VipGuiDisplayParamaters::DirectRendering);
				inst->setPlotRenderingStrategy(VipGuiDisplayParamaters::DirectRendering);
				inst->setRenderingThreads(1);
			}

			arch.save();
			VipTextStyle titleTextStyle;
			if (!diff_skin && arch.content("titleTextStyle", titleTextStyle))
				inst->setTitleTextStyle(titleTextStyle);
			else
				arch.restore();

			arch.save();
			VipTextStyle defaultTextStyle;
			if (!diff_skin && arch.content("defaultTextStyle", defaultTextStyle))
				inst->setDefaultTextStyle(defaultTextStyle);
			else
				arch.restore();


			//new in 3.0.2
			arch.save();
			QBrush b; QPen p; int c;
			if (arch.content("backgroundBrush", b)) {
				arch.content("borderPen", p);
				arch.content("drawComponents", c);
				inst->setShapeBackgroundBrush(b);
				inst->setShapeBorderPen(p);
				inst->setShapeDrawComponents((VipPlotShape::DrawComponents)c);
			}
			else
				arch.restore();


			// New in 3.11.0
			arch.save();
			int displayExactPixels = 0;
			if (arch.content("displayExactPixels", displayExactPixels)) {
				inst->setDisplayExactPixels(displayExactPixels != 0);
			}
			else
				arch.restore();

			arch.end();

			//Force some parameters for old version

			//check version
			if (ivers < (QList<int>() << 3 << 0 << 2)) {
				//make ROI more transparent
				//QBrush b = VipDefaultSceneModelDisplayOptions::backgroundBrush();
				// QColor c = b.color();
				// c.setAlpha(70);
				// b.setColor(c);
				// VipDefaultSceneModelDisplayOptions::setBackgroundBrush(b);

				//force font
				//QBrush b = vipWidgetTextBrush(vipGetMainWindow());
				// VipTextStyle st;
				// st.setTextPen(QPen(b.color()));
				// QFont f;
				// f.fromString("DejaVu Sans Display,8,-1,5,50,0,0,0,0,0,Display");
				// st.setFont(f);
				// inst->setDefaultTextStyle(st);
				// inst->setTitleTextStyle(st);
			}


		}
		else
			arch.restore();

	}
	else if (arch.mode() == VipArchive::Write)
	{
		if (arch.start("VipGuiDisplayParamaters"))
		{
			arch.content("version", QString(VIP_VERSION));
			arch.content("skin", _skin);
			//printf("save: %s\n", _skin.toLatin1().data());

			arch.content("itemPaletteFactor", inst->itemPaletteFactor());
			arch.content("playerColorScale", (int)inst->playerColorScale());
			arch.content("video_player_axes", inst->videoPlayerShowAxes());
			arch.content("legendPosition", (int)inst->legendPosition());
			arch.content("defaultPlotArea", inst->defaultPlotArea());
			arch.content("defaultCurve", inst->defaultCurve());
			arch.content("displayTimeOffset", inst->displayTimeOffset());
			arch.content("displayType", (int)inst->displayType());

			//new in 2.2.18
			arch.content("showTimeMarkerAlways", inst->alwaysShowTimeMarker());
			arch.content("globalColorScale", inst->globalColorScale());

			//new in 3.3.3
			arch.content("flatHistogramStrength", inst->flatHistogramStrength());

			//new in 3.4.7
			arch.content("videoRenderingStrategy", inst->videoRenderingStrategy());
			arch.content("plotRenderingStrategy", inst->plotRenderingStrategy());
			arch.content("renderingThreads", inst->renderingThreads());

			//printf("write showTimeMarkerAlways: %i\n", (int)inst->alwaysShowTimeMarker());
			//printf("write globalColorScale: %i\n", (int)inst->globalColorScale());

			if (inst->hasTitleTextStyle())
				arch.content("titleTextStyle", inst->titleTextStyle());
			if (inst->hasDefaultTextStyle())
				arch.content("defaultTextStyle", inst->defaultTextStyle());

			//new in 3.0.2
			arch.content("backgroundBrush", inst->shapeBackgroundBrush());
			arch.content("borderPen", inst->shapeBorderPen());
			arch.content("drawComponents", (int)inst->shapeDrawComponents());

			// New in 3.11.0
			arch.content("displayExactPixels", (int)inst->displayExactPixels());

			arch.end();
		}

	}
}

bool  VipGuiDisplayParamaters::save(VipArchive & ar)
{
	serialize_VipGuiDisplayParamaters(this,ar);
	return !ar.hasError();
}
bool  VipGuiDisplayParamaters::restore(VipArchive & ar)
{
	serialize_VipGuiDisplayParamaters(this, ar);
	return !ar.hasError();
}

static int registerVipGuiDisplayParamaters()
{
	return 0;
}
static int _registerVipGuiDisplayParamaters = registerVipGuiDisplayParamaters();
