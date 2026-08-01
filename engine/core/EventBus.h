#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <any>
#include <algorithm>
#include <cstdint>


namespace FaluEngine {

    using HandlerId = uint32_t;

// ── 使い方 ────────────────────────────────────────────────────────────────
// struct WindowResizeEvent { uint32_t width, height; };
//
// EventBus bus;
// auto id = bus.subscribe<WindowResizeEvent>([](const WindowResizeEvent& e) {
//     LOG_INFO("Resized: {}x{}", e.width, e.height);
// });
// bus.publish(WindowResizeEvent{ 1920, 1080 });
// bus.unsubscribe<WindowResizeEvent>(id);
// ──────────────────────────────────────────────────────────────────────────

class EventBus {
public:
    /// @brief シングルトン
    /// @return 
    static EventBus& get()
    {
        static EventBus instance;
        return instance;
    }

    //======== 購読 ============
    
    /// @brief priorityが大きいほど先に呼ばれる(デフォルト: 0)
    /// @tparam Event 
    /// @param handler 
    /// @return 
    template<typename Event>
    HandlerId subscribe(std::function<void(const Event&)> handler,int priority = 0) {
        auto key = std::type_index(typeid(Event));
        HandlerId id = m_nextId++;

        // イベントを登録
        auto& vec = m_handlers[key];
        vec.push_back({ id,priority,false,[h = std::move(handler)](const std::any& e) {
            h(std::any_cast<const Event&>(e));
            } });
        
        // 降順ソート
        std::sort(vec.begin(), vec.end(),
            [](const Entry& a, const Entry& b) {return a.priority > b.priority; });

        return id;
    }

    /// @brief 一度だけ受け取る購読
    /// @tparam Event 
    /// @param handler 
    /// @param priority 
    /// @return 
    template<typename Event>
    HandlerId subscribeOnce(std::function<void(const Event&)> handler, int priority = 0) {
        auto key = std::type_index(typeid(Event));
        HandlerId id = m_nextId++;

        auto& vec = m_handlers[key];
        vec.push_back({ id , priority,true,[h = std::move(handler)](const std::any& e) {
                h(std::any_cast<const Event&>(e));
            } });

        std::sort(vec.begin(), vec.end(),
            [](const Entry& a, const Entry& b) {return a.priority > b.priority; });

        return id;
    }

    /// @brief 購読解除
    /// @tparam Event 
    /// @param id 
    template<typename Event>
    void unsubscribe(HandlerId id) {
        auto key = std::type_index(typeid(Event));
        auto it = m_handlers.find(key);
        if (it == m_handlers.end())return;

        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [id](const Entry& e) {return e.id == id; }), vec.end());
    }

    /// @brief イベント発行
    /// @tparam Event 
    /// @param event 
    template<typename Event>
    void publish(const Event& event) {
        auto key = std::type_index(typeid(Event));
        auto it = m_handlers.find(key);
        if (it == m_handlers.end())return;

        auto handleCopy = it->second;
        std::any wrapped = event;

        // once　フラグの物を後で削除するため別リストで管理
        std::vector<HandlerId> toRemove;

        for (auto& entry : handleCopy)
        {
            entry.fn(wrapped);
            if (entry.once)toRemove.push_back(entry.id);
        }

        // subscribeOnce の後処理
        for (auto removeId : toRemove) {
            auto& vec = it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [removeId](const Entry& e) {return e.id == removeId; }), vec.end());
        }
    }

    /// @brief 全ハンドラーをクリア
    void clear() { m_handlers.clear(); }

private:
    EventBus() = default;

    struct Entry {
        HandlerId id;
        int priority;
        bool once;
        std::function<void(const std::any&)> fn;
    };
    std::unordered_map<std::type_index, std::vector<Entry>> m_handlers;
    HandlerId m_nextId = 0;
};

} // namespace FaluEngine
