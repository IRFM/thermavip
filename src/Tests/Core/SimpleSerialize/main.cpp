#include "objects.h"

#include "VipXmlArchive.h"

#include <iostream>


#include <VipFunctionTraits.h>

template<class C>
void print_info(C&& c)
{
	using traits = VipFunctionTraits<C>;
	std::cout << "Return: " << typeid(typename traits::return_type).name() << std::endl;
	std::cout << "Signature: " << typeid(typename traits::signature_type).name() << std::endl;
	std::cout << "Arity: " <<traits::nargs << std::endl;
	std::cout << "Type (0): " << typeid(typename traits::element<0>::type).name() << std::endl;
	std::cout << "Type (1): " << typeid(typename traits::element<1>::type).name() << std::endl;
}


struct test_fun
{
	double operator()(int a, double b) { return a + b; }
};

int main(int argc, char** argv)
{
	const std::tuple<int, double> t(1, 2.2);
	auto ret = vipApply(test_fun{}, t);

	print_info(test_fun{});

	// register serialization functions
	vipRegisterArchiveStreamOperators<BaseClass*>();
	vipRegisterArchiveStreamOperators<DerivedClass*>();

	// Create a DerivedClass object, but manipulate it through a QObject
	DerivedClass* derived = new DerivedClass(4, 5.6);
	QObject* object = derived;

	// Create archive to serialize object to XML
	VipXOStringArchive arch;
	if (!arch.content("Object", object)) {
		std::cerr << "An error occured while writing to archive!" << std::endl;
		return -1;
	}
	
	// get XML output 
	QString content = arch.toString();

	// output the resulting xml
	std::cout << content.toLatin1().data() << std::endl;


	// modify object's value to be sure of the result
	derived->ivalue = 23;
	derived->dvalue = 45.6;

	// Now read back the XML content
	VipXIStringArchive iarch(content);
	if (!iarch.content("Object", object)) {
		std::cerr << "An error occured while reading archive!" << std::endl;
		return -1;
	}

	// check the result
	if (derived->ivalue == 4 && derived->dvalue == 5.6) {
		std::cout << "Read archive success!" << std::endl;
	}


	// now another option: use the builtin factory to read the archive and create a new DerivedClass
	iarch.open(content);
	QVariant var = iarch.read("Object");
	DerivedClass * derived2 = var.value<DerivedClass*>();

	//check again the result
	if (derived2->ivalue == 4 && derived2->dvalue == 5.6) {
		std::cout << "Read archive success (again)!" << std::endl;
	}

	// we just created a new object, destroy it
	delete derived2;
	delete derived;

	return 0;
}