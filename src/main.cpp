#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// Глобальные переменные для хранения состояния (Шаг 1)
int g_currentFrame = 0;
bool g_isRecording = false;
bool g_isPlaying = false;

// Хукаем PlayLayer для отслеживания игровых кадров
class $modify(MyPlayLayer, PlayLayer) {
    void update(float dt) {
        // Вызываем оригинальный update
        PlayLayer::update(dt);
        
        // В реальном макросе здесь мы будем привязываться к внутреннему
        // счетчику игры (например, m_gameState), но пока делаем базовый инкремент
        if (g_isRecording || g_isPlaying) {
            g_currentFrame++;
        }
    }
};

// Хукаем GJBaseGameLayer для перехвата нажатий (клики, пробел, стрелки)
class $modify(MyBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool push, int button, bool isPlayer1) {
        // Обязательно вызываем оригинальную функцию, чтобы игра реагировала на клик
        GJBaseGameLayer::handleButton(push, button, isPlayer1);

        // Перехватываем действие для записи
        if (g_isRecording) {
            log::info("Record Action - Frame: {}, Button: {}, Push: {}, Player1: {}", 
                g_currentFrame, button, push, isPlayer1);
        }
    }
};
