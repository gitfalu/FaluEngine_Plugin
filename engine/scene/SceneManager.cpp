#include "SceneManager.h"
#include "SceneSerializer.h"
#include "core/PathResolver.h"
#include <algorithm>

namespace FaluEngine
{
	SceneManager& SceneManager::get()
	{
		static SceneManager instance;
		return instance;
	}


	void SceneManager::loadSceneFromFile(const std::string& path)
	{
		const std::string assetPath =
			PathResolver::normalizeAssetPath(path);

		const auto fsPath =
			PathResolver::resolve(assetPath);

		std::string name =
			PathResolver::toUtf8(fsPath.stem());

		registerEmptyScene(name);
		setScenePath(name, assetPath);

		switchTo(name);

		if (m_active)
		{
			SceneSerializer serializer(*m_active);

			if (!serializer.deserialize(assetPath))
			{
				LOG_ERROR("SceneManager: failed to load scene '{}'",
					assetPath);
			}
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
			const auto absolutePath = std::filesystem::absolute(entry.path()).lexically_normal();
			std::string name = PathResolver::toUtf8(absolutePath.stem());

			std::string assetPath = PathResolver::toAssetPath(absolutePath);

			if (assetPath.empty()) continue;

			registerEmptyScene(name);
			setScenePath(name, assetPath);

			LOG_INFO("SceneManager: found scene file '{}' -> '{}'", name, assetPath);
		}
	}

	void SceneManager::createNewScene(const std::string& name)
	{
		registerEmptyScene(name);
		switchTo(name);
	}
}

