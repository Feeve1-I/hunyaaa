#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/ui/GeodeUI.hpp>

using namespace geode::prelude;

// --- Структура макроса ---
struct MacroAction {
    int frame;
    int button;
    bool push;
    bool isPlayer1;
};

// Глобальное состояние
int g_currentFrame = 0;
bool g_isRecording = false;
bool g_isPlaying = false;
std::vector<MacroAction> g_macroActions;
size_t g_playbackIndex = 0;

void recordClick(int frame, int button, bool push, bool isPlayer1) {
    g_macroActions.push_back({frame, button, push, isPlayer1});
}

void saveMacro(const std::string& filename) {
    auto path = Mod::get()->getConfigDir() / "macros";
    (void)file::createDirectoryAll(path); 
    
    std::string jsonStr = "[\n";
    for (size_t i = 0; i < g_macroActions.size(); ++i) {
        const auto& a = g_macroActions[i];
        jsonStr += fmt::format(
            "  {{\"frame\": {}, \"button\": {}, \"push\": {}, \"isPlayer1\": {}}}",
            a.frame, a.button, a.push ? "true" : "false", a.isPlayer1 ? "true" : "false"
        );
        if (i < g_macroActions.size() - 1) jsonStr += ",";
        jsonStr += "\n";
    }
    jsonStr += "]";

    auto filePath = path / (filename + ".json");
    auto res = file::writeString(filePath, jsonStr);
    
    if (res.isOk()) {
        log::info("Macro saved!");
    }
}

void loadMacro(const std::string& filename) {
    auto filePath = Mod::get()->getConfigDir() / "macros" / (filename + ".json");
    auto res = file::readJson(filePath);
    
    if (res.isErr()) return;

    auto json = res.unwrap();
    g_macroActions.clear();
    
    if (json.isArray()) {
        for (const auto& item : json.asArray().unwrap()) {
            MacroAction action;
            action.frame = item["frame"].asInt().unwrapOr(0);
            action.button = item["button"].asInt().unwrapOr(0);
            action.push = item["push"].asBool().unwrapOr(false);
            action.isPlayer1 = item["isPlayer1"].asBool().unwrapOr(false);
            g_macroActions.push_back(action);
        }
    }
    g_playbackIndex = 0;
}

// --- Минималистичный UI (Silicate-style) ---
class MacroMenuLayer : public CCLayer {
protected:
    CCLabelBMFont* m_statusLabel;
    CCScale9Sprite* m_bgSprite;

    bool init() override {
        if (!CCLayer::init()) return false;

        auto director = CCDirector::sharedDirector();
        auto winSize = director->getWinSize();

        // Задаем темный, полупрозрачный фон окна (в стиле современных читов)
        m_bgSprite = CCScale9Sprite::create("square02b_001.png");
        m_bgSprite->setContentSize({280.f, 180.f});
        m_bgSprite->setPosition(winSize.width / 2, winSize.height / 2);
        m_bgSprite->setColor({20, 20, 25});
        m_bgSprite->setOpacity(200);
        this->addChild(m_bgSprite);

        auto title = CCLabelBMFont::create("MacroBot", "bigFont.fnt");
        title->setPosition(winSize.width / 2, winSize.height / 2 + 180.f / 2 - 25);
        title->setScale(0.7f);
        this->addChild(title);

        m_statusLabel = CCLabelBMFont::create("Idle", "bigFont.fnt");
        m_statusLabel->setPosition(winSize.width / 2, winSize.height / 2 + 35);
        m_statusLabel->setScale(0.5f);
        this->addChild(m_statusLabel);
        updateStatusLabel();

        auto buttonMenu = CCMenu::create();
        buttonMenu->setPosition(winSize.width / 2, winSize.height / 2 - 10);
        this->addChild(buttonMenu);

        // Функция для создания аккуратных кнопок
        auto createBtn = [](const char* text, ccColor3B color) {
            auto bg = CCScale9Sprite::create("square02_001.png");
            bg->setContentSize({100, 30});
            bg->setColor(color);
            bg->setOpacity(180);
            auto label = CCLabelBMFont::create(text, "bigFont.fnt");
            label->setScale(0.45f);
            label->setPosition(50, 15);
            bg->addChild(label);
            return bg;
        };

        auto recordBtn = CCMenuItemSpriteExtra::create(
            createBtn("Record", {200, 50, 50}), static_cast<cocos2d::CCObject*>(this), menu_selector(MacroMenuLayer::onRecord)
        );
        recordBtn->setPosition(-60, 15);
        buttonMenu->addChild(recordBtn);

        auto playBtn = CCMenuItemSpriteExtra::create(
            createBtn("Play", {50, 200, 50}), static_cast<cocos2d::CCObject*>(this), menu_selector(MacroMenuLayer::onPlay)
        );
        playBtn->setPosition(60, 15);
        buttonMenu->addChild(playBtn);

        auto saveBtn = CCMenuItemSpriteExtra::create(
            createBtn("Save", {50, 150, 255}), static_cast<cocos2d::CCObject*>(this), menu_selector(MacroMenuLayer::onSave)
        );
        saveBtn->setPosition(-60, -25);
        buttonMenu->addChild(saveBtn);

        auto loadBtn = CCMenuItemSpriteExtra::create(
            createBtn("Load", {200, 150, 50}), static_cast<cocos2d::CCObject*>(this), menu_selector(MacroMenuLayer::onLoad)
        );
        loadBtn->setPosition(60, -25);
        buttonMenu->addChild(loadBtn);
        
        // Кнопка закрытия
        auto closeMenu = CCMenu::create();
        closeMenu->setPosition(winSize.width / 2 + 280.f / 2 - 15, winSize.height / 2 + 180.f / 2 - 15);
        auto closeSprite = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        closeSprite->setScale(0.7f);
        auto closeBtn = CCMenuItemSpriteExtra::create(
            closeSprite, static_cast<cocos2d::CCObject*>(this), menu_selector(MacroMenuLayer::onClose)
        );
        closeMenu->addChild(closeBtn);
        this->addChild(closeMenu);

        // Делаем слой модальным (перехватывает клики)
        this->setTouchEnabled(true);
        this->setKeypadEnabled(true);
        this->setZOrder(100);

        return true;
    }
    
    void onClose(CCObject*) {
        this->removeFromParentAndCleanup(true);
    }

    void updateStatusLabel() {
        if (g_isRecording) {
            m_statusLabel->setString("Status: RECORDING");
            m_statusLabel->setColor({255, 50, 50});
        } else if (g_isPlaying) {
            m_statusLabel->setString("Status: PLAYING");
            m_statusLabel->setColor({50, 255, 50});
        } else {
            m_statusLabel->setString("Status: Idle");
            m_statusLabel->setColor({200, 200, 200});
        }
    }

    void onRecord(CCObject*) {
        if (g_isPlaying) return;
        g_isRecording = !g_isRecording;
        if (g_isRecording) g_macroActions.clear();
        updateStatusLabel();
    }

    void onPlay(CCObject*) {
        if (g_isRecording) return;
        g_isPlaying = !g_isPlaying;
        updateStatusLabel();
    }

    void onSave(CCObject*) {
        saveMacro("macro");
        geode::Notification::create("Macro saved!", geode::NotificationIcon::Success)->show();
    }

    void onLoad(CCObject*) {
        loadMacro("macro");
        geode::Notification::create("Macro loaded!", geode::NotificationIcon::Success)->show();
    }

public:
    static MacroMenuLayer* create() {
        auto ret = new MacroMenuLayer();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// --- Хук на F6 для открытия меню ---
class $modify(MyKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool arr) {
        if (down && key == enumKeyCodes::KEY_F6) {
            // Убираем проверку на PlayLayer, чтобы можно было открыть везде
            if (!CCDirector::sharedDirector()->getRunningScene()->getChildByID("MacroMenuLayer")) {
                auto menu = MacroMenuLayer::create();
                menu->setID("MacroMenuLayer");
                CCDirector::sharedDirector()->getRunningScene()->addChild(menu);
            }
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, arr);
    }
};

// --- Хуки логики макроса ---
class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_currentFrame = 0;
        g_playbackIndex = 0;
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        g_currentFrame = 0;
        g_playbackIndex = 0;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        if (g_isRecording || g_isPlaying) g_currentFrame++;

        if (g_isPlaying) {
            while (g_playbackIndex < g_macroActions.size() && 
                   g_macroActions[g_playbackIndex].frame == g_currentFrame) {
                auto action = g_macroActions[g_playbackIndex];
                this->handleButton(action.push, action.button, action.isPlayer1);
                g_playbackIndex++;
            }
            if (g_playbackIndex >= g_macroActions.size()) {
                g_isPlaying = false;
            }
        }
    }
};

class $modify(MyBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool push, int button, bool isPlayer1) {
        GJBaseGameLayer::handleButton(push, button, isPlayer1);
        if (g_isRecording && !g_isPlaying) {
            recordClick(g_currentFrame, button, push, isPlayer1);
        }
    }
};