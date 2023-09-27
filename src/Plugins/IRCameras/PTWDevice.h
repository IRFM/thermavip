#ifndef PTW_LOADER_H
#define PTW_LOADER_H


#include "VipIODevice.h"



///////////////////////////////////////////////////////////////////////////
/// @class PTWDevice
/// @brief Loader for Jet Processing Format PTW video files.
///        Inherits from the generic data loader class : IVideoLoader
///////////////////////////////////////////////////////////////////////////
class PTWDevice : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	Q_PROPERTY(VipOutput image)

public:

    
    PTWDevice();
    virtual ~PTWDevice();

	virtual bool open(VipIODevice::OpenModes mode);
	virtual QString fileFilters() const { return "PTW video file (*.ptw)"; }
	virtual void close();
	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}

protected:

	virtual bool readData(qint64 time);

private:
    
    
    // -----------------------------------------------------------------------
    //  Private data members
    // -----------------------------------------------------------------------
    /** @name Private data members */
    //@{
    // Image width (pixels)
    unsigned int            m_width;
    // Image height (pixels)
    unsigned int            m_height;
    // left coord of the window
    quint16         		m_left;
    // top coord of the window
    quint16         		m_top;
    //
    unsigned int            m_bytePerPixel;
    //
    unsigned int            m_FileHeaderSize;
    unsigned int            m_FrameHeaderSize;
    quint32         		m_ui32_MovieSize;
    // Number of frames of the movie
    quint32         		m_ui32_NbFrames;
    // Current image
    VipNDArray              m_pt_CurrentImage;
    // Video interlaced or not
    bool                    m_b_IsInterlaced;
    // Video is a file or is on the JAC
    bool                    m_b_IsFromFile;
    //
    QString             	m_s_OpenString;
    //
    QString             	m_s_CameraName;
    //
    quint32         		m_ui32_PulseNumber;
    double					m_sampling_time;
    //@}
};

VIP_REGISTER_QOBJECT_METATYPE(PTWDevice*)


#endif // _PTW_LOADER_H_
