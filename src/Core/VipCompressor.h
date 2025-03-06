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

#ifndef VIP_COMPRESSOR_H
#define VIP_COMPRESSOR_H

#include "VipProcessingObject.h"

/// Tells if the input data is NOT compressed.
/// If this functions returns false, it means that the input data is possibly compressed with the zlib (and you'll have to uncompress it to be sure).
///
/// To use this function on a QByteArray that has been compressed with qCompress, you need to remove the first 4 bytes since
/// Qt add the uncompressed size at the beginning.
// VIP_CORE_EXPORT bool vipIsUncompressed(const uchar* data, int len);

/// Same as vipIsUncompressed, but works on a QByteArray compressed with qCompress function.
// VIP_CORE_EXPORT bool vipIsQtUncompressed(const QByteArray& ar);

/// VipCompressor is the base class for all processing implementing a compression algorithm.
///
/// A compressor object can be used for both compression and decompression of any kind of data. For that, you must
/// reimplement the members #VipCompressor::compress() and #VipCompressor::uncompress().
///
/// If the compressor only support a few types (for instance we can imagine a PNG compressor that only accept image data),
/// you should reimplement the #VipProcessingObject::acceptInput() member function.
class VIP_CORE_EXPORT VipCompressor : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input_data)
	VIP_IO(VipOutput output_data)
	VIP_IO(VipProperty is_compressing)

public:
	enum Mode
	{
		Compress,
		Uncompress
	};

	VipCompressor(QObject* parent = nullptr);
	~VipCompressor();

	/// Set the compression mode (compress or uncompress)
	void setMode(Mode mode) { propertyAt(0)->setData(mode == Compress ? true : false); }
	Mode mode() const { return propertyAt(0)->value<bool>() ? Compress : Uncompress; }

	/// Compress a QVariant into a QByteArray.
	/// In case of error, you must call #VipProcessingObject::setError().
	virtual QByteArray compressVariant(const QVariant& value) = 0;

	/// Uncompress a raw binary packet. In case of error, you must call #VipProcessingObject::setError().
	/// For temporal compression (like most of the video compression algorithms), the algorithm might need
	/// several binary packets before constructing the final data. In this case, \a need_more should be set to true.
	virtual QVariant uncompressVariant(const QByteArray& raw_data, bool& need_more) = 0;

protected:
	virtual void apply();
};

/// @brief Compressor processing using DEFLATE compression.
/// Currently deprecated
class VIP_CORE_EXPORT VipGzipCompressor : public VipCompressor
{
	Q_OBJECT
	Q_CLASSINFO("category", "compressor")
	Q_CLASSINFO("description", "GZip based compressor/decompressor")
	VIP_IO(VipProperty compression_level)

public:
	VipGzipCompressor() { this->propertyAt(1)->setData(-1); }

	virtual QByteArray compressVariant(const QVariant& value);
	virtual QVariant uncompressVariant(const QByteArray& raw_data, bool& need_more);
};

#endif
