#pragma once
#include <string>


namespace FaluEngine
{
	class Scene;

	class SceneSerializer
	{
	public:
		explicit SceneSerializer(Scene& scene) : m_scene(scene) {}

		/// @brief シリアライズ
		/// @param path 
		/// @return 
		bool serialize(const std::string& path) const;

		bool deserialize(const std::string& path);

	private:
		Scene& m_scene;

	};
}
