#include "objects.h"

#include "VipXmlArchive.h"

int main(int argc, char** argv)
{
	// register serialization functions
	vipRegisterArchiveStreamOperators<BaseClass*>();
	vipRegisterArchiveStreamOperators<DerivedClass*>();

	// Create archive to serialize to XML
	VipXOStringArchive arch;



	return 0;
}