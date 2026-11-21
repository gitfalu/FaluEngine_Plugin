/*****************************************************************//**
 * \file   RippleComponent.h
 * \brief  水面のパラメータを定義するスクリプト
 * 
 * \author tsunn
 * \date   November 2026
 *********************************************************************/
#pragma once
#include "RippleSolver.h"
#include <vector>
#include <cstdint>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	/// @brief シミュレーション負荷の計算値(Inspector表示・負荷チェック用)
	struct SimStats
	{
		float stepMs = 0.0f; // CPUシミュ時間(リアルタイム
		float meshMs = 0.0f; // 頂点生成+GPUアップロード時間(リアルタイム
		float avgTotalMs = 0.0f; // 指数移動平均
		uint32_t vertexCount = 0;
		int subSteps = 0;
	};

	/// @brief 水面コンポーネント：波紋(波動方程式)　+　うねり　をメッシュ頂点の動きで表現する
	struct FALU_SCENE_API RippleComponent
	{
		//===== 保存されるパラメータ ======
		bool simulate = true;
		uint32_t resolution = 128; // 一片の頂点(2...512)
		float sizeX = 40.0f;
		float sizeZ = 40.0f;
		bool centered = true; // エンティティの中心を水面の中心にするか
		float uvTiling = 8.0f;

		float waveSpeed = 4.0f;
		float damping = 0.6f;
		float maxHeight = 1.5f;
		int absorbWidth = 8;

		/// @brief 水面のうねり表現
		GerstnerWave waves[4] = {
			{0.25f,12.0f,0.0f,0.5f},
			{0.12f,6.0f,35.0f,0.5f},
			{0.0f,4.0f,-50.0f,0.5f},
			{0.0f,2.5f,80.0f,0.5f},
		};
		float waveTimeScale = 1.0f;


		// 自動雨滴
		float dropPerSecond = 0.0f;
		float dropStrength = 0.4f;
		float dropRadius = 1.0f;
		uint32_t seed = 1;

		//==== ランタイム状態 ======
		struct Impulse
		{
			float x, z, strength, radius; // ローカルXZ(メッシュ左下原点)
		};
		std::vector<Impulse> pending;
		RippleGrid grid;
		std::vector<glm::vec3> scratch;
		float time = 0.0f;
		float dropAccum = 0.0f;
		uint32_t rng = 0;
		bool built = false;
		uint32_t builtResolution = 0;
		float builtSizeX = 0.0f, builtSizeZ = 0.0f;
		SimStats stats;
	};
}
