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

#ifndef VIP_EDIT_XML_SYMBOLS_H
#define VIP_EDIT_XML_SYMBOLS_H

#include "VipXmlArchive.h"
#include <qwidget.h>

/// Base widget used to edit XML symbol within a VipArchive.
///
/// Usually VipEditXMLSymbols is used to edit symbols in a session file. However you can provide your
/// own widget to do that in order to simplifiy the process. For instance, WEST plugin provides a
/// simplier widget that only ollows to save a custom pulse number, and only in some circonstances.
///
/// Use vipSetBaseEditXMLSymbols to provide a new behavior to the session saving.
class VIP_GUI_EXPORT VipBaseEditXMLSymbols : public QWidget
{
public:
	VipBaseEditXMLSymbols(QWidget* parent = nullptr)
	  : QWidget(parent)
	{
	}
	virtual void setEditableSymbols(const QList<VipEditableArchiveSymbol>& symbols) = 0;
	virtual const QList<VipEditableArchiveSymbol>& editableSymbols() const = 0;

	virtual void applyToArchive(VipXArchive& arch) = 0;
};

/// Set the function used to create the VipBaseEditXMLSymbols that will provide XML symbol edition features.
VIP_GUI_EXPORT void vipSetBaseEditXMLSymbols(const std::function<VipBaseEditXMLSymbols*()>&);

/// Edit the editable symbols of a XML file (marked with 'content_editable') in order to create a custom session file (more like a perspective in Eclipse)
class VIP_GUI_EXPORT VipEditXMLSymbols : public VipBaseEditXMLSymbols
{
	Q_OBJECT
public:
	VipEditXMLSymbols(QWidget* parent = nullptr);
	~VipEditXMLSymbols();

	void setEditableSymbols(const QList<VipEditableArchiveSymbol>& symbols);
	const QList<VipEditableArchiveSymbol>& editableSymbols() const;

	void applyToArchive(VipXArchive& arch);

private Q_SLOTS:
	void selectionChanged(bool);
	void valueChanged(int);

private:
	class PrivateData;
	PrivateData* m_data;
};

/// Options to save a Thermavip session
class VIP_GUI_EXPORT VipExportSessionWidget : public QWidget
{
	Q_OBJECT
public:
	VipExportSessionWidget(QWidget* parent = nullptr, bool export_current_area = false);
	~VipExportSessionWidget();

	bool exportMainWindow() const;
	bool exportCurrentArea() const;
	// bool exportCurrentPlayer() const;
	QString filename() const;

public Q_SLOTS:
	void exportSession();
	void setExportCurrentArea(bool);
	void setFilename(const QString& filename);
private Q_SLOTS:
	void exportTypeChanged();

private:
	class PrivateData;
	PrivateData* m_data;
};

/// Load a Thermavip session.
/// Display the editable content found within the XML archive.
class VIP_GUI_EXPORT VipImportSessionWidget : public QWidget
{
	Q_OBJECT

public:
	VipImportSessionWidget(QWidget* parent = nullptr);
	~VipImportSessionWidget();

	static bool hasEditableContent(VipXArchive& arch);

	void importArchive(VipXArchive& arch);
	void applyToArchive(VipXArchive& arch);

private:
	class PrivateData;
	PrivateData* m_data;
};


#endif