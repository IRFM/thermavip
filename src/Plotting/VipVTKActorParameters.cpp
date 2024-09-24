/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#include "VipVTKActorParameters.h"
#include "VipVTKObject.h"
#include "VipDisplayVTKObject.h"

#include <vtkActor.h>
#include <vtkProperty.h>

#include <qgridlayout.h>
#include <qlabel.h>



VipVTKActorParameters::VipVTKActorParameters()
{
	vtkActor * actor = vtkActor::New();
	this->ambiantLighting = actor->GetProperty()->GetAmbient();
	this->diffuseLighting = actor->GetProperty()->GetDiffuse();
	this->edgeColor = toQColor(actor->GetProperty()->GetEdgeColor());
	this->lighting = actor->GetProperty()->GetLighting();
	this->lineWidth = actor->GetProperty()->GetLineWidth();
	this->pointSize = actor->GetProperty()->GetPointSize();
	this->shading = actor->GetProperty()->GetShading();
	this->specularLighting = actor->GetProperty()->GetSpecular();
	actor->Delete();

	this->color = toQColor(VipVTKObject::stdColor());
	this->selectionColor = toQColor(VipVTKObject::stdColor());
	this->layer = 0;
}

void VipVTKActorParameters::import(VipPlotVTKObject* data)
{
	if (data->actor())
	{
		this->ambiantLighting = data->actor()->GetProperty()->GetAmbient();
		this->diffuseLighting = data->actor()->GetProperty()->GetDiffuse();
		this->edgeColor = (data->edgeColor());
		this->lighting = data->actor()->GetProperty()->GetLighting();
		this->lineWidth = data->actor()->GetProperty()->GetLineWidth();
		this->pointSize = data->actor()->GetProperty()->GetPointSize();
		this->shading = data->actor()->GetProperty()->GetShading();
		this->specularLighting = data->actor()->GetProperty()->GetSpecular();
		this->color = (data->color());
		this->selectionColor = (data->selectedColor());
		this->layer = data->layer();
	}
}

void VipVTKActorParameters::apply(VipPlotVTKObject* data)
{
		if (vtkActor * actor = data->actor())
		{
			actor->GetProperty()->SetAmbient(this->ambiantLighting);
			actor->GetProperty()->SetDiffuse(this->diffuseLighting);
			actor->GetProperty()->SetLighting(this->lighting);
			actor->GetProperty()->SetLineWidth(this->lineWidth);
			actor->GetProperty()->SetPointSize(this->pointSize);
			actor->GetProperty()->SetShading(this->shading);
			actor->GetProperty()->SetSpecular(this->specularLighting);
		}

		data->setColor((this->color));
		data->setSelectedColor((this->selectionColor));
		data->setLayer(this->layer);
		data->setEdgeColor((this->edgeColor));
}




VipVTKActorParameters * vipGlobalActorParameters()
{
	static VipVTKActorParameters * inst = nullptr;
	if (!inst)
	{
		inst = new VipVTKActorParameters();
		inst->edgeColor = Qt::black;
		inst->pointSize = 3;
	}
	return inst;
}

