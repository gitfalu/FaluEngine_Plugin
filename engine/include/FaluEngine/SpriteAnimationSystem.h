#pragma once
#include <entt/entt.hpp>
#include <cstdint>
#include <string>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	/// @brief スプライトクリップのイベント発火時にEventBusへPublishされる
	struct SpriteAnimationEvent
	{
		uint32_t entity = 0; // entt::entity の数値
		uint64_t entityUUID = 0; // IDComponentがあればuuid(無ければ0)
		std::string name;
		std::string payload;
	};

	/// @brief SpriteAnimatorComponent を進め、同じEntityのImageComponentの
	///			texturePath / uvRect / pointFilter を置き換える
	///			Entityの生成・破棄は行わない(イベントハンドラ内での破棄は遅延させること)
	/// @param registry 
	/// @param deltaTime 
	/// @return 
	FALU_SCENE_API void updateSpriteAnimations(entt::registry& registry, float deltaTime);

	/// @brief 
	/// @param registry 
	/// @param entity 
	/// @return 
	FALU_SCENE_API void applySprteAnimatorFrame(entt::registry& registry, entt::entity entity);
}
