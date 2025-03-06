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

#ifndef VIP_FOV_EDITOR_H
#define VIP_FOV_EDITOR_H

#include "VipStandardWidgets.h"
#include "VipToolWidget.h"
#include "VipFieldOfView.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QRadioButton>
#include <qdatetimeedit.h>


class VIP_GUI_EXPORT VipPoint3DEditor : public QWidget
{
	Q_OBJECT

	VipDoubleEdit x;
	VipDoubleEdit y;
	VipDoubleEdit z;
	double value[3];

public:
	VipPoint3DEditor(QWidget * parent=nullptr);
	void SetValue(const double *);
	bool GetValue(double *) const;
	double * GetValue() const;

Q_SIGNALS:
	void changed();

private Q_SLOTS:
	void EmitChanged() {
		Q_EMIT changed();
	}
};


class VIP_GUI_EXPORT VipFOVTimeEditor : public QWidget
{
	Q_OBJECT
	QLineEdit nanoTimeEdit;
	QDateTimeEdit dateEdit;
	QDateTime date;

public:
	VipFOVTimeEditor(QWidget * parent = nullptr);

	void setTime(qint64 milli_time);
	qint64 time() const;

Q_SIGNALS:
	void changed();

private Q_SLOTS:
	void dateEdited();
	void nanoTimeEdited();

};

class VIP_GUI_EXPORT VipFOVOffsetEditor : public QWidget
{
	Q_OBJECT
	QLabel m_image;
	VipDoubleEdit m_add_yaw;
	VipDoubleEdit m_add_pitch;
	VipDoubleEdit m_add_roll;
	VipDoubleEdit m_add_alt;
	VipDoubleEdit m_fixed_yaw;
	VipDoubleEdit m_fixed_pitch;
	VipDoubleEdit m_fixed_roll;
	VipDoubleEdit m_fixed_alt;

	QRadioButton m_r_add_yaw;
	QRadioButton m_r_add_pitch;
	QRadioButton m_r_add_roll;
	QRadioButton m_r_add_alt;
	QRadioButton m_r_fixed_yaw;
	QRadioButton m_r_fixed_pitch;
	QRadioButton m_r_fixed_roll;
	QRadioButton m_r_fixed_alt;

public:
	VipFOVOffsetEditor(QWidget * parent = nullptr);

	double additionalYaw() const;
	double additionalPitch() const;
	double additionalRoll() const;
	double additionalAltitude() const;
	double fixedYaw() const;
	double fixedPitch() const;
	double fixedRoll() const;
	double fixedAltitude() const;

	bool hasFixedYaw() const;
	bool hasFixedPitch() const;
	bool hasFixedRoll() const;
	bool hasFixedAltitude() const;

public Q_SLOTS:
	void setAdditionalYaw(double deg);
	void setAdditionalPitch(double deg);
	void setAdditionalRoll(double deg);
	void setAdditionalAltitude(double alt);
	void setFixedYaw(double deg);
	void setFixedPitch(double deg);
	void setFixedRoll(double deg);
	void setFixedAltitude(double alt);
	void setUseFixedYaw(bool enable);
	void setUseFixedPitch(bool enable);
	void setUseFixedRoll(bool enable);
	void setUseFixedAltitude(bool enable);

private Q_SLOTS:
	void emitChanged() {
	Q_EMIT changed();
}

Q_SIGNALS:
	void changed();
};

class VIP_GUI_EXPORT VipFOVEditor : public QWidget
{
	Q_OBJECT
public:

	QWidget stdOptions;
	QWidget opticalDistortions;
	QGroupBox showStdOptions;
	QGroupBox showOpticalDistortions;

	QLineEdit name;
	VipPoint3DEditor pupilPos;
	VipPoint3DEditor targetPoint;
	QLineEdit verticalFov;
	QLineEdit horizontalFov;
	QLineEdit rotation;
	QComboBox viewUp;
	QLineEdit focal;
	QLineEdit zoom;
	QSpinBox pixWidth;
	QSpinBox pixHeight;
	QSpinBox cropX;
	QSpinBox cropY;
	VipFOVTimeEditor time;
	QLineEdit K2;
	QLineEdit K4;
	QLineEdit K6;
	QLineEdit P1;
	QLineEdit P2;
	QLineEdit AlphaC;
	VipFieldOfView FOV;

	QWidget * lastSender;
 
	VipFOVEditor(QWidget * parent = nullptr);

	void setFieldOfView(const VipFieldOfView &);
	VipFieldOfView fieldOfView() const;

	QGroupBox* AddSection(const QString & section_name, QWidget * section);

Q_SIGNALS:
	void changed();
	void SizeChanged();

private Q_SLOTS:
	void EmitChanged() {
		lastSender = qobject_cast<QWidget*>(sender());
		Q_EMIT changed();
	}
	void EmitSizeChanged() {
		Q_EMIT SizeChanged();
	}
};



class VipFOVSequence;
class VipFOVItem;
class AdjustFieldOfView;
class VipVTKGraphicsView;

/**
Modify a VipFOVSequence
*/
class VIP_GUI_EXPORT VipFOVSequenceEditor : public QWidget
{
	Q_OBJECT

public:
	VipFOVSequenceEditor(VipVTKGraphicsView * view = nullptr, QWidget * parent = nullptr);
	~VipFOVSequenceEditor();

	void SetGraphicsView(VipVTKGraphicsView *);
	VipVTKGraphicsView * GetGraphicsView() const;

	void SetFOVItem(VipFOVItem *);
	VipFOVItem * GetFOVItem() const;

	void SetSequence(VipFOVSequence * seq);
	VipFOVSequence * GetSequence() const;

	void addFieldOfView(const VipFieldOfView & fov);
	VipFieldOfViewList GetFOVs() const;

	VipFOVEditor * Editor() const;

	VipFieldOfView SelectedFOV() const;

public Q_SLOTS:
	void AddCurrentFOV();
	void RemoveSelectedFOVs();
	void ChangeCurrentFOV();
	void selectionChanged();
	void EnabledInterpolation(bool enable);
	void ApplyCurrentFOV();
	void Apply();
	bool CheckValidity();

private Q_SLOTS:
	void EditorChanged();
	void EmitAccepted() {
		Q_EMIT Accepted();
	}
	void EmitRejected() {
		Q_EMIT Rejected();
	}

Q_SIGNALS:
	void Accepted();
	void Rejected();
	void SizeChanged();

private Q_SLOTS:
	void EmitSizeChanged() {
		Q_EMIT SizeChanged();
	}

protected:
	void resizeEvent(QResizeEvent * evt)
	{
		QWidget::resizeEvent(evt);
	}


private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};


class VIP_GUI_EXPORT VipFOVSequenceEditorTool : public VipToolWidget
{
public:
	VipFOVSequenceEditorTool(VipMainWindow * window);
	VipFOVSequenceEditor * editor() const;

private:
	VipFOVSequenceEditor * m_editor;
};

class VipMainWindow;
VIP_GUI_EXPORT VipFOVSequenceEditorTool* vipGetFOVSequenceEditorTool(VipMainWindow* win = nullptr);

#endif
