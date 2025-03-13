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

#ifndef VIP_PY_NPZ_DEVICE
#define VIP_PY_NPZ_DEVICE

#include "VipPyOperation.h"
#include "VipIODevice.h"

/// @brief Save 2D array objects in NPZ file format.
///
/// VipPyNPZDevice will vertically stack all 2D arrays given as input,
/// and save the 3D array in the close() member.
/// 
/// The array name in the NPZ file will  be 'arr_...'
/// where '...' is deduced from input names.
/// 
/// 
class VIP_CORE_EXPORT VipPyNPZDevice : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)

public:
	VipPyNPZDevice(QObject* parent = nullptr);
	~VipPyNPZDevice();

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool acceptInput(int, const QVariant& v) const
	{
		if (v.userType() == qMetaTypeId<VipNDArray>()) {
			const VipNDArray ar = v.value<VipNDArray>();
			if (vipIsImageArray(ar))
				return false;
			return true;
		}
		return v.canConvert<VipNDArray>();
	}
	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "Python files (*.npz)"; }
	virtual void close();

protected:
	virtual void apply();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPyNPZDevice*)


/// @brief Save 2D array objects in Matlab format.
///
/// VipPyMATDevice behaves in the same way as VipPyNPZDevice,
/// but save the resulting 3D array in a Matlab file.
///
class VIP_CORE_EXPORT VipPyMATDevice : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)

public:
	VipPyMATDevice(QObject* parent = nullptr);
	~VipPyMATDevice();

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool acceptInput(int, const QVariant& v) const
	{
		if (v.userType() == qMetaTypeId<VipNDArray>()) {
			const VipNDArray ar = v.value<VipNDArray>();
			if (vipIsImageArray(ar))
				return false;
			return true;
		}
		return v.canConvert<VipNDArray>();
	}
	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "Matlab files (*.mat)"; }
	virtual void close();

protected:
	virtual void apply();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPyMATDevice*)

#endif
