#include "objects.h"

#include "VipXmlArchive.h"

#include <iostream>

 
int main(int , char** )
{
	// register serialization functions
	vipRegisterArchiveStreamOperators<BaseClass*>();
	vipRegisterArchiveStreamOperators<DerivedClass*>();

	// Create a DerivedClass object, but manipulate it through a QObject
	DerivedClass* derived = new DerivedClass(4, 5.6);
	QObject* object = derived;

	// Create archive to serialize object to XML buffer
	VipXOStringArchive arch;

	// Serialize a DerivedClass object through a QObject pointer
	if (!arch.content("Object", object)) {
		std::cerr << "An error occured while writing to archive!" << std::endl;
		return -1;
	}
	
	
	// output the resulting xml content
	std::cout << arch.toString().toLatin1().data() << std::endl;


	// modify object's value to be sure of the result
	derived->ivalue = 23;
	derived->dvalue = 45.6;

	// Now read back the XML content
	VipXIStringArchive iarch(arch.toString());
	if (!iarch.content("Object", object)) {
		std::cerr << "An error occured while reading archive!" << std::endl;
		return -1;
	}

	// check the result
	if (derived->ivalue == 4 && derived->dvalue == 5.6) {
		std::cout << "Read archive success!" << std::endl;
	}


	// now another option: use the builtin factory to read the archive and create a new DerivedClass
	// Each step can fail on a truncated or unexpected archive, and an example is
	// what readers copy: check the open, the read and the type before using them.
	if (!iarch.open(arch.toString())) {
		std::cerr << "Cannot open archive" << std::endl;
		return 1;
	}
	QVariant var = iarch.read("Object");
	DerivedClass * derived2 = var.value<DerivedClass*>();
	if (iarch.hasError() || !derived2) {
		std::cerr << "Cannot read a DerivedClass from the archive" << std::endl;
		return 1;
	}

	//check again the result
	if (derived2->ivalue == 4 && derived2->dvalue == 5.6) {
		std::cout << "Read archive success (again)!" << std::endl;
	}

	// we just created a new object, destroy it
	delete derived2;
	delete derived;

	return 0;
}