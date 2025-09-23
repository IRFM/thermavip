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

#include "VipVTKActorParametersEditor.h"
#include "VipVTKObject.h"
#include "VipDisplayVTKObject.h"
#include "VipStandardEditors.h"

#include <vtkActor.h>
#include <vtkProperty.h>

#include <qgridlayout.h>
#include <qlabel.h>




class VipVTKActorParametersEditor::PrivateData 
{
public:
	QList<VipVTKActorParameters> params;
	PlotVipVTKObjectList data;

	QSpinBox layer;

	VipDoubleEdit ambiantLighting;
	VipDoubleEdit diffuseLighting;
	VipDoubleEdit specularLighting;
	VipDoubleEdit lineWidth;
	VipDoubleEdit pointSize;

	QCheckBox lighting;
	QCheckBox shading;
	
	VipPenButton edgeColor;
	VipPenButton color;
	VipPenButton selectionColor;
};

VipVTKActorParametersEditor::VipVTKActorParametersEditor(QWidget * parent)
	:QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	QGridLayout * lay = new QGridLayout();
	int row = -1;

	lay->addWidget(new QLabel("Object layer"), ++row, 0);
	lay->addWidget(&d_data->layer, row, 1);
	d_data->layer.setRange(0, 10);

	lay->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	lay->addWidget(new QLabel("Ambiant lighting"), ++row, 0);
	lay->addWidget(&d_data->ambiantLighting, row, 1);

	lay->addWidget(new QLabel("Diffuse lighting"), ++row, 0);
	lay->addWidget(&d_data->diffuseLighting, row, 1);

	lay->addWidget(new QLabel("Specular lighting"), ++row, 0);
	lay->addWidget(&d_data->specularLighting, row, 1);

	lay->addWidget(new QLabel("Enable lighting"), ++row, 0);
	lay->addWidget(&d_data->lighting, row, 1);

	lay->addWidget(new QLabel("Enable shading"), ++row, 0);
	lay->addWidget(&d_data->shading, row, 1);


	lay->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	lay->addWidget(new QLabel("Edge color"), ++row, 0);
	lay->addWidget(&d_data->edgeColor, row, 1);

	lay->addWidget(new QLabel("Object color"), ++row, 0);
	lay->addWidget(&d_data->color, row, 1);

	lay->addWidget(new QLabel("Object selection color"), ++row, 0);
	lay->addWidget(&d_data->selectionColor, row, 1);


	lay->addWidget(VipLineWidget::createHLine(), ++row, 0,1,2);
	
	lay->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	lay->addWidget(new QLabel("Line width"), ++row, 0);
	lay->addWidget(&d_data->lineWidth, row, 1);

	lay->addWidget(new QLabel("Point size"), ++row, 0);
	lay->addWidget(&d_data->pointSize, row, 1);


	d_data->edgeColor.setMode(VipPenButton::Color);
	d_data->color.setMode(VipPenButton::Color);
	d_data->selectionColor.setMode(VipPenButton::Color);


	connect(&d_data->layer, SIGNAL(valueChanged(int)), this, SLOT(Update()));
	connect(&d_data->ambiantLighting, SIGNAL(valueChanged(double)), this, SLOT(Update()));
	connect(&d_data->diffuseLighting, SIGNAL(valueChanged(double)), this, SLOT(Update()));
	connect(&d_data->specularLighting, SIGNAL(valueChanged(double)), this, SLOT(Update()));
	connect(&d_data->lineWidth, SIGNAL(valueChanged(double)), this, SLOT(Update()));
	connect(&d_data->pointSize, SIGNAL(valueChanged(double)), this, SLOT(Update()));
	connect(&d_data->lighting, SIGNAL(clicked(bool)), this, SLOT(Update()));
	connect(&d_data->shading, SIGNAL(clicked(bool)), this, SLOT(Update()));
	connect(&d_data->edgeColor, SIGNAL(penChanged(const QPen &)), this, SLOT(Update()));
	connect(&d_data->color, SIGNAL(penChanged(const QPen &)), this, SLOT(Update()));
	connect(&d_data->selectionColor, SIGNAL(penChanged(const QPen &)), this, SLOT(Update()));

	setLayout(lay);
}

VipVTKActorParametersEditor::~VipVTKActorParametersEditor()
{
}

void VipVTKActorParametersEditor::Update()
{
	for (int i = 0; i < d_data->params.size(); ++i)
	{
		VipVTKActorParameters & params = d_data->params[i];
		if (d_data->layer.isEnabled()) params.layer = d_data->layer.value();
		if(d_data->ambiantLighting.isEnabled()) params.ambiantLighting = d_data->ambiantLighting.value();
		if (d_data->diffuseLighting.isEnabled()) params.diffuseLighting = d_data->diffuseLighting.value();
		if (d_data->specularLighting.isEnabled()) params.specularLighting = d_data->specularLighting.value();
		if (d_data->lighting.isEnabled()) params.lighting = d_data->lighting.isChecked();
		if (d_data->shading.isEnabled()) params.shading = d_data->shading.isChecked();

		if (d_data->edgeColor.isEnabled()) params.edgeColor = d_data->edgeColor.pen().color();
		if (d_data->color.isEnabled()) params.color = d_data->color.pen().color();
		if (d_data->selectionColor.isEnabled()) params.selectionColor = d_data->selectionColor.pen().color();

		if (d_data->lineWidth.isEnabled()) params.lineWidth = d_data->lineWidth.value();
		if (d_data->pointSize.isEnabled()) params.pointSize = d_data->pointSize.value();

		if (i < d_data->data.size())
			params.apply(d_data->data[i]);
	}
	Q_EMIT changed();
}

void VipVTKActorParametersEditor::setActorParameters(const QList<VipVTKActorParameters> & ps)
{
	d_data->data.clear();
	d_data->params = ps;
	d_data->layer.setEnabled(true);
	d_data->ambiantLighting.setEnabled(true);
	d_data->diffuseLighting.setEnabled(true);
	d_data->specularLighting.setEnabled(true);
	d_data->lineWidth.setEnabled(true);
	d_data->pointSize.setEnabled(true);
	d_data->lighting.setEnabled(true);
	d_data->shading.setEnabled(true);
	d_data->edgeColor.setEnabled(true);
	d_data->color.setEnabled(true);
	d_data->selectionColor.setEnabled(true);

	d_data->layer.blockSignals(true);
	d_data->ambiantLighting.blockSignals(true);
	d_data->diffuseLighting.blockSignals(true);
	d_data->specularLighting.blockSignals(true);
	d_data->lighting.blockSignals(true);
	d_data->shading.blockSignals(true);
	d_data->edgeColor.blockSignals(true);
	d_data->color.blockSignals(true);
	d_data->lineWidth.blockSignals(true);
	d_data->pointSize.blockSignals(true);
	d_data->selectionColor.blockSignals(true);

	if (d_data->params.size())
	{

		VipVTKActorParameters p = d_data->params.first();

		d_data->layer.setValue(p.layer);
		d_data->ambiantLighting.setValue(p.ambiantLighting);
		d_data->diffuseLighting.setValue(p.diffuseLighting);
		d_data->specularLighting.setValue(p.specularLighting);
		d_data->lighting.setChecked(p.lighting);
		d_data->shading.setChecked(p.shading);

		d_data->edgeColor.setPen(p.edgeColor);
		d_data->color.setPen(p.color);
		d_data->selectionColor.setPen(p.selectionColor);

		d_data->lineWidth.setValue(p.lineWidth);
		d_data->pointSize.setValue(p.pointSize);

		VipVTKActorParameters first = p;
		for (int i = 1; i < d_data->params.size(); ++i)
		{
			if (d_data->params[i].layer != first.layer) d_data->layer.setEnabled(false);
			if (d_data->params[i].ambiantLighting != first.ambiantLighting) d_data->ambiantLighting.setEnabled(false);
			if (d_data->params[i].diffuseLighting != first.diffuseLighting) d_data->diffuseLighting.setEnabled(false);
			if (d_data->params[i].specularLighting != first.specularLighting) d_data->specularLighting.setEnabled(false);
			if (d_data->params[i].lineWidth != first.lineWidth) d_data->lineWidth.setEnabled(false);
			if (d_data->params[i].pointSize != first.pointSize) d_data->pointSize.setEnabled(false);
			if (d_data->params[i].lighting != first.lighting) d_data->lighting.setEnabled(false);
			if (d_data->params[i].shading != first.shading) d_data->shading.setEnabled(false);
			if (d_data->params[i].edgeColor != first.edgeColor) d_data->edgeColor.setEnabled(false);
			if (d_data->params[i].color != first.color) d_data->color.setEnabled(false);
			if (d_data->params[i].selectionColor != first.selectionColor) d_data->selectionColor.setEnabled(false);
		}

	}


	d_data->layer.blockSignals(false);
	d_data->ambiantLighting.blockSignals(false);
	d_data->diffuseLighting.blockSignals(false);
	d_data->specularLighting.blockSignals(false);
	d_data->lighting.blockSignals(false);
	d_data->shading.blockSignals(false);
	d_data->edgeColor.blockSignals(false);
	d_data->color.blockSignals(false);
	d_data->selectionColor.blockSignals(false);
	d_data->lineWidth.blockSignals(false);
	d_data->pointSize.blockSignals(false);

	Q_EMIT changed();
}

const QList<VipVTKActorParameters> & VipVTKActorParametersEditor::actorParameters() const
{
	return d_data->params;
}


void VipVTKActorParametersEditor::setObjects(const PlotVipVTKObjectList& data)
{
	PlotVipVTKObjectList tmp;
	QList<VipVTKActorParameters> params;
	for (int i = 0; i < data.size(); ++i)
	{
		if (data[i])
		{
			VipVTKActorParameters p;
			p.import(data[i]);
			params << p;
			tmp << data[i];
		}
	}
	setActorParameters(params);
	d_data->data = tmp;
}

const PlotVipVTKObjectList& VipVTKActorParametersEditor::objects()
{
	return d_data->data;
}




static VipVTKActorParametersEditor * editDataObjectPtr(VipPlotVTKObject * plot)
{
	VipVTKActorParametersEditor * w = new VipVTKActorParametersEditor();
	w->setObjects(PlotVipVTKObjectList() << plot);
	return w;
}

static int registerEditors()
{
	vipFDObjectEditor().append<VipVTKActorParametersEditor *(VipPlotVTKObject*)>(editDataObjectPtr);
	return 0;
}

static bool _registerEditors = vipAddInitializationFunction(registerEditors);