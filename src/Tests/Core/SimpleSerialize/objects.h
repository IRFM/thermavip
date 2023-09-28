#include "VipProcessingObject.h"


class MultiplyNumericalValue : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipProperty factor)

public:
	double sum;

	MultiplyNumericalValue(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
		sum = 0;
	}

protected:

	virtual void apply() 
	{
		double input = inputAt(0)->data().value<double>();
		input *= propertyAt(0)->value<double>();
		sum += input;
	}
};