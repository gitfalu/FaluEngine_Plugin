#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace FaluEngine
{
	/// @brief オブジェクトレイヤー(衝突判定のグループ分け)
	namespace Layers {
		static constexpr JPH::ObjectLayer NON_MOVING = 0;
		static constexpr JPH::ObjectLayer MOVING = 1;
		static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
	}


	namespace BroadPhaseLayers {
		static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
		static constexpr JPH::BroadPhaseLayer MOVING(1);
		static constexpr JPH::uint NUM_LAYERS = 2;
	}

	class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
		{
			switch (a)
			{
			case Layers::NON_MOVING:
				return b == Layers::MOVING;
			case Layers::MOVING:
				return true;
			default:
				return false;
			}
		}
	};


	class BPlayerInterfaceImpl : public JPH::BroadPhaseLayerInterface
	{
	public:
		BPlayerInterfaceImpl() {
			m_objectToBP[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			m_objectToBP[Layers::MOVING] = BroadPhaseLayers::MOVING;
		}

		JPH::uint GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NUM_LAYERS;
		}

		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
		{
			return m_objectToBP[layer];
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
		{
			switch ((JPH::BroadPhaseLayer::Type)layer) 
			{
			case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
				return "NON_MOVING";
			case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
				return "MOVING";
			default:
				return "INVALID";
			}
		}
#endif

	private:
		JPH::BroadPhaseLayer m_objectToBP[Layers::NUM_LAYERS];
	};

	class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bpLayer) const override
		{
			switch (layer)
			{
			case Layers::NON_MOVING:
				return bpLayer == BroadPhaseLayers::MOVING;
			case Layers::MOVING:
				return true;
			default:
				return false;
			}
		}
	};
}
