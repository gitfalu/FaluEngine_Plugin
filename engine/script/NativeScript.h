#pragma once

namespace FaluEngine
{
	class Entity;

	class NativeScript
	{
	public:
		virtual ~NativeScript() = default;
		virtual void onInit(Entity& entity) {}
		virtual void onUpdate(Entity& entity,float deltaTime) {}
		virtual void onDestroy(Entity& entity) {}
		virtual void onClick(Entity& entity) {}
	};
}
