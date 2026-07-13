#include "EditorStateManager.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include "physics/PhysicsSystem.h"
#include "core/Logger.h"
#include <sstream>
#include <fstream>
#include <filesystem>

namespace FaluEngine
{
	void EditorStateManager::play(Scene& scene)
	{
		if (m_state != PlayState::Editing) return;

		std::string tempPath = (std::filesystem::temp_directory_path() /
			"falu_engine_play_snapshot.scene").string();

		SceneSerializer serializer(scene);
		if (serializer.serialize(tempPath))
		{
			std::ifstream file(tempPath);
			std::ostringstream ss;
			ss << file.rdbuf();
			m_snapshotJson = ss.str();
		}
	
		m_state = PlayState::Playing;
		LOG_INFO("EditorStateManager: Play started (snapshot saved)");
	}

	void EditorStateManager::pause()
	{
		if (m_state != PlayState::Playing) return;
		m_state = PlayState::Paused;
		LOG_INFO("EditorStateManager: Paused");

	}

	void EditorStateManager::resume()
	{
		if (m_state != PlayState::Paused) return;
		m_state = PlayState::Playing;
		LOG_INFO("EditorStateManager: Resumed");
	}

	void EditorStateManager::stop(Scene& scene)
	{
		if (m_state == PlayState::Editing) return;

		// Entity‘I‘ð‚Ì‰ðœ
		if (m_onBeforeShop) m_onBeforeShop();

		if (!m_snapshotJson.empty())
		{
			std::string tempPath = (std::filesystem::temp_directory_path() /
				"falu_engine_play_snapshot.scene").string();

			std::ofstream file(tempPath);
			file << m_snapshotJson;
			file.close();

			PhysicsSystem::get().unregisterScene(scene);

			SceneSerializer serializer(scene);
			serializer.deserialize(tempPath);

			PhysicsSystem::get().registerScene(scene);
		}

		m_snapshotJson.clear();
		m_state = PlayState::Editing;
		LOG_INFO("EditorStateManager: Stopped (scene restored)");
	}
}
