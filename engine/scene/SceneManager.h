#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>
#include "Scene.h"
#include "core/Logger.h"
#include "core/EventBus.h"
#include "core/Events.h"

namespace FaluEngine {

struct SceneChangeEvent {
	std::string from;
	std::string to;
};

class SceneManager {
public:
	static SceneManager& get() {
		static SceneManager instance;
		return instance;
	}

	template<typename T,typename... Args>
	void registerScene(const std::string& name, Args&&... args) {
		m_scenes[name] = std::make_shared<T>(std::forward<Args>(args)...);
		LOG_INFO("SceneManager: registered '{}'", name);
	}

	void registerEmptyScene(const std::string& name)
	{
		if (m_scenes.count(name)) return;
		m_scenes[name] = std::make_shared<Scene>(name);
		LOG_INFO("SceneManager: registered '{}'", name);
	}

	void switchTo(const std::string& name) {
		auto it = m_scenes.find(name);
		if (it == m_scenes.end()) {
			LOG_ERROR("SceneManager: scene '{}' not found", name);
			return;
		}

		std::string fromName = m_active ? m_active->getName() : "none";
		
		if (m_active)m_active->onExit();
		m_active = it->second;
		m_active->onEnter();

		EventBus::get().publish(SceneChangeEvent{ fromName,name });
		LOG_INFO("SceneManager: '{}' -> '{}'", fromName, name);
	}

	void loadSceneFromFile(const std::string& path);

	void scanSceneFolder(const std::filesystem::path& scenesDir);

	void createNewScene(const std::string& name);

	void onUpdate(float deltaTime) {
		if (m_active) m_active->onUpdate(deltaTime);
	}

	void onRender() {
		if (m_active) m_active->onRender();
	}

	[[nodiscard]] Scene* getActive() const noexcept { return m_active.get(); }
	[[nodiscard]] bool haActive() const noexcept { return m_active != nullptr; }

	/// @brief ƒV[ƒ“ˆê——æ“¾
	/// @return 
	[[nodiscard]] std::vector<std::string> getSceneNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_scenes.size());
		for (auto& [name, scene] : m_scenes)
			names.push_back(name);
		return names;
	}

	[[nodiscard]] std::string getScenePath(const std::string& name)const
	{
		auto it = m_scenePaths.find(name);
		return it != m_scenePaths.end() ? it->second : "";
	}

	void setScenePath(const std::string& name, const std::string& path)
	{
		m_scenePaths[name] = path;
	}

private:
	SceneManager() = default;

	std::unordered_map<std::string, std::shared_ptr<Scene>> m_scenes;
	std::unordered_map<std::string, std::string> m_scenePaths;
	std::shared_ptr<Scene> m_active;
};

}
