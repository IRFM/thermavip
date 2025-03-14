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

#ifndef VIP_PY_REGISTER_PROCESSING_H
#define VIP_PY_REGISTER_PROCESSING_H

#include "VipProcessingObject.h"


/// @brief Class managing registered Python processings
/// (VipPySignalFusionProcessing and VipPyProcessing) using
/// VipPySignalFusionProcessing::registerThisProcessing() or
/// VipPyProcessing::registerThisProcessing().
///
class VIP_CORE_EXPORT VipPyRegisterProcessing
{
public:
	/// @brief Save given processings in the Python custom processing XML file.
	static bool saveCustomProcessings(const QList<VipProcessingObject::Info>& infos);
	/// @brief Save all registered processings in the Python custom processing XML file.
	static bool saveCustomProcessings();
	/// @brief Returns all registered custom processings
	static QList<VipProcessingObject::Info> customProcessing();

	/// @brief Load the custom processing from the XML file and add them to the global VipProcessingObject system.
	/// If \a overwrite is true, this will overwrite already existing processing with the same name and category.
	static int loadCustomProcessings(bool overwrite);
};

#endif
