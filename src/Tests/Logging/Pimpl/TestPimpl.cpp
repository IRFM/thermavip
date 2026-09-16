/// @file TestPimpl.cpp
///
/// Characterisation tests for the private data carrier: who a block of private
/// data belongs to, and what the validity oracle answers.

#include <QTest>

#include "vip_test_main.h"

#include "VipPimpl.h"
#include "VipText.h"

#include <type_traits>
#include <utility>

class TestPimpl : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// A block of private data remembers the address of the object that owns it.
	/// Moving the carrier itself carried the block over without rebinding it, so
	/// the object moved to was reported invalid and the deregistration and the
	/// destroyed signal went to the one left behind. Nothing inside the carrier
	/// can know its new owner, so the move is now a compile error.
	void theCarrierOfPrivateDataCannotBeMoved()
	{
		using Carrier = pimpl_detail::InternalDataPtr<int>;
		QVERIFY(!std::is_move_constructible<Carrier>::value);
		QVERIFY(!std::is_move_assignable<Carrier>::value);
		QVERIFY(!std::is_copy_constructible<Carrier>::value);
		QVERIFY(!std::is_copy_assignable<Carrier>::value);
	}

	/// The one class of the SDK that moves its private data moves the data, not
	/// the block: the object moved to is the valid one, and it carries the text.
	void aMovedTextObjectIsTheValidOne()
	{
		VipTextObject source(VipText("measured"), QRectF(0, 0, 10, 10));
		QVERIFY(vipIsObjectValid(&source));

		VipTextObject target(std::move(source));

		QVERIFY2(vipIsObjectValid(&target), "the object moved to must be the valid one");
		QCOMPARE(target.text().text(), QString("measured"));
	}

	/// Same through the assignment.
	void aTextObjectAssignedFromAMoveIsTheValidOne()
	{
		VipTextObject source(VipText("second"), QRectF(0, 0, 10, 10));
		VipTextObject target(VipText("first"), QRectF(0, 0, 10, 10));

		target = std::move(source);

		QVERIFY(vipIsObjectValid(&target));
		QCOMPARE(target.text().text(), QString("second"));
	}
};

VIP_TEST_MAIN(TestPimpl)
#include "TestPimpl.moc"
