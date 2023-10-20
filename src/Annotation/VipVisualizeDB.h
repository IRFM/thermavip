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

#ifndef VIP_VISUALIZE_DB_H
#define VIP_VISUALIZE_DB_H

#include <qtablewidget.h>

#include "VipProcessingObject.h"
#include "VipSqlQuery.h"
#include "VipStandardWidgets.h"
#include "VipToolWidget.h"

// Add options
//  - show selected events on: 1) new player, 2) list of existing players
//  - for selected events, plot time trace of: max Y, centroid X/Y (gather all columns in one widget with check boxes,
// and one checkbox to merge pulses. If merged, split them with  nan values. Make the action droppable.

class VipPlotItem;
class VIP_ANNOTATION_EXPORT VisualizeDB : public QWidget
{
	Q_OBJECT

public:
	VisualizeDB(QWidget* parent = nullptr);
	~VisualizeDB();

	static VisualizeDB* fromChild(QWidget* w);

	VipQueryDBWidget* queryWidget() const;
	QPushButton* launchQueryButton() const;
	QPushButton* resetQueryButton() const;
	QTableWidget* tableWidget() const;
	VipEventQueryResults events() const;

	void displayEventResult(const VipEventQueryResults& res, VipProgress* p = nullptr);

	virtual bool eventFilter(QObject* watched, QEvent* evt);
	// virtual bool viewportEvent(QEvent *event);
public Q_SLOTS:
	void suppressSelectedLines();
	void editSelectedColumn();
	void launchQuery();
	void resetQueryParameters();

	void saveToCSV();
	void copyToClipBoard();

private Q_SLOTS:
	void displaySelectedEvents(QAction*);
	void findRelatedEvents();
	void plotTimeTrace();

private:
	QByteArray dumpSelection();

	class PrivateData;
	PrivateData* m_data;
};

class VIP_ANNOTATION_EXPORT VisualizeDBToolWidget : public VipToolWidget
{
	Q_OBJECT

public:
	VisualizeDBToolWidget(VipMainWindow* = nullptr);
	VisualizeDB* getVisualizeDB() const;
};

VIP_ANNOTATION_EXPORT VisualizeDBToolWidget* vipGetVisualizeDBToolWidget(VipMainWindow* win = nullptr);

/// @brief Initialize VisualizeDBToolWidget, must be called in Thermavip entry point
VIP_ANNOTATION_EXPORT bool vipInitializeVisualizeDBWidget();

#endif
