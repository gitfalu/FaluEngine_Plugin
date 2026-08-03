#include "ScriptInstance.h"
#include "ScriptEngine.h"
#include "core/Logger.h"
#include "scene/Entity.h"

namespace FaluEngine {
	ScriptInstance::ScriptInstance(sol::state& lua, const std::string& scriptPath)
	{
		m_env = sol::environment(lua, sol::create, lua.globals());

		auto result = lua.safe_script_file(
			scriptPath,
			m_env,
			sol::script_pass_on_error
		);

		if (!result.valid()) {
			sol::error err = result;
			LOG_ERROR("ScriptInstance: failed to load '{}':{}", scriptPath, err.what());
			m_valid = false;
			return;
		}

		m_onInit = m_env["onInit"];
		m_onUpdate = m_env["onUpdate"];
		m_onDestroy = m_env["onDestroy"];
		m_onClick = m_env["onClick"];

		m_valid = true;
		LOG_INFO("ScriptInstance: loaded '{}'", scriptPath);
	}

	ScriptInstance::~ScriptInstance()
	{
		m_onInit = sol::nil;
		m_onUpdate = sol::nil;
		m_onDestroy = sol::nil;
		m_env = sol::environment{};
	}

	void ScriptInstance::onInit(Entity& entity)
	{
		if (!m_valid || m_initialized) return;
		if (m_onInit.valid()) {
			auto result = m_onInit(entity);
			if (!result.valid()) {
				sol::error err = result;
				LOG_ERROR("ScriptInstance onInit error: {}", err.what());
			}
		}

		m_initialized = true;
	}

	void ScriptInstance::onUpdate(Entity& entity, float deltaTime)
	{
		if (!m_valid || !m_initialized) return;
		if (m_onUpdate.valid()) {
			auto result = m_onUpdate(entity, deltaTime);
			if (!result.valid()) {
				sol::error err = result;
				LOG_ERROR("ScriptInstance onUpdate error: {}", err.what());
			}
		}
	}

	void ScriptInstance::onDestroy(Entity& entity)
	{
		if (!m_valid) return;
		if (m_onDestroy.valid()) {
			auto result = m_onDestroy(entity);
			if (!result.valid()) {
				sol::error err = result;
				LOG_ERROR("ScriptInstance onDestroy error: {}", err.what());
			}
		}
	}
	void ScriptInstance::onClick(Entity& entity)
	{
		if (!m_valid) return;
		if (m_onClick.valid()) {
			auto result = m_onClick(entity);
			if (!result.valid()) {
				sol::error err = result;
				LOG_ERROR("ScriptInstance onClick error: {}", err.what());
			}
		}
	}
}
