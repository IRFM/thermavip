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
	
	VIP_LOGGING_EXPORT void addPimpl(const void* p);
	VIP_LOGGING_EXPORT void removePimpl(const void* p);
	VIP_LOGGING_EXPORT bool existPimpl(const void* p);

	// Detect QObject inheriting class
	template<class T>
	struct isQObject : public std::is_base_of<QObject, T>
	{
	};

	// Internal data for Pimpl idiom
	template<class PrivateData>
	struct InternalData
	{
		template<class U>
		static void emitObjectDestroy(void * u) {Q_EMIT static_cast<U*>(u)->destroy();}

		void* object ;
		PrivateData data;
		void (*emitDestroy)(void *);
		
		template<class T, class... Args>
		InternalData(T* obj, Args&&... args)
		  : object(obj)
		  , data(std::forward<Args>(args)...)
		  , emitDestroy(nullptr)
		{
			if constexpr (isQObject<T>::value)
				emitDestroy = emitObjectDestroy<T>;
			addPimpl(obj);
		}

		~InternalData() noexcept 
		{
			// Remove object from internal list
			removePimpl(object);
			// emit signal destroy() for QObject types
			if (emitDestroy)
				emitDestroy(object);
		}

		
	};

	/// @brief Internal data of types classes using  VIP_DECLARE_PRIVATE_DATA or VIP_DECLARE_PRIVATE_DATA_NO_QOBJECT
	/// @tparam PrivateData 
	template<class PrivateData>
	class InternalDataPtr
	{
		using Internal = InternalData<PrivateData>;
		std::unique_ptr<Internal> d_ptr;

	public:
		InternalDataPtr() = default;
		~InternalDataPtr() = default;
		InternalDataPtr(const InternalDataPtr&) = delete;
		InternalDataPtr(InternalDataPtr&& other) noexcept = default;

		InternalDataPtr& operator=(InternalDataPtr&&) noexcept = default;
		InternalDataPtr& operator=(const InternalDataPtr&) = delete;

		template<class T, class... Args>
		void reset(T* obj, Args&&... args)
		{
			d_ptr.reset(new Internal(obj,std::forward<Args>(args)...));
		}
		void clear() { d_ptr.reset();}

		PrivateData* get() const noexcept { return const_cast<PrivateData*>(&d_ptr->data); }
		PrivateData* operator->() const noexcept { return get(); }
		PrivateData& operator*() const noexcept { return *get(); }
		operator const void*() const noexcept { return get(); }
	};

	template<class T>
	struct isPimpl : std::false_type
	{
	};
	template<class T>
	struct isPimpl<InternalDataPtr<T> > : std::true_type
	{
	};
	
	template <typename T>                                                      
    struct hasPimplData
    {                                                                          
        typedef char yes_type;
        typedef long no_type;
        template <typename U> static yes_type test(decltype(&U::d_data)); 
        template <typename U> static no_type  test(...);                       
    public:                                                                    
        static constexpr bool value = sizeof(test<T>(0)) == sizeof(yes_type) && isPimpl<decltype(T::d_data)>::value;  
    };


}


/// @brief Declare private data for Pimpl idiom, use in class definition, inside the private section.
/// This version is for any kind of (non copyable) classes
#define VIP_DECLARE_PRIVATE_DATA_NO_QOBJECT()                                                                                                                                                          \
	template<class T>                                                                                                                                                                              \
	friend struct detail::hasPimplData;                                                                                                                                                    \
	class PrivateData;                                                                                                                                                                             \
	detail::InternalDataPtr<PrivateData> d_data

/// @brief Declare private data for Pimpl idiom, use in class definition, inside the private section.
/// This version is for QObject inheriting classes using the Q_OBJECT macro.
/// It adds the signal destroy() which is called at the end of object destructor while the private data is STILL valid
#define VIP_DECLARE_PRIVATE_DATA() \
	Q_SIGNALS:\
	void destroy();\
	private:\
	VIP_DECLARE_PRIVATE_DATA_NO_QOBJECT()

/// @brief Declare private data for Pimpl idiom, use in class constructor
#define VIP_CREATE_PRIVATE_DATA(...) d_data.reset(this, ##__VA_ARGS__)

/// @brief Access object private data as declared with VIP_DECLARE_PRIVATE_DATA()
#define VIP_D d_data


/// @brief Returns true if given object is still valid (not yet destroyed).
/// Passed object must use the pimpl idiom based on VIP_DECLARE_PRIVATE_DATA and VIP_DECLARE_PRIVATE_DATA_NO_QOBJECT macros.
template<class T>
bool vipIsObjectValid(const T* obj)
{
	static_assert(detail::hasPimplData<T>::value, "vipIsObjectValid called on invalid data type");
	return detail::existPimpl(obj);
}

#endif
