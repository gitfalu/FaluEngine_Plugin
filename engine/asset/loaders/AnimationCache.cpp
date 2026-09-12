#include "AnimationCache.h"

namespace FaluEngine
{
	AnimationCache& AnimationCache::get()
	{
		static AnimationCache instance;
		return instance;
	}
}
