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

#ifndef VIP_PROCESSING_BLOCK_H
#define VIP_PROCESSING_BLOCK_H


#include "VipProcessingObject.h"

/// @brief Block of connected processings.
///
/// VipProcessingBlock behaves like a function: it encapsulate one
/// or more processing pipeline which are connected to the VipProcessingBlock
/// inputs/outputs/properties.
/// 
/// Processings are added to a VipProcessingBlock using QObjecy children mechanism:
/// setting a VipProcessingObject parent to a VipProcessingBlock will add this
/// processing to the VipProcessingBlock.
/// 
/// All processings added to a VipProcessingBlock are made synchronous. the 
/// VipProcessingBlock will destroy its internal processing pipelines on destruction.
/// 
/// Use members setInputConnection(), setPropertyConnection() and setOutputConnection()
/// to connect internal processings IO to the VipProcessingBlock IO. These functions
/// must be called AFTER adding the processing to its parent VipProcessingBlock.
/// 
class VIP_CORE_EXPORT VipProcessingBlock : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipMultiProperty properties)
	VIP_IO(VipMultiInput inputs)
	VIP_IO(VipMultiOutput outputs)
public:
	VipProcessingBlock(QObject* parent = nullptr);
	~VipProcessingBlock();

	/// @brief Connect processing input
	/// 
	/// Connect a VipProcessingBlock input to one of its internal
	/// processing input. Setting the VipProcessingBlock input data 
	/// will internaly set the input of the child processing.
	/// 
	/// This function must be called AFTER resizing the VipProcessingBlock
	/// input list, and AFTER adding the child processing (owning the passed VipInput)
	/// to the VipProcessingBlock.
	/// 
	/// Note that a VipProcessingBlock input can be forwarded to multiple internal processing
	/// inputs.
	/// 
	/// @param index input index in the VipProcessingBlock
	/// @param input input of an internal processing
	/// @return true on success, false otherwise.
	bool setInputConnection(int index, VipInput* input);
	QVector<VipInput*> inputConnection(int index) const;

	/// @brief Connect processing property
	///
	/// Connect a VipProcessingBlock property to one of its internal
	/// processing property. Setting the VipProcessingBlock property value
	/// will internaly set the property of the child processing.
	///
	/// This function must be called AFTER resizing the VipProcessingBlock
	/// property list, and AFTER adding the child processing (owning the passed VipProperty)
	/// to the VipProcessingBlock.
	/// 
	/// Note that a VipProcessingBlock property can be forwarded to multiple internal processing
	/// properties.
	///
	/// @param index property index in the VipProcessingBlock
	/// @param input property of an internal processing
	/// @return true on success, false otherwise.
	bool setPropertyConnection(int index, VipProperty* outut);
	QVector<VipProperty*> propertyConnection(int index) const;

	/// @brief Connect processing input
	///
	/// Connect a VipProcessingBlock input to one of its internal
	/// processing input. Setting the VipProcessingBlock input data
	/// will internaly set the input of the child processing.
	///
	/// This function must be called AFTER resizing the VipProcessingBlock
	/// input list, and AFTER adding the child processing (owning the passed VipInput)
	/// to the VipProcessingBlock.
	///
	/// @param index input index in the VipProcessingBlock
	/// @param input input of an internal processing
	/// @return true on success, false otherwise.
	bool setOutputConnection(int index, VipOutput* outut);
	VipOutput* outputConnection(int index) const;

	virtual VipProcessingObject::DisplayHint displayHint() const;
	void setDisplayHint(VipProcessingObject::DisplayHint);

	virtual bool useEventLoop() const;
	virtual bool acceptInput(int index, const QVariant&  v) const;
	virtual bool acceptProperty(int  index, const QVariant& v) const;
	virtual void setSourceProperty(const char* name, const QVariant& value);

protected:
	virtual void apply();
	virtual void resetProcessing();
	virtual QTransform imageTransform(bool* from_center) const;

	virtual void childEvent(QChildEvent*);

private Q_SLOTS:
	void receiveNewError(QObject*, const VipErrorData&);

private:
	void computeLeaves();
	QTransform computeTransform() const;

	VIP_DECLARE_PRIVATE_DATA();
};

VIP_REGISTER_QOBJECT_METATYPE(VipProcessingBlock*)
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& stream, const VipProcessingBlock* r);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& stream, VipProcessingBlock* r);

/*
	{
		// TEST
		VipProcessingBlock block;
		block.topLevelPropertyAt(0)->toMultiProperty()->resize(2);
		block.topLevelInputAt(0)->toMultiInput()->resize(2);
		block.topLevelOutputAt(0)->toMultiOutput()->resize(2);

		VipClamp* cl1 = new VipClamp(&block);
		VipClamp* cl2 = new VipClamp(&block);

		VipAbs* a1 = new VipAbs(&block);
		VipAbs* a2 = new VipAbs(&block);

		cl1->outputAt(0)->setConnection(a1->inputAt(0));
		cl2->outputAt(0)->setConnection(a2->inputAt(0));

		block.setInputConnection(0, cl1->inputAt(0));
		block.setInputConnection(1, cl2->inputAt(0));
		block.setOutputConnection(0, a1->outputAt(0));
		block.setOutputConnection(1, a2->outputAt(0));
		block.setPropertyConnection(0, cl1->propertyAt(0));
		block.setPropertyConnection(1, cl1->propertyAt(1));
		block.setPropertyConnection(0, cl2->propertyAt(0));
		block.setPropertyConnection(1, cl2->propertyAt(1));

		block.propertyAt(0)->setData(-10);
		block.propertyAt(1)->setData(10);

		for (int i = -20; i < 20; ++i) {
			block.inputAt(0)->setData(i);
			block.inputAt(1)->setData(i);
			block.update();

			printf("%i %i\n", block.outputAt(0)->value<int>(), block.outputAt(1)->value<int>());
		}

		bool stop = true;

		VipXOStringArchive arch;
		arch.content("block", &block);
		QString xml = arch.toString();
		printf("%s\n", xml.toLatin1().data());

		VipXIStringArchive arch2(xml);
		VipProcessingBlock* block2 = arch2.read("block").value<VipProcessingBlock*>();
		block2->propertyAt(0)->setData(-15);
		block2->propertyAt(1)->setData(15);
		for (int i = -20; i < 20; ++i) {
			block2->inputAt(0)->setData(i);
			block2->inputAt(1)->setData(i);
			block2->update();

			printf("%i %i\n", block2->outputAt(0)->value<int>(), block2->outputAt(1)->value<int>());
		}
		stop = true;
	}
	*/
#endif