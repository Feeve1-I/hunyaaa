#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <filesystem>

using namespace geode::prelude;

// --- Структура макроса и чекпоинтов ---
struct MacroAction {
    int frame;
    int button;
    bool push;
    bool isPlayer1;
};

struct CheckpointData {
    int frame;
    size_t playbackIndex;
    size_t macroSize;
};

// Глобальное состояние
int g_currentFrame = 0;
bool g_isRecording = false;
bool g_isPlaying = false;
std::vector<MacroAction> g_macroActions;
size_t g_playbackIndex = 0;
std::vector<CheckpointData> g_checkpoints;

// Путь к папке с макросами (в корне игры)
std::filesystem::path getMacrosPath() {
    return geode::dirs::getGameDir() / "macros";
}

void recordClick(int frame, int button, bool push, bool isPlayer1) {
    g_macroActions.push_back({frame, button, push, isPlayer1});
}

void saveMacro(const std::string& filename) {
    auto path = getMacrosPath();
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
    (void)file::writeString(filePath, jsonStr);
}

void loadMacro(const std::string& filename) {
    auto filePath = getMacrosPath() / (filename + ".json");
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

// --- Плавающее UI (Silicate / Modern Cheat Style) ---
class MacroMenuLayer : public CCLayer {
protected:
    CCLabelBMFont* m_statusLabel;
    CCLabelBMFont* m_macroNameLabel;
    CCNode* m_windowNode;

    std::vector<std::string> m_macroFiles;
    int m_currentMacroIndex = 0;
    
    bool m_isDragging = false;
    CCPoint m_dragOffset;

    bool init() override {
        if (!CCLayer::init()) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        m_windowNode = CCNode::create();
        m_windowNode->setPosition(winSize.width / 2, winSize.height / 2);
        m_windowNode->setContentSize({340.f, 210.f});
        m_windowNode->setAnchorPoint({0.5f, 0.5f});
        this->addChild(m_windowNode);

        // Основной фон - используем square02b_001.png для закругленных краев и flat цвета. 
        // Это убирает лаги и дает идеальные хитбоксы.
        auto bg = CCScale9Sprite::create("square02b_001.png");
        bg->setContentSize({340.f, 210.f});
        bg->setPosition(170.f, 105.f); // В центре windowNode
        bg->setColor({18, 18, 22});
        bg->setOpacity(245);
        m_windowNode->addChild(bg);

        // Шапка
        auto topBar = CCScale9Sprite::create("square02b_001.png");
        topBar->setContentSize({340.f, 30.f});
        topBar->setPosition(170.f, 195.f);
        topBar->setColor({30, 30, 38});
        m_windowNode->addChild(topBar);

        // Текст заголовка
        auto title = CCLabelBMFont::create("SILICATE BOT", "chatFont.fnt");
        title->setPosition(70.f, 195.f);
        title->setScale(0.65f);
        title->setColor({220, 220, 220});
        m_windowNode->addChild(title);

        // Статус
        m_statusLabel = CCLabelBMFont::create("IDLE", "chatFont.fnt");
        m_statusLabel->setPosition(170.f, 150.f);
        m_statusLabel->setScale(0.8f);
        m_windowNode->addChild(m_statusLabel);
        updateStatusLabel();

        // Меню для кнопок
        auto buttonMenu = CCMenu::create();
        buttonMenu->setPosition(170.f, 105.f); // Центр окна
        m_windowNode->addChild(buttonMenu);

        // Кнопка закрытия
        auto closeSprite = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        closeSprite->setScale(0.5f);
        auto closeBtn = CCMenuItemSpriteExtra::create(closeSprite, this, menu_selector(MacroMenuLayer::onClose));
        closeBtn->setPosition(150.f, 90.f);
        buttonMenu->addChild(closeBtn);

        // Функция создания красивой плоской кнопки с ПРАВИЛЬНЫМИ хитбоксами
        auto createBtn = [this](const char* text, ccColor3B color, cocos2d::SEL_MenuHandler selector) {
            auto btnBg = CCScale9Sprite::create("square02b_001.png");
            btnBg->setContentSize({100.f, 30.f});
            btnBg->setColor(color);
            
            auto label = CCLabelBMFont::create(text, "chatFont.fnt");
            label->setPosition(50.f, 15.f);
            label->setScale(0.65f);
            btnBg->addChild(label);
            
            return CCMenuItemSpriteExtra::create(btnBg, this, selector);
        };

        auto createSmallBtn = [this](const char* text, ccColor3B color, cocos2d::SEL_MenuHandler selector) {
            auto btnBg = CCScale9Sprite::create("square02b_001.png");
            btnBg->setContentSize({30.f, 30.f});
            btnBg->setColor(color);
            
            auto label = CCLabelBMFont::create(text, "chatFont.fnt");
            label->setPosition(15.f, 15.f);
            label->setScale(0.8f);
            btnBg->addChild(label);
            
            return CCMenuItemSpriteExtra::create(btnBg, this, selector);
        };

        // Кнопки RECORD и PLAY
        auto recordBtn = createBtn("RECORD", {200, 50, 50}, menu_selector(MacroMenuLayer::onRecord));
        recordBtn->setPosition(-60.f, 5.f);
        buttonMenu->addChild(recordBtn);

        auto playBtn = createBtn("PLAY", {50, 200, 50}, menu_selector(MacroMenuLayer::onPlay));
        playBtn->setPosition(60.f, 5.f);
        buttonMenu->addChild(playBtn);

        // Файл селектор (фон под текст)
        auto macroBg = CCScale9Sprite::create("square02b_001.png");
        macroBg->setContentSize({160.f, 30.f});
        macroBg->setColor({30, 30, 38});
        macroBg->setPosition(170.f, 60.f);
        m_windowNode->addChild(macroBg);

        m_macroNameLabel = CCLabelBMFont::create("No macros", "chatFont.fnt");
        m_macroNameLabel->setPosition(170.f, 60.f);
        m_macroNameLabel->setScale(0.6f);
        m_windowNode->addChild(m_macroNameLabel);

        // Кнопки < и >
        auto prevBtn = createSmallBtn("<", {50, 50, 60}, menu_selector(MacroMenuLayer::onPrevMacro));
        prevBtn->setPosition(-110.f, -45.f);
        buttonMenu->addChild(prevBtn);

        auto nextBtn = createSmallBtn(">", {50, 50, 60}, menu_selector(MacroMenuLayer::onNextMacro));
        nextBtn->setPosition(110.f, -45.f);
        buttonMenu->addChild(nextBtn);

        // Кнопки SAVE и LOAD
        auto saveBtn = createBtn("SAVE", {50, 150, 200}, menu_selector(MacroMenuLayer::onSave));
        saveBtn->setPosition(-60.f, -85.f);
        buttonMenu->addChild(saveBtn);

        auto loadBtn = createBtn("LOAD", {200, 150, 50}, menu_selector(MacroMenuLayer::onLoad));
        loadBtn->setPosition(60.f, -85.f);
        buttonMenu->addChild(loadBtn);

        refreshMacroList();

        // Касания для Drag&Drop окна
        this->setTouchEnabled(true);
        this->setKeypadEnabled(true);
        this->setZOrder(105);

        return true;
    }
    
    void refreshMacroList() {
        m_macroFiles.clear();
        auto path = getMacrosPath();
        file::createDirectoryAll(path);
        
        if (std::filesystem::exists(path)) {
            for (auto const& entry : std::filesystem::directory_iterator(path)) {
                if (entry.path().extension() == ".json") {
                    m_macroFiles.push_back(entry.path().stem().string());
                }
            }
        }
        
        if (m_macroFiles.empty()) {
            m_macroNameLabel->setString("new_macro");
        } else {
            if (m_currentMacroIndex >= m_macroFiles.size()) m_currentMacroIndex = 0;
            if (m_currentMacroIndex < 0) m_currentMacroIndex = m_macroFiles.size() - 1;
            m_macroNameLabel->setString(m_macroFiles[m_currentMacroIndex].c_str());
        }
    }

    void onPrevMacro(CCObject*) {
        if (m_macroFiles.empty()) return;
        m_currentMacroIndex--;
        refreshMacroList();
    }

    void onNextMacro(CCObject*) {
        if (m_macroFiles.empty()) return;
        m_currentMacroIndex++;
        refreshMacroList();
    }

    void registerWithTouchDispatcher() override {
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -500, true);
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        auto pos = m_windowNode->convertToNodeSpace(touch->getLocation());
        auto rect = CCRect{ 0.f, 0.f, 340.f, 210.f }; // m_windowNode size
        
        if (rect.containsPoint(pos)) {
            m_isDragging = true;
            m_dragOffset = m_windowNode->getPosition() - touch->getLocation();
            return true; 
        }
        return false;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        if (m_isDragging) {
            m_windowNode->setPosition(touch->getLocation() + m_dragOffset);
        }
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        m_isDragging = false;
    }

    void onClose(CCObject*) {
        this->removeFromParentAndCleanup(true);
    }

    void updateStatusLabel() {
        if (g_isRecording) {
            m_statusLabel->setString("RECORDING");
            m_statusLabel->setColor({255, 80, 80});
        } else if (g_isPlaying) {
            m_statusLabel->setString("PLAYING");
            m_statusLabel->setColor({80, 255, 80});
        } else {
            m_statusLabel->setString("IDLE");
            m_statusLabel->setColor({120, 120, 120});
        }
    }

    void onRecord(CCObject*) {
        if (g_isPlaying) return;
        g_isRecording = !g_isRecording;
        if (g_isRecording) {
            g_macroActions.clear();
            g_checkpoints.clear();
            g_currentFrame = 0;
        }
        updateStatusLabel();
    }

    void onPlay(CCObject*) {
        if (g_isRecording) return;
        g_isPlaying = !g_isPlaying;
        if (g_isPlaying) {
            g_playbackIndex = 0;
        }
        updateStatusLabel();
    }

    void onSave(CCObject*) {
        std::string name = m_macroFiles.empty() ? "new_macro" : m_macroFiles[m_currentMacroIndex];
        saveMacro(name);
        refreshMacroList();
    }

    void onLoad(CCObject*) {
        if (m_macroFiles.empty()) return;
        std::string name = m_macroFiles[m_currentMacroIndex];
        loadMacro(name);
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
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool arr, double time) {
        if (down && key == enumKeyCodes::KEY_F6) {
            if (!CCDirector::sharedDirector()->getRunningScene()->getChildByID("MacroMenuLayer")) {
                auto menu = MacroMenuLayer::create();
                menu->setID("MacroMenuLayer");
                CCDirector::sharedDirector()->getRunningScene()->addChild(menu);
            }
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, arr, time);
    }
};

// --- Логика макроса, кадров и чекпоинтов ---
class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_currentFrame = 0;
        g_playbackIndex = 0;
        g_checkpoints.clear();
        if (g_isRecording) g_macroActions.clear();
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        
        // Снимаем все зажатия, чтобы кубик не застрял при респавне
        if (g_isPlaying) {
            this->handleButton(false, 1, true);
            this->handleButton(false, 1, false);
            this->handleButton(false, 2, true);
            this->handleButton(false, 2, false);
            this->handleButton(false, 3, true);
            this->handleButton(false, 3, false);
        }
        
        // Логика Practice Mode
        if (this->m_isPracticeMode && !g_checkpoints.empty()) {
            auto cp = g_checkpoints.back();
            g_currentFrame = cp.frame;
            if (g_isRecording) g_macroActions.resize(cp.macroSize);
            if (g_isPlaying) g_playbackIndex = cp.playbackIndex;
        } else {
            g_currentFrame = 0;
            g_playbackIndex = 0;
            g_checkpoints.clear();
            if (g_isRecording) g_macroActions.clear();
        }
    }

    void markCheckpoint() {
        PlayLayer::markCheckpoint();
        g_checkpoints.push_back({g_currentFrame, g_playbackIndex, g_macroActions.size()});
    }

    void removeCheckpoint(bool first) {
        PlayLayer::removeCheckpoint(first);
        if (!g_checkpoints.empty()) {
            g_checkpoints.pop_back();
        }
    }
};

class $modify(MyBaseGameLayer, GJBaseGameLayer) {
    void update(float dt) {
        GJBaseGameLayer::update(dt);
        
        if (g_isRecording || g_isPlaying) {
            g_currentFrame++;
        }

        if (g_isPlaying) {
            while (g_playbackIndex < g_macroActions.size() && 
                   g_macroActions[g_playbackIndex].frame <= g_currentFrame) {
                auto action = g_macroActions[g_playbackIndex];
                this->handleButton(action.push, action.button, action.isPlayer1);
                g_playbackIndex++;
            }
            if (g_playbackIndex >= g_macroActions.size()) {
                g_isPlaying = false;
            }
        }
    }

    void handleButton(bool push, int button, bool isPlayer1) {
        GJBaseGameLayer::handleButton(push, button, isPlayer1);
        if (g_isRecording && !g_isPlaying) {
            recordClick(g_currentFrame, button, push, isPlayer1);
        }
    }
};