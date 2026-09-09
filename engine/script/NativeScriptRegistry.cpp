#include "FaluEngine/NativeScriptRegistry.h"

namespace FaluEngine
{
	NativeScriptRegistry& NativeScriptRegistry::get()
	{
		static NativeScriptRegistry instance;
		return instance;
	}
}
