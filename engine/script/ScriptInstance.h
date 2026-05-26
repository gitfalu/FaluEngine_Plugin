#pragma once 
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <string>

namespace FaluEngine {
	class Entity;


	class ScriptInstance {
	public:
		ScriptInstance(sol::state& lua, const std::string& scriptPath);
		~ScriptInstance();

		ScriptInstance(const ScriptInstance&) = delete;
		ScriptInstance& operator=(const ScriptInstance&) = delete;
		ScriptInstance(ScriptInstance&&) = default;
		ScriptInstance& operator=(ScriptInstance&&) = default;

		void onInit(Entity& entity);
		void onUpdate(Entity& entity, float deltaTime);
		void onDestroy(Entity& entity);

		[[nodiscard]] bool isValid() const noexcept { return m_valid; }
		[[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }
	private:
		sol::environment m_env;
		sol::safe_function m_onInit;
		sol::safe_function m_onUpdate;
		sol::safe_function m_onDestroy;

		bool m_valid = false;
		bool m_initialized = false;
	};
}
