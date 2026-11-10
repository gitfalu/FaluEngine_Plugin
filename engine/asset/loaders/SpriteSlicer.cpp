#include "SpriteSlicer.h"

#include <algorithm>
#include <cmath>

namespace FaluEngine::SpriteSlicer
{
	glm::vec4 clampRect(const glm::vec4& rect, const glm::ivec2& texSize)
	{
		const float tw = static_cast<float>((std::max)(texSize.x, 1));
		const float th = static_cast<float>((std::max)(texSize.y, 1));

		float x = std::round(rect.x), y = std::round(rect.y);
		float w = (std::max)(std::round(rect.z), 1.0f);
		float h = (std::max)(std::round(rect.w), 1.0f);

		w = (std::min)(w, tw);
		h = (std::min)(h, th);
		x = std::clamp(x, 0.0f, tw - w);
		y = std::clamp(y, 0.0f, th - h);
		return { x,y,w,h };
	}

	std::vector<SpriteSlice> sliceByCellSize(const glm::ivec2& texSize, const CellGrid& grid, 
		const std::string& prefix)
	{
		std::vector<SpriteSlice> out;
		if (texSize.x <= 0 || texSize.y <= 0 || grid.cell.x <= 0 || grid.cell.y <= 0) return out;
		
		const int padX = (std::max)(grid.padding.x, 0);
		const int padY = (std::max)(grid.padding.y, 0);
		int index = 0;
		for (int y = (std::max)(grid.offset.y, 0); y + grid.cell.y <= texSize.y; y += grid.cell.y + padY)
		{
			for (int x = (std::max)(grid.offset.x, 0); x + grid.cell.x <= texSize.x; x += grid.cell.x + padX)
			{
				SpriteSlice s;
				s.name = prefix + "_" + std::to_string(index++);
				s.rect = { static_cast<float>(x),static_cast<float>(y),
						static_cast<float>(grid.cell.x),static_cast<float>(grid.cell.y) };
				out.push_back(std::move(s));
			}
		}

		return out;
	}

	std::vector<SpriteSlice> sliceByCount(const glm::ivec2& texSize, const glm::ivec2& count, 
		const glm::ivec2& padding, const std::string& prefix)
	{
		if (count.x <= 0 || count.y <= 0 || texSize.x <= 0 || texSize.y <= 0) return {};

		const int padX = (std::max)(padding.x, 0);
		const int padY = (std::max)(padding.y, 0);
		const int cw = (texSize.x - padX * (count.x - 1)) / count.x;
		const int ch = (texSize.y - padY * (count.y - 1)) / count.y;
		if (cw <= 0 || ch <= 0) return {};

		CellGrid g;
		g.cell = { cw,ch };
		g.padding = { padX,padY };
		auto slices = sliceByCellSize(texSize, g, prefix);
		// 端数で余分なセルが増えないよう指定数に切り詰める
		const size_t want = static_cast<size_t>(count.x) * static_cast<size_t>(count.y);
		if (slices.size() > want) slices.resize(want);

		return slices;
	}

	std::string makeUniqueName(const std::vector<SpriteSlice>& slices, const std::string& base, 
		int ignoreIndex)
	{
		const std::string root = base.empty() ? "sprite" : base;
		auto exists = [&](const std::string& n) {
			for (size_t i = 0; i < slices.size(); ++i)
				if (static_cast<int>(i) != ignoreIndex && slices[i].name == n) return true;
			return false;
			};
		if (!exists(root)) return root;
		for (int i = 1; i < 100000; ++i)
		{
			std::string candiate = root + "_" + std::to_string(i);
			if (!exists(candiate)) return candiate;
		}

		return root;
	}

	Handle hitHandle(const glm::vec4& rect, const glm::vec2& p, float handlePx)
	{
		const float hp = (std::max)(handlePx, 0.5f);
		const bool inX = p.x >= rect.x - hp && p.x <= rect.x + rect.z + hp;
		const bool inY = p.y >= rect.y - hp && p.y <= rect.y + rect.w + hp;
		if (!inX || !inY) return Handle::None;

		const bool left		= std::fabs(p.x - rect.x)				<= hp;
		const bool right	= std::fabs(p.x - (rect.x + rect.z))	<= hp;
		const bool top		= std::fabs(p.y - rect.y)				<= hp;
		const bool bottom	= std::fabs(p.y - (rect.y + rect.w))	<= hp;

		if (left && top) return Handle::TopLeft;
		if (right && top) return Handle::TopRight;
		if (left && bottom) return Handle::BottomLeft;
		if (right && bottom) return Handle::BottomRight;
		if (left) return Handle::Left;
		if (right) return Handle::Right;
		if (top) return Handle::Top;
		if (bottom) return Handle::Bottom;

		const bool inside = p.x >= rect.x && p.x <= rect.x + rect.z && p.y >= rect.y && p.y <= rect.y + rect.w;
		return inside ? Handle::Body : Handle::None;
	}

	glm::vec4 dragRect(const glm::vec4& orig, Handle h, const glm::vec2& delta, 
		const glm::ivec2& texSize)
	{
		float l = orig.x, t = orig.y, r = orig.x + orig.z, b = orig.y + orig.w;

		switch (h)
		{
		case FaluEngine::SpriteSlicer::Handle::Body:
			l += delta.x; r += delta.x; t += delta.y; b += delta.y; break;
		case Handle::Left:			l += delta.x; break;
		case Handle::Right:			r += delta.x; break;
		case Handle::Top:			t += delta.y; break;
		case Handle::Bottom:		b += delta.y; break;
		case Handle::TopLeft:		l += delta.x; t += delta.y; break;
		case Handle::TopRight:		r += delta.x; t += delta.y; break;
		case Handle::BottomLeft:	l += delta.x; b += delta.y; break;
		case Handle::BottomRight:	r += delta.x; b += delta.y; break;
		case Handle::None: return orig;
		}

		if (h == Handle::Body)
			return clampRect({ 1,t,orig.z,orig.w }, texSize);// 大きさを保ったまま移動

		// リサイズ
		if (r - l < 1.0f) { if (h == Handle::Left || h == Handle::TopLeft || h == Handle::BottomLeft) l = r - 1.0f; else r = l + 1.0f; }
		if (b - t < 1.0f) { if (h == Handle::Top || h == Handle::TopLeft || h == Handle::TopRight) t = b - 1.0f; else b = t + 1.0f; }

		l = std::clamp(std::round(l), 0.0f, static_cast<float>(texSize.x) - 1.0f);
		t = std::clamp(std::round(t), 0.0f, static_cast<float>(texSize.y) - 1.0f);
		r = std::clamp(std::round(r), l + 1.0f, static_cast<float>(texSize.x));
		b = std::clamp(std::round(b), t + 1.0f, static_cast<float>(texSize.y));
		return { l,t,r - 1 ,b - 1 };
	}

	SpriteClipAsset makeClipFromSlices(const std::string& sheetPath, 
		const std::vector<int>& sliceIndices, float fps, bool loop)
	{
		SpriteClipAsset clip;
		clip.sheetPath = sheetPath;
		clip.loop = loop;
		const float dur = 1.0 / (std::max)(fps, 0.001f);
		for (int i : sliceIndices) clip.frames.push_back({ i,dur });
		return clip;
	}
}