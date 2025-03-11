#pragma once

#include <thread>

#include <qpushbutton.h>
#include <qboxlayout.h>

#include "VipDoubleSlider.h"
#include "VipSliderGrip.h"
#include "VipIODevice.h"

/// @brief Multithreaded Mandelbrot set image generator
class Mandelbrot
{
	int MAX;

	VIP_ALWAYS_INLINE int mandelbrot(double startReal, double startImag) const
	{
		double zReal = startReal;
		double zImag = startImag;

		for (int counter = 0; counter < MAX; ++counter) {
			double r2 = zReal * zReal;
			double i2 = zImag * zImag;
			if (r2 + i2 > 4.0)
				return counter;
			zImag = 2.0 * zReal * zImag + startImag;
			zReal = r2 - i2 + startReal;
		}
		return MAX;
	}

	void updateImageSlice(double zoom, double offsetX, double offsetY, VipNDArrayTypeView<int> image) const
	{
		const int height = image.shape(0);
		const int width = image.shape(1);
		double real = 0 * zoom - width / 2.0 * zoom + offsetX;
		const double imagstart = -height / 2.0 * zoom + offsetY;
		int* img = image.ptr();

#pragma omp parallel for
		for (int y = 0; y < height; y++) {
			double iv = imagstart + y * zoom;
			int* p = img + y * width;
			double rv = real;
			for (int x = 0; x < width; x++, rv += zoom) {
				p[x] = mandelbrot(rv, iv);
			}
		}
	}

public:
	Mandelbrot(int max = 0)
	  : MAX(max)
	{
		if (max == 0)
			MAX = std::thread::hardware_concurrency() * 32 - 1;
	}
	void updateImage(double zoom, double offsetX, double offsetY, VipNDArrayTypeView<int> image) { updateImageSlice(zoom, offsetX, offsetY, (image)); }
};


/// @brief A temporal VipIODevice that generates images from the mandelbrot set.
class MandelbrotDevice : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput image)
public:
	MandelbrotDevice(QObject* parent = nullptr)
	  : VipTimeRangeBasedGenerator(parent)
	{
		// set output data
		outputAt(0)->setData(VipNDArray());

	}

	virtual VipIODevice::OpenModes supportedModes() const { return ReadOnly; }
	virtual DeviceType deviceType() const { return Temporal; }
	virtual bool open(VipIODevice::OpenModes modes)
	{
		if (modes != ReadOnly)
			return false;

		
		// build timestamps and zooms
		double zoom = (0.004);
		VipTimestamps times;
		while (zoom > 2.38339e-13) {
			zooms.push_back(zoom);
			times.push_back(times.size());
			zoom *= 0.96;
		}
		setTimestamps(times);

		setOpenMode(modes);
		return true;
	}

protected:

	QVector<double> zooms;

	virtual bool readData(qint64 time)
	{
		// compute position
		qint64 pos = computeTimeToPos(time);
		if(pos < 0 || pos >= size())
			return false;

		// build image
		Mandelbrot gen(383);
		VipNDArray ar(qMetaTypeId<int>(), vipVector(420, 640));
		double offsetX = (-0.745917);
		double offsetY = (0.09995);
		gen.updateImage(zooms[pos], offsetX, offsetY, VipNDArrayTypeView<int>(ar));

		// build output data
		VipAnyData any = create(QVariant::fromValue(ar));
		any.setTime(time);

		// send image to output
		outputAt(0)->setData(any);
		return true;
	}
};


/// @brief Minimalist play widget to control a VipProcessingPool
class PlayWidget : public QWidget
{
	Q_OBJECT

	QPushButton* play;
	VipDoubleSliderWidget* slider;
	VipProcessingPool* pool;

public:
	PlayWidget(VipProcessingPool * p, QWidget* parent = nullptr)
	  :QWidget(parent), pool(p)
	{
		// the pool in repeat mode
		pool->setMode(VipProcessingPool::Repeat);

		QHBoxLayout* lay = new QHBoxLayout();
		setLayout(lay);

		play = new QPushButton();
		play->setText("Play");

		slider = new VipDoubleSliderWidget(VipBorderItem::Bottom);
		slider->slider()->setScale(p->firstTime(), p->lastTime());
		slider->slider()->setSingleStep(1, 0);
		slider->slider()->setSingleStepEnabled(true);
		slider->slider()->scaleDraw()->setTicksPosition(VipScaleDraw::TicksOutside);
		slider->slider()->grip()->setHandleDistance(10);
		slider->slider()->setMouseClickEnabled(true);
		slider->setStyleSheet("background: transparent");

		lay->addWidget(play);
		lay->addWidget(slider);

		connect(play, SIGNAL(clicked(bool)), this, SLOT(playStop()));
		connect(slider, SIGNAL(valueChanged(double)), this, SLOT(setTime(double)));
		connect(pool, SIGNAL(timeChanged(qint64)), this, SLOT(timeChanged(qint64)));
	}


public Q_SLOTS:

	void playStop() { 
		if (pool->isPlaying()) {
			pool->stop();
			play->setText("Play");
		}
		else {
			pool->play();
			play->setText("Stop");
		}
	}
	void timeChanged(qint64 time) { 
		slider->blockSignals(true);
		slider->setValue(time);
		slider->blockSignals(false);
	}
	void setTime(double time) { 
		pool->seek(time);
	}
};