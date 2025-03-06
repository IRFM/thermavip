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

#ifndef VIP_VTK_IMAGE_DATA_H
#define VIP_VTK_IMAGE_DATA_H

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <QFileInfo>
#include <QStringList>
#include <QString>

#include "VipConfig.h"

class vtkScalarsToColors;

/// @brief Simple wapper class for vtkImageData
///
/// VipVTKImage wrap a vtkImageData and provides
/// convenient functions to manipulate it.
///
/// VipVTKImage uses shared ownership.
/// 
class VIP_DATA_TYPE_EXPORT VipVTKImage
{
	vtkSmartPointer<vtkImageData> d_image;
	QString d_name;
	QFileInfo d_info;

	static vtkSmartPointer<vtkImageData> createVTKImage(int w, int h, double value, int type = VTK_DOUBLE);
	static vtkSmartPointer<vtkImageData> createVTKImage(int w, int h, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
	static vtkSmartPointer<vtkImageData> loadImageFileToVTK(const QString& image);

public:
	/// @brief Construct a null image
	VipVTKImage();
	/// @brief Construct from width, height, fill value and type (VTK_FLOAT, VTK_INT...)
	VipVTKImage(int w, int h, double value, int type);
	/// @brief Construct RGBA image from width, height, fill color
	VipVTKImage(int w, int h, uint pixel);
	/// @brief Construct from a vtkImageData
	VipVTKImage(vtkImageData* img);
	/// @brief Construct from an image path.
	/// Supports common image formats (bmp, png, jpeg...), vti files (VTK format), text files (ascii format)
	VipVTKImage(const QString& filename);

	VipVTKImage(const VipVTKImage&) = default;
	VipVTKImage& operator=(const VipVTKImage&) = default;

	/// @brief Save image to file
	bool save(const QString& filename);

	/// @brief Returns the image width
	int width() const;
	/// @brief Returns the image height
	int height() const;
	/// @brief Returns the image type (VTK_FLOAT, VTK_INT...).
	/// RGBA images have a type of VTK_UNSIGNED_CHAR.
	int type() const;

	/// @brief Returns the file name (not the full path)
	/// if the image was constructed from a file or saved to a file.
	const QString& name() const;
	const QFileInfo& info() const;

	/// @brief Returns the underlying vtkImageData
	vtkImageData* image() const;
	/// @brief Returns true if the vtkImageData is null
	bool isNull() const;
	/// @brief Returns true if this image is a color one
	bool isRGBA() const;

	/// @brief Set/get the image origin
	void setOrigin(const QPoint&);
	QPoint origin() const;

	/// @brief Returns RGBA pixel at given position
	uint RGBAPixelAt(int x, int y) const;
	/// @brief Returns pixel at given position
	double doublePixelAt(int x, int y) const;

	void setRGBAPixelAt(int x, int y, uint pixel);
	void setDoublePixelAt(int x, int y, double value);

	/// @brief Convert to QImage.
	/// If the image is not a  color one, use provided vtkScalarsToColors
	/// to convert to color.
	QImage toQImage(vtkScalarsToColors* = nullptr) const;

	/// @brief Apply a zoom factor and returns the new image
	VipVTKImage zoom(double zoom_factor) const;
	/// @brief Apply a vertical and/or horizontal mirrorring
	VipVTKImage mirrored(bool horizontal = false, bool vertical = false) const;
	/// @brief Returns a rescaled version of this image
	VipVTKImage scaled(int width, int height, bool interpolate = true) const;

	bool operator==(const VipVTKImage& other) const { return image() == other.image(); }
	bool operator!=(const VipVTKImage& other) const { return image() != other.image(); }

	/// @brief Returns all supported image suffixes
	static QStringList imageSuffixes();
	/// @brief Returns image filters that can be used in a file dialog
	static QStringList imageFilters();
};

Q_DECLARE_METATYPE(VipVTKImage)

#endif
