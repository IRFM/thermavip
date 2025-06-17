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
 
extern "C" {
#include <H5Ipublic.h>
#include <H5FDpublic.h>
#include <hdf5.h>
}
#include <qiodevice.h>

#include "VipH5DeviceDriver.h"
#include "VipLogging.h"

/* The driver identification number, initialized at runtime */
static hid_t H5FD_DEVICE_g = 0;

#define H5FD_SEC2_MAX_FILENAME_LEN      1024
#define SUCCEED 0


typedef struct H5FD_device_t {
	H5FD_t	pub;			/*public stuff, must be first	*/
	QIODevice * device;
	haddr_t	eoa;			/*end of allocated region	*/
	haddr_t	eof;			/*end of file; current file size*/
} H5FD_device_t;




/*
* These macros check for overflow of various quantities.  These macros
* assume that HDoff_t is signed and haddr_t and size_t are unsigned.
*
* ADDR_OVERFLOW:	Checks whether a file address of type `haddr_t'
*			is too large to be represented by the second argument
*			of the file seek function.
*
* SIZE_OVERFLOW:	Checks whether a buffer size of type `hsize_t' is too
*			large to be represented by the `size_t' type.
*
* REGION_OVERFLOW:	Checks whether an address and size pair describe data
*			which can be addressed entirely by the second
*			argument of the file seek function.
*/
#define MAXADDR (((haddr_t)1<<(8*sizeof(quint64)-1))-1)
#define ADDR_OVERFLOW(A)	(HADDR_UNDEF==(A) ||			      \
				 ((A) & ~(haddr_t)MAXADDR))
#define SIZE_OVERFLOW(Z)	((Z) & ~(hsize_t)MAXADDR)
#define REGION_OVERFLOW(A,Z)	(ADDR_OVERFLOW(A) || SIZE_OVERFLOW(Z) ||      \
                                 HADDR_UNDEF==(A)+(Z) ||		      \
				 (HDoff_t)((A)+(Z))<(HDoff_t)(A))

/* Prototypes */
static H5FD_t *H5FD_device_open(const char *name, unsigned flags, hid_t fapl_id,
	haddr_t maxaddr);
static herr_t H5FD_device_close(H5FD_t *_file);
static haddr_t H5FD_device_get_eoa(const H5FD_t *_file, H5FD_mem_t type);
static herr_t H5FD_device_set_eoa(H5FD_t *_file, H5FD_mem_t type, haddr_t addr);

#if (H5_VERS_MAJOR*100 + H5_VERS_MINOR) < 110 
	static haddr_t H5FD_device_get_eof(const H5FD_t *_file);
#else
	static haddr_t H5FD_device_get_eof(const H5FD_t *_file, H5FD_mem_t type);
#endif

static herr_t H5FD_device_read(H5FD_t *_file, H5FD_mem_t type, hid_t fapl_id, haddr_t addr,
	size_t size, void *buf);
static herr_t H5FD_device_write(H5FD_t *_file, H5FD_mem_t type, hid_t fapl_id, haddr_t addr,
	size_t size, const void *buf);


#if (H5_VERS_MAJOR*100 + H5_VERS_MINOR) < 110 

static const H5FD_class_t H5FD_device_g = {
	"QIODevice",					/*name			*/
	MAXADDR,					/*maxaddr		*/
	H5F_CLOSE_WEAK,				/* fc_degree		*/
	nullptr,					/*sb_size		*/
	nullptr,					/*sb_encode		*/
	nullptr,					/*sb_decode		*/
	0, 						/*fapl_size		*/
	nullptr,					/*fapl_get		*/
	nullptr,					/*fapl_copy		*/
	nullptr, 					/*fapl_free		*/
	0,						/*dxpl_size		*/
	nullptr,					/*dxpl_copy		*/
	nullptr,					/*dxpl_free		*/
	H5FD_device_open,			        /*open			*/
	H5FD_device_close,		                /*close			*/
	nullptr,			        /*cmp			*/
	nullptr,		                /*query			*/
	nullptr,					/*get_type_map		*/
	nullptr,					/*alloc			*/
	nullptr,					/*free			*/
	H5FD_device_get_eoa,				/*get_eoa		*/
	H5FD_device_set_eoa, 				/*set_eoa		*/
	H5FD_device_get_eof,				/*get_eof		*/
	nullptr,                       /*get_handle            */
	H5FD_device_read,				/*read			*/
	H5FD_device_write,				/*write			*/
	nullptr,					/*flush			*/
	nullptr,				/*truncate		*/
	nullptr,                                       /*lock                  */
	nullptr,                                       /*unlock                */
	H5FD_FLMAP_SINGLE 				/*fl_map		*/
};

#else

static const H5FD_class_t H5FD_device_g = {
	#if (H5_VERS_MAJOR*100 + H5_VERS_MINOR) >= 113 
	123456,
	#endif
	"QIODevice",					/*name			*/
	MAXADDR,					/*maxaddr		*/
	H5F_CLOSE_WEAK,				/* fc_degree		*/
	nullptr,					/*terminate*/
	nullptr,					/*sb_size		*/
	nullptr,					/*sb_encode		*/
	nullptr,					/*sb_decode		*/
	0, 						/*fapl_size		*/
	nullptr,					/*fapl_get		*/
	nullptr,					/*fapl_copy		*/
	nullptr, 					/*fapl_free		*/
	0,						/*dxpl_size		*/
	nullptr,					/*dxpl_copy		*/
	nullptr,					/*dxpl_free		*/
	H5FD_device_open,			        /*open			*/
	H5FD_device_close,		                /*close			*/
	nullptr,			        /*cmp			*/
	nullptr,		                /*query			*/
	nullptr,					/*get_type_map		*/
	nullptr,					/*alloc			*/
	nullptr,					/*free			*/
	H5FD_device_get_eoa,				/*get_eoa		*/
	H5FD_device_set_eoa, 				/*set_eoa		*/
	H5FD_device_get_eof,				/*get_eof		*/
	nullptr,                       /*get_handle            */
	H5FD_device_read,				/*read			*/
	H5FD_device_write,				/*write			*/
	nullptr,					/*flush			*/
	nullptr,				/*truncate		*/
	nullptr,                                       /*lock                  */
	nullptr,                                       /*unlock                */
#if (H5_VERS_MAJOR*100 + H5_VERS_MINOR) >= 113 
	nullptr, /* del */
	nullptr, /* ctl */
#endif
	H5FD_FLMAP_SINGLE 				/*fl_map		*/
};

#endif





hid_t
H5FD_device_init()
{
	if (H5FD_DEVICE_g)
		return H5FD_DEVICE_g;
	else
	{
		H5FD_DEVICE_g = H5FDregister(&H5FD_device_g);
		return H5FD_DEVICE_g;
	}
} /* end H5FD_device_init() */



void
H5FD_device_term(void)
{
	H5FD_DEVICE_g = 0;
} /* end H5FD_device_term() */


qint64 vipH5OpenQIODevice(QIODevice * device)
{
	if (!device)
		return 0;

	hid_t faplist_id = H5Pcreate(H5P_FILE_ACCESS);
	/*herr_t status =*/ H5Pset_driver(faplist_id, H5FD_device_init(), nullptr);  

	QByteArray filename = QByteArray::number(((qint64)(device)));

	unsigned flags = 0;
	if (device->openMode() & QIODevice::WriteOnly) {
		flags = H5F_ACC_RDWR;
	}
	else
		flags = H5F_ACC_RDONLY;
	//if (device->openMode() & QIODevice::Truncate)
	//	flags |= H5F_ACC_TRUNC;

	return H5Fopen(filename.data(), flags, faplist_id);
}


static H5FD_t *
H5FD_device_open(const char *name, unsigned /*flags*/, hid_t /*fapl_id*/, haddr_t maxaddr)
{
	H5FD_device_t	*file = nullptr;   /* sec2 VFD info */
	
	/* Check arguments */
	if (!name || !*name)
	{
		return nullptr;
	}
		
	if (0 == maxaddr || HADDR_UNDEF == maxaddr)
	{
		VIP_LOG_ERROR("bogus maxaddr");
		return nullptr;
	}
	if (ADDR_OVERFLOW(maxaddr))
	{
		VIP_LOG_ERROR("bogus maxaddr");
		return nullptr;
	}

	qint64 addr = QString(name).toLongLong();
	QIODevice * dev = (QIODevice *)(addr);
	if (!dev)
	{
		VIP_LOG_ERROR("wrong device pointer");
		return nullptr;
	}

	/* Create the new file struct */
	if (nullptr == (file = (H5FD_device_t*)malloc(sizeof(H5FD_device_t))))
	{
		VIP_LOG_ERROR("unable to allocate file struct");
		return nullptr;
	}

	file->device = dev;
	file->eof = dev->size();
	file->eoa = file->eof;
	  /* Set return value */
	return(H5FD_t*)file;
} /* end H5FD_device_open() */


static herr_t
H5FD_device_close(H5FD_t *_file)
{
	H5FD_device_t	*file = (H5FD_device_t*)_file;

	if (file->device)
	{
		file->device->close();
		file->device = nullptr;
	}

	//free(file);
	return SUCCEED;
} /* end H5FD_device_close() */



static haddr_t
H5FD_device_get_eoa(const H5FD_t *_file, H5FD_mem_t  /*type*/)
{
	const H5FD_device_t	*file = (const H5FD_device_t*)_file;
	return file->eoa;
} /* end H5FD_device_get_eoa() */


static herr_t
H5FD_device_set_eoa(H5FD_t *_file, H5FD_mem_t /* type*/, haddr_t addr)
{
	H5FD_device_t	*file = (H5FD_device_t*)_file;
	file->eoa = addr;
	return SUCCEED;
} /* end H5FD_device_set_eoa() */




#if (H5_VERS_MAJOR*100 + H5_VERS_MINOR) < 110 
	static haddr_t
	H5FD_device_get_eof(const H5FD_t *_file)
	{
		const H5FD_device_t	*file = (const H5FD_device_t*)_file;
		//haddr_t ret_value;  /* Return value */
		return qMax(file->eof, file->eoa);
	}
#else
	static haddr_t
	H5FD_device_get_eof(const H5FD_t *_file, H5FD_mem_t)
	{
		const H5FD_device_t	*file = (const H5FD_device_t*)_file;
		//haddr_t ret_value;  /* Return value */
		return qMax(file->eof, file->eoa);
	}
#endif


static herr_t
H5FD_device_read(H5FD_t *_file, H5FD_mem_t  /*type*/, hid_t  /*dxpl_id*/,
	haddr_t addr, size_t size, void *buf/*out*/)
{
	H5FD_device_t		*file = (H5FD_device_t*)_file;

	if ((addr + size) > file->eoa)
	{
		VIP_LOG_ERROR("addr overflow, addr = " + QString::number((unsigned long long)addr));
		return -1;
	}

	/* Seek to the correct location */
	if (!file->device->seek(addr))
	{
		VIP_LOG_ERROR("unable to seek to proper position");
		return -1;
	}

	//read data
	qint64 nbytes = file->device->read((char*)buf, size);
	if(nbytes == -1)
	{
		VIP_LOG_ERROR("unable to read data");
		return -1;
	}

	return 0;
} /* end H5FD_device_read() */


static herr_t
H5FD_device_write(H5FD_t *_file, H5FD_mem_t  /*type*/, hid_t  /*dxpl_id*/, haddr_t addr,
	size_t size, const void *buf)
{
	H5FD_device_t		*file = (H5FD_device_t*)_file;
	
	if ((addr + size) > file->eoa)
	{
		VIP_LOG_ERROR("addr overflow, addr = " + QString::number((unsigned long long)addr));
		return -1;
	}
	/* Seek to the correct location */
	if (!file->device->seek(addr))
	{
		VIP_LOG_ERROR("unable to seek to proper position");
		return -1;
	}

	qint64 nbytes = file->device->write((const char*)buf, size);
	if(nbytes == -1)
	{
		VIP_LOG_ERROR("unable to seek to proper position");
		return -1;
	}

	file->eof = file->device->size();

	return 0;
} /* end H5FD_device_write() */

