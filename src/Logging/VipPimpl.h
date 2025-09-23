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

#ifndef VIP_PIMPL_H
#define VIP_PIMPL_H

#include "VipConfig.h"

namespace detail
{
	
	VIP_LOGGING_EXPORT void* addPimpl(void* p);
	VIP_LOGGING_EXPORT void removePimpl(const void* p);
	VIP_LOGGING_EXPORT bool isPimplValid(const void* p);

	template<class T, class D>
	bool isPimplValid(const std::unique_ptr<T, D>& p)
	{
		return isPimplValid(p.get());
	}

	struct PimplDeleter
	{
		template<class T>
		void operator()(T* p) const
		{
			removePimpl(p);
			delete p;
		}
	};

}

/// @brief Declare private data for Pimpl idiom, use in class definition, inside the private section
#define VIP_DECLARE_PRIVATE_DATA()                                                                                                                                                                     \
	template<class PrivData>                                                                                                                                                                              \
	friend bool vipIsObjectValid(const PrivData*);                                                                                                                                                         \
	class PrivateData;                                                                                                                                                                             \
	std::unique_ptr<PrivateData, detail::PimplDeleter> d_data

/// @brief Declare private data for Pimpl idiom, use in class constructor
#define VIP_CREATE_PRIVATE_DATA(...) d_data.reset(static_cast<PrivateData*>(detail::addPimpl(new PrivateData(__VA_ARGS__))))

/// @brief Access object private data as declared with VIP_DECLARE_PRIVATE_DATA()
#define VIP_D d_data


/// @brief Returns true if given object is still valid (not yet destroyed).
/// Passed object must use the pimpl idiom based on VIP_DECLARE_PRIVATE_DATA() macro.
template<class T>
bool vipIsObjectValid(const T* obj)
{
	return detail::isPimplValid(obj->VIP_D);
}

#endif
