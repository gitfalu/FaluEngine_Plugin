#include "SceneManager.h"
#include "SceneSerializer.h"
#include <algorithm>

namespace FaluEngine
{
	void SceneManager::loadSceneFromFile(const std::string& path)
	{
		std::string name = std::filesystem::path(path).stem().string();

		registerEmptyScene(name);
		setScenePath(name, path);

		switchTo(name);

		if (m_active)
		{
			SceneSerializer serializer(*m_active);
			serializer.deserialize(path);
		}
	}

	void SceneManager::scanSceneFolder(const std::filesystem::path& scenesDir)
	{
		if (!std::filesystem::exists(scenesDir))
		{
			LOG_WARN("SceneManager: scenes folder not found '{}'", scenesDir.string());
			return;
		}

		for (const auto& entry : std::filesystem::directory_iterator(scenesDir))
		{
			if (entry.path().extension() != ".scene") continue;

			std::string name = entry.path().stem().string();
			std::string path = entry.path().string();

			registerEmptyScene(name);
			setScenePath(name, path);

			LOG_INFO("SceneManager: found scene file '{}' -> '{}'", name, path);
		}
	}

	void SceneManager::createNewScene(const std::string& name)
	{
		registerEmptyScene(name);
		switchTo(name);
	}
}

