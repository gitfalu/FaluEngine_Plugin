/*****************************************************************//**
 * \file   WindComponent.h
 * \brief  シーン全体に影響する風を起こすコンポーネント
 * 
 * \author falu
 * \date   November 2026
 *********************************************************************/
#pragma once
#include "WindField.h"
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	/// @brief シーン全体に風を起こすコンポーネント
	struct FALU_SCENE_API WindComponent
	{
		bool enabled = true;
		WindParams params{ 0.0f,0.0f,4.0f,0.4f,0.3f,0.25f,0.35f };
	};
}
