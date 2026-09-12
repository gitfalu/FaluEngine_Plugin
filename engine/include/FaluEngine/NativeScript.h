#pragma once
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	class Entity;

	class FALU_ENGINE_API NativeScript
	{
	public:
		virtual ~NativeScript() = default;
		virtual void onInit(Entity& entity) {}
		virtual void onUpdate(Entity& entity,float deltaTime) {}
		virtual void onDestroy(Entity& entity) {}
		virtual void onClick(Entity& entity) {}
	};
}
