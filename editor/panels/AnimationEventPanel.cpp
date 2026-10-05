#include "AnimationEventPanel.h"

#include "FaluEngine/Scene.h"
#include "FaluEngine/Component.h"
#include "FaluEngine/Localization.h"
#include "FaluEngine/ActionEventTrack.h"
#include "FaluEngine/ActionEventLibrary.h"
#include "FaluEngine/ActionEventSystem.h"
#include "asset/loaders/AnimationCache.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>


namespace Editor
{
	using namespace FaluEngine;

	namespace
	{
		constexpr float kRowHeight = 22.0f; // 一行の高さ
		consteval float kRulerHeight = 22.0f; // 
		constexpr float kLabelWidth = 96.0f; // ラベルの幅
		consteval float kHandleWidth = 6.0f; // 

		/// @brief 登録するイベントによって表示する色を変える
		/// @param t 
		/// @param alpha 
		/// @return 
		ImU32 colorOf(ActionWindowType t, float alpha = 1.0f)
		{
			static const ImVec4 c[] = {
				{0.95f, 0.75f, 0.20f, 1}, // Parry
				{0.30f, 0.65f, 0.95f, 1}, // JustGuard
				{0.35f, 0.85f, 0.55f, 1}, // JustDodge
				{0.75f, 0.75f, 0.80f, 1}, // Invincible
				{0.70f, 0.50f, 0.90f, 1}, // SuperArmor
				{0.95f, 0.35f, 0.35f, 1}, // HitActive
				{0.95f, 0.55f, 0.25f, 1}, // CancelWindow
				{0.40f, 0.80f, 0.85f, 1}, // InputBuffer
				{0.60f, 0.60f, 0.60f, 1}, // Custom
			};

			ImVec4 v = c{ static_cast<int>(t) };
			v.w = alpha;
			return ImGui::ColorConvertFloat4ToU32(v);
		}

		/// @brief アクションにおいてデフォルトの効果時間を返す
		/// @param t 
		/// @param fallback 
		/// @return 
		float defaultLengthFor(ActionWindowType t, int fallback)
		{
			switch (t)
			{
			case ActionWindowType::Parry: return 8.0f; // パリィ
			case ActionWindowType::JustGuard: return 6.0f; // ジャストガード
			case ActionWindowType::JustDodge: return 10.0f; // ジャスト回避
			case ActionWindowType::Invincible: return 20.0f; // 無敵時間
			default: return static_cast<float>(fallback);
			}
		}
	}


	void AnimationEventPanel::draw(FaluEngine::Scene* scene, entt::entity selected)
	{
		ImGui::Begin(TR("Animation Events"));

		if(!scene || selected == entt::null || !scene->registry().valid(selected))
		{
			ImGui::Text(TR("No entity selected."));
			ImGui::End();
			return;
		}

		// Mesh/Animatorを持っているか確認
		auto& reg = scene->registry();
		auto* animator = reg.try_get<AnimatorComponent>(selected);
		auto* mesh = reg.try_get<MeshComponent>(selected);
		if(!animator || !mesh || mesh->meshPath.empty())
		{
			ImGui::TextDisabled(TR("Selected entity has no Animator or Mesh component."));
			ImGui::End();
			return;
		}

		// AnimationClipが登録済みか確認
		const auto& clips = AnimationCache::get().getAnimations(mesh->meshPath);
		if(clips.empty())
		{
			ImGui::TextDisabled(TR("No animation clips found for the mesh."));
			ImGui::End();
			return;
		}

		auto& lib = ActionEventLibrary::get();
		const std::string& meshPath = mesh->meshPath;


		//=========== Clip Selection ============
		{
			ImGui::SetNextItemWidth(260.0f);
			if (ImGui::BeginCombo(TR("Clip"), animator->currentClipName.empty() ? "(none)" : animator->currentClipName.c_str()))
			{
				for (const auto& c : clips)
				{
					animator->currentClipName = c->name;
					animator->playbackTime = 0.0f;
					m_selectedWindow = m_selectedPoint = -1;
				}
				ImGui::EndCombo();	
			}
		}

		auto clip = AnimationCache::get().getClip(meshPath, animator->currentClipName);
		if (!clip)
		{
			ImGui::TextDisabled(TR("select a clip"));
			ImGui::End();
			return;
		}

		const bool trackExists = lib.find(meshPath, clip->name) != nullptr;
		std::shared_ptr<ActionEventTrack> track = trackExists ? lib.edit(meshPath, clip->name) : nullptr;
		const float fps = track ? track->fps : 60.0f;
		const float totalFrames = std::max(1.0f, clip->duration * fps);
		const float curFrame = animator->playbackTime * fps;

		//============ Transport ===================
		auto scrubTo = [&](float frame)
			{
				frame = std::clamp(frame, 0.0f, clip->duration);
				animator->playing = false;
				animator->playbackTime = std::min(frame / fps, clip->duration);
				ActionEventSystem::resync(reg, selected, *animator, meshPath, clip->duration);
				scene->sampleAnimationPoses();// ポーズを更新する
			};

		if (ImGui::Button(animator->playing ? TR("Pause") : TR("Play")))
		{
			animator->playing = !animator->playing;
		}

		ImGui::SameLine();
		if (ImGui::Button("|<")) scrubTo(std::floor(curFrame) - 1.0f);
		ImGui::SameLine();
		if (ImGui::Button(">|")) scrubTo(std::floor(curFrame) + 1.0f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		ImGui::DragFloat(TR("Speed"), &animator->playbackSpeed, 0.01f, 0.05f, 4.0f, "%.2fx");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::SliderFloat(TR("Zoom"), &m_pixelPerFrame, 2.0f, 24.0f, "%.0f px/f");

		//======== タイムライン ===========
		const float rows = static_cast<float>(kActionWindowTypeCount) + 1.0f;
		const float timelineHeight = kRulerHeight + rows * kRowHeight + 24.0f;
		const float contentWidth = totalFrames * m_pixelPerFrame + 40.0f;

		ImGui::BeginChild("##timeline", ImVec2(0, timelineHeight + ImGui::GetStyle().ScrollberSize),
			true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove);
		{// 編集する箇所を作成、
			ImDrawList* dl = ImGui::GetWindowDrawList();
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			const float x0 = origin.x + kHandleWidth;
			const float yRuler = origin.y;
			const float yRows = origin.y + kRulerHeight;
			auto frameToX = [&](float f) {return x0 + f * m_pixelPerFrame; };
			auto xToFrame = [&](float x) { return (x - x0) / m_pixelPerFrame; };

			ImGui::Dummy(ImVec2(kLabelWidth + contentWidth, timelineHeight));

			// 行背景とラベル(ラベルはスクロールで流れていい簡易実装)
			for (uint32_t i = 0; i <= kActionWindowTypeCount; ++i)
			{
				const float y = yRows + i * kRowHeight;
				const bool isPointRow = (i == kActionWindowTypeCount);
				dl->AddRectFilled({ origin.x,y }, { x0 + totalFrames * m_pixelPerFrame,y + kRowHeight - 1 },
					(i % 2) ? IM_COL32(255, 255, 255, 10) : IM_COL32(255, 255, 255, 4));
				dl->AddText({ origin.x + 4,y + 3 },
					IM_COL32(200, 200, 200, 255),
					isPointRow ? "Points" : toString(static_cast<ActionWindowType>(i))
				);
			}

			// ルーラー（クリック/ドラッグでスクラブ）
			ImGui::SetCursorScreenPos({ x0,yRuler });
			ImGui::InvisibleButton("##ruler", ImVec2(totalFrames * m_pixelPerFrame + 1.0f, kRulerHeight);
			if (ImGui::IsItemActive())
				scrubTo(std::round(xToFrame(ImGui::GetIO().MousePos.x)));

			const int tickStep = m_pixelPerFrame >= 12.0f ? 1 : (m_pixelPerFrame >= 5.0f ? 5 : 10);
			for (int f = 0; f < static_cast<int>(totalFrames); f+=tickStep)
			{
				const float x = frameToX(static_cast<float>(f));
				const bool major = (f % (tickStep * 2) == 0);
				dl->AddLine({ x,yRuler + (major ? 4.0f : 12.0f) }, { x,yRows + rows * kRowHeight }, IM_COL32(255, 255, 255, major ? 40 : 18));
				if (major)
				{
					char buf[16];
					std::snprintf(buf, sizeof(buf), "%d", f);
					dl->AddText({ x + 2,yRuler }, IM_COL32(190, 190, 190, 255), buf);
				}
			}

			//====== 窓 ==========
			if (track)
			{
				for (int wi = 0; wi < static_cast<int>(track->windows.size()); ++wi)
				{
					ActionWindow& w = track->windows[wi];
					const float y = yRows + static_cast<float>(static_cast<int>(w.type)) * kRowHeight + 2.0f;
					const float xs = frameToX(w.startFrame);
					const float xe = frameToX(w.endFrame);
					const float barW = std::max(xe - xs, kHandleWidth * 2.0f + 4.0f);
					const bool selecredBar = (wi == m_selectedWindow);

					dl->AddRectFilled({ xs,y }, { xs + barW,y + kRowHeight - 5.0f }, colorOf(w.type, selecredBar ? 0.95f : 0.65f), 3.0f);
					if (selecredBar)
						dl->AddRect({ xs,y }, { xs + barW,y + kRowHeight - 5.0f }, IM_COL32(255, 255, 255, 230), 3.0f, 0, 1.5f);

					ImGui::PushID(wi);
					// ハンドル(左/本体/右)
					const ImVec2 sz(kHandleWidth, kRowHeight - 5.0f);
					const ImVec2 bodySz(std::max(barW - kHandleWidth * 2.0f, 1.0f), kRowHeight - 5.0f);

					struct Part { ImVec2 pos; ImVec2 size; Drag mode; const char* id; };
					const Part parts[3] = {
						{ {xs,y},sz,Drag::ResizeLeft,"L"},
						{ {xs + kHandleWidth , y },bodySz,Drag::Move,"B"},
						{ {xs + barW - kHandleWidth,y},sz,Drag::ResizeRight,"R"},
					};
					for (const Part& p : parts)
					{
						ImGui::SetCursorScreenPos(p.pos);
						ImGui::InvisibleButton(p.id, p.size);
						if (ImGui::IsItemHovered() && p.mode != Drag::Move)
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
						if (ImGui::IsItemActivated())
						{
							m_selectedWindow = wi;
							m_selectedPoint = -1;
							m_dragOrigStart = w.startFrame;
							m_dragOrigEnd = w.endFrame;
						}
						if (ImGui::IsItemActive())
						{
							const float df = std::round(ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f).x / m_pixelPerFrame);
							if (p.mode == Drag::Move)
							{
								const float len = m_dragOrigEnd - m_dragOrigStart;
								w.startFrame = std::clamp(m_dragOrigStart + df, 0.0f, std::max(0.0f, totalFrames - len));
								w.endFrame = w.startFrame + len;
							}
							else if (p.mode == Drag::ResizeLeft)
							{
								w.startFrame = std::clamp(m_dragOrigStart + df, 0.0f, w.endFrame - 1.0f);
							}
							else
							{
								w.endFrame = std::clamp(m_dragOrigEnd + df, w.startFrame + 1.0f, totalFrames);
							}
							lib.markEdited(meshPath);
						}
					}

					ImGui::PopID();

					if (barW > 54.0f)
					{
						char len[24];
						std::snprintf(len, sizeof(len), "%.0ff", w.endFrame - w.startFrame);
						dl->AddText({ xs + 8,y + 1 }, IM_COL32(20, 20, 20, 255), len);
					}
				}

				//========= ポイントイベント =======================--
				const float yp = yRows + static_cast<float>(kActionWindowTypeCount) * kRowHeight + 2.0f;

				for (int pi = 0; pi < static_cast<int>(track->points.size()); ++pi)
				{
					ActionPointEvent& p = track->points[pi];
					const float cx = frameToX(p.frame);
					const bool selectedPt = (pi == m_selectedPoint);
					const ImU32 col = selectedPt ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 210, 90, 255);
					dl->AddQuadFilled({ cx,yp }, { cx + 7,yp + 8 }, { cx,yp + 16 }, { cx - 7,yp + 8 }, col);

					ImGui::PushID(1000 + pi);
					ImGui::SetCursorScreenPos({ cx - 7,yp });
					ImGui::InvisibleButton("pt", ImVec2(14, 16));
					if (ImGui::IsItemHovered())
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
						ImGui::SetTooltip("%s @%.0f", p.name.c_str(), p.frame);
					}

					if (ImGui::IsItemActivated())
					{
						m_selectedPoint = pi;
						m_selectedWindow = -1;
						m_dragOrigFrame = p.frame;
					}

					if (ImGui::IsItemActive())
					{
						const float df = std::round(ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f).x / m_pixelPerFrame);
						p.frame = std::clamp(m_dragOrigFrame + df, 0.0f, totalFrames);
						lib.markEdited(meshPath);
					}
					ImGui::PopID();
				}
			}

			const float hx = frameToX(curFrame);
			dl->AddLine({ hx,yRuler }, { hx,yRows + rows * kRowHeight }, IM_COL32(255, 80, 80, 255), 2.0f);
		}
		ImGui::EndChild();


		//============ 追加ボタン =======================
		ImGui::TextUnformatted(TR("Add at playhead:"));
		for (uint32_t i = 0; i < kActionWindowTypeCount; ++i)
		{
			const auto type = static_cast<ActionWindowType>(i);
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, colorOf(type, 0.55f));
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::SmallButton(toString(type)))
			{
				auto t = lib.edit(meshPath, clip->name);
				t->fps = t->fps > 0 ? t->fps : 60.0f;
				ActionWindow w;
				w.type = type;
				w.startFrame = std::clamp(std::round(curFrame), 0.0f, totalFrames - 1.0f);
				w.endFrame = std::min(w.startFrame + defaultLengthFor(type, m_defaultLengthFrames), totalFrames);
				if (type == ActionWindowType::Custom) w.tag = "Custom";
				t->windows.push_back(w);
				m_selectedWindow = static_cast<int>(t->windows.size()) - 1;
				m_selectedPoint = -1;
				lib.markEdited(meshPath);
			}

			ImGui::PopID();
			ImGui::PopStyleColor();
		}

		ImGui::SetNextItemWidth(160.0f);
		ImGui::InputText("##newPoint", m_newPointName, sizeof(m_newPointName));
		ImGui::SameLine();
		if (ImGui::SmallButton(TR("Add point event")))
		{
			auto t = lib.edit(meshPath, clip->name);
			t->points.push_back({ std::clamp(std::round(curFrame),0.0f,totalFrames),m_newPointName,"" });
			m_selectedPoint = static_cast<int>(t->points.size()) - 1;
			m_selectedWindow = 1;
			lib.markEdited(meshPath);
		}

		//============- 選択中プロパティ =====================-
		if (track)
		{
			ImGui::Separator();
			if (m_selectedWindow >= 0 && m_selectedWindow < static_cast<int>(track->windows.size()))
			{
				ActionWindow& w = track->windows[m_selectedWindow];
				bool edited = false;

				int typeIdx = static_cast<int>(w.type);
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::BeginCombo(TR("Type"), toString(w.type)))
				{
					for (uint32_t i = 0; i < kActionWindowTypeCount; ++i)
					{
						if (ImGui::Selectable(toString(static_cast<ActionWindowType>(i)), typeIdx == static_cast<int>(i)))
						{
							w.type = static_cast<ActionWindowType>(i); edited = true;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::SetNextItemWidth(100.0f);
				edited |= ImGui::DragFloat(TR("Start (f)"), &w.startFrame, 0.5f, 0.0f, totalFrames, "%.1f");

				ImGui::SameLine();
				ImGui::TextDisabled("= %.1f f / %.1f ms", w.endFrame - w.startFrame, (w.endFrame - w.startFrame) / fps * 1000.0f);

				char tag[64];
				std::strncpy(tag, w.tag.c_str(), sizeof(tag) - 1);
				tag[sizeof(tag) - 1] = '\0';
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::InputText(TR("Tag"), tag, sizeof(tag))) { w.tag = tag; edited = true; }

				ImGui::SameLine();
				ImGui::SetNextItemWidth(100.0f);
				edited |= ImGui::DragFloat(TR("Param"), & w.param, 0.01f);

				if (ImGui::Button(TR("Delete window")))
				{
					track->windows.erase(track->windows.begin()* m_selectedWindow);
					m_selectedWindow = -1;
					edited = true;
				}
				if (edited) lib.markEdited(meshPath);
			}
			else if (m_selectedPoint >= 0 && m_selectedPoint < static_cast<int>(track->points.size()))
			{
				ActionPointEvent& p = track->points[m_selectedPoint];
				bool edited = false;
				char name[64], payload[128];
				std::strncpy(name, p.name.c_str(), sizeof(name) - 1); name[sizeof(name) - 1] = '\0';

				std::strncpy(payload, p.payload.c_str(), sizeof(payload) - 1); payload[sizeof(payload) - 1] = '\0';
				ImGui::SetNextItemWidth(180.0f);
				if (ImGui::InputText(TR("Name"), name, sizeof(name))) { p.name = name; edited = true; }
				ImGui::SetNextItemWidth(100.0f);
				if (ImGui::InputText(TR("Payload"), payload, sizeof(payload))) { p.payload = payload; edited = true; }
				edited |= ImGui::DragFloat(TR("Frame"), &p.frame, 0.5f, 0.0f, totalFrames, "%.1f");

				if (ImGui::Button(TR("Delete point")))
				{
					track->points.erase(track->point.begin() + m_selectedPoint);
					m_selectedPoint = -1;
					edited = true;
				}
				if (edited) lib.markEdited(meshPath);
			}
			else
			{
				ImGui::TextDisabled(TR("Click a window or point to edit it"));
			}

			for (const auto& issue : track->validate(clip->duration))
				ImGui::TextColored(Imvec4(1.0f, 0.55f, 0.3f, 1.0f), "! %s", issue.c_str());
		}

		//======== ライブ状態 ==========
		if (auto* s = reg.try_get<ActionStateComponent>(selected))
		{
			ImGui::Separator();
			ImGui::TextUnformatted(TR("Live state:"));
			for (uint32_t i = 0; i < kActionWindowTypeCount; ++i)
			{
				const auto type = static_cast<ActionWindowType>(i);
				if (!s->isActive(type)) continue;
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, colorOf(type, 0.8f));
				ImGui::SmallButton(toString(type));
				ImGui::PopStyleColor();
			}

			ImGui::Text("%s: %s", TR("If hit now"), toString(DefenseJudge::resolve(*s)));
		}

		//============= 保存 =====================
		ImGui::Separator();
		const bool dirty = lib.isDirty(meshPath);
		if (ImGui::Button(dirty ? TR("Save * ") : TR("Save")))
		{
			std::string err;
			if (lib.save(meshPath, &err)) m_lastSaveError.clear();
			else m_lastSaveError = err;
		}
		ImGui::SameLine();
		if (ImGui::Button(TR("Revert")))
		{
			lib.reload(meshPath);
			m_selectedWindow = m_selectedPoint = -1;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%s", ActionEventLibrary::sidecarAssetPath(meshPath).c_str());
		if (!m_lastSaveError.empty())
			ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Save failed: %s", m_lastSaveError.c_str());

		ImGui::End();
	}
}

