#include "VipProcessingObject.h"


class MultiplyNumericalValue : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipProperty factor)

public:
	VipAnyDataList lst;
	double sum;

	MultiplyNumericalValue(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
		sum = 0;
		//lst.reserve(10000);
	}

protected:

	virtual void apply() 
	{
		// This also works
		/* double mul = propertyAt(0)->value<double>();
		if (inputAt(0)->allData(lst)) {

			for (const VipAnyData& a : lst)
				sum += a.value<double>() * mul;
		}*/
		

		double input = inputAt(0)->data().value<double>();
		input *= propertyAt(0)->value<double>();
		sum += input;
	}
};