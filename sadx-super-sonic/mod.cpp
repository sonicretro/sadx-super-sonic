#include "stdafx.h"
#include <SADXModLoader.h>

#define EXPORT __declspec(dllexport)

DataPointer(NJS_TEXLIST, SUPERSONIC_TEXLIST, 0x0142272C);
DataPointer(bool, SuperSonicFlag, 0x03C55D45);

static const PVMEntry SuperSonicPVM = { "SUPERSONIC", &SUPERSONIC_TEXLIST };

void __cdecl Sonic_SuperPhysics_Delete(ObjectMaster* _this)
{
	char index = *(char*)_this->UnknownB_ptr;
	CharObj2* data2 = CharObj2Ptrs[index];

	if (data2 != nullptr)
		data2->PhysicsData = PhysicsArray[Characters_Sonic];
}

extern "C"
{
	void EXPORT Init(const char* path, HelperFunctions* helper)
	{
		helper->RegisterCharacterPVM(Characters_Sonic, SuperSonicPVM);
		WriteData((void**)0x004943C2, (void*)Sonic_SuperPhysics_Delete);
	}

	void EXPORT OnFrame()
	{
		if (GameState != 15)
			return;

		for (Uint32 i = 0; i < 8; i++)
		{
			CharObj1* data1 = CharObj1Ptrs[i];
			CharObj2* data2 = CharObj2Ptrs[i];

			if (data1 == nullptr || data1->CharID != Characters_Sonic)
				continue;

			if (ControllerPointers[i]->PressedButtons & Buttons_Z)
			{
				// I'm confused by this. Every time I check this, that bit isn't there,
				// but the super physics object checks for that bit and it IS there,
				// otherwise it would restore the original physics.
				// Consequently, I'm setting the bit myself.
				if ((data2->Upgrades & Upgrades_SuperSonic) != Upgrades_SuperSonic)
				{
					// Transform into Super Sonic
					data1->NextAction = 46;
					data1->Status |= Status_DoNextAction;
					data2->Upgrades |= Upgrades_SuperSonic;
				}
				else
				{
					// Change back to normal Sonic
					data1->NextAction = 47;
					data1->Status |= Status_DoNextAction;
					data2->Upgrades &= ~Upgrades_SuperSonic;
				}
			}
		}
	}

	EXPORT ModInfo SADXModInfo = { ModLoaderVer };
}
