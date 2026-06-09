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
    auto res = file::writeString(filePath, jsonStr);
    
    if (res.isOk()) {
        log::info("Macro saved to {}", filePath.string());
    }
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

// --- Плаваю��ее UI (Стиль чит-меню CS2 / ImGui) ---
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

        // Главный контейнер окна
        m_windowNode = CCNode::create();
        m_windowNode->setPosition(winSize.width / 2, winSize.height / 2);
        this->addChild(m_windowNode);

        // Внешняя рамка (тонкая, чуть светлее фона)
        auto border = CCLayerColor::create({40, 40, 45, 255});
        border->setContentSize({342.f, 212.f});
        border->setPosition(-171.f, -106.f);
        m_windowNode->addChild(border);

        // Темный плоский фон (в стиле ImGui)
        auto bgFill = CCLayerColor::create({15, 15, 18, 255});
        bgFill->setContentSize({340.f, 210.f});
        bgFill->setPosition(-170.f, -105.f);
        m_windowNode->addChild(bgFill);

        // Шапка окна
        auto topBar = CCLayerColor::create({25, 25, 30, 255});
        topBar->setContentSize({340.f, 25.f});
        topBar->setPosition(-170.f, 105.f - 25.f);
        m_windowNode->addChild(topBar);

        // Акцентная линия (фиолетовая/синяя как в читах)
        auto accentLine = CCLayerColor::create({120, 90, 255, 255});
        accentLine->setContentSize({340.f, 2.f});
        accentLine->setPosition(-170.f, 105.f - 27.f);
        m_windowNode->addChild(accentLine);

        // Заголовок
        auto title = CCLabelBMFont::create("MACROBOT v1.0", "chatFont.fnt");
        title->setPosition(-100.f, 92.f);
        title->setScale(0.6f);
        title->setColor({200, 200, 220});
        m_windowNode->addChild(title);

        // Статус
        m_statusLabel = CCLabelBMFont::create("IDLE", "chatFont.fnt");
        m_statusLabel->setPosition(0, 45.f);
        m_statusLabel->setScale(0.8f);
        m_windowNode->addChild(m_statusLabel);
        updateStatusLabel();

        // Меню кнопок
        auto buttonMenu = CCMenu::create();
        buttonMenu->setPosition(0, 0);
        m_windowNode->addChild(buttonMenu);

        // Кнопка закрытия [X]
        auto closeBtnBg = CCLayerColor::create({200, 50, 50, 0}); // прозрачный фон
        closeBtnBg->setContentSize({25.f, 25.f});
        auto closeLabel = CCLabelBMFont::create("X", "chatFont.fnt");
        closeLabel->setPosition(12.5f, 12.5f);
        closeLabel->setScale(0.6f);
        closeBtnBg->addChild(closeLabel);
        
        auto closeBtn = CCMenuItemSpriteExtra::create(closeBtnBg, this, menu_selector(MacroMenuLayer::onClose));
        closeBtn->setPosition(155.f, 92.f);
        buttonMenu->addChild(closeBtn);

        // Лямбда для создания плоских кнопок
        auto createBtn = [this](const char* text, ccColor4B color, cocos2d::SEL_MenuHandler selector) {
            auto bg = CCLayerColor::create(color);
            bg->setContentSize({100.f, 25.f});
            auto label = CCLabelBMFont::create(text, "chatFont.fnt");
            label->setPosition(50.f, 12.5f);
            label->setScale(0.65f);
            bg->addChild(label);
            return CCMenuItemSpriteExtra::create(bg, this, selector);
        };

        // Кнопки управления (Плоские цвета)
        auto recordBtn = createBtn("RECORD", {200, 60, 60, 255}, menu_selector(MacroMenuLayer::onRecord));
        recordBtn->setPosition(-60.f, 10.f);
        buttonMenu->addChild(recordBtn);

        auto playBtn = createBtn("PLAY", {60, 200, 60, 255}, menu_selector(MacroMenuLayer::onPlay));
        playBtn->setPosition(60.f, 10.f);
        buttonMenu->addChild(playBtn);

        // Текст выбранного макроса
        auto macroBg = CCLayerColor::create({25, 25, 30, 255});
        macroBg->setContentSize({160.f, 25.f});
        macroBg->setPosition(-80.f, -42.5f);
        m_windowNode->addChild(macroBg);

        m_macroNameLabel = CCLabelBMFont::create("No macros", "chatFont.fnt");
        m_macroNameLabel->setPosition(0, -30.f);
        m_macroNameLabel->setScale(0.6f);
        m_windowNode->addChild(m_macroNameLabel);

        // Стрелки переключения макросов
        auto prevBtn = createBtn("<", {40, 40, 45, 255}, menu_selector(MacroMenuLayer::onPrevMacro));
        prevBtn->getNormalImage()->setContentSize({25.f, 25.f});
        static_cast<CCLabelBMFont*>(prevBtn->getNormalImage()->getChildren()->objectAtIndex(0))->setPosition(12.5f, 12.5f);
        prevBtn->setPosition(-100.f, -30.f);
        buttonMenu->addChild(prevBtn);

        auto nextBtn = createBtn(">", {40, 40, 45, 255}, menu_selector(MacroMenuLayer::onNextMacro));
        nextBtn->getNormalImage()->setContentSize({25.f, 25.f});
        static_cast<CCLabelBMFont*>(nextBtn->getNormalImage()->getChildren()->objectAtIndex(0))->setPosition(12.5f, 12.5f);
        nextBtn->setPosition(100.f, -30.f);
        buttonMenu->addChild(nextBtn);

        // Сохранение и загрузка
        auto saveBtn = createBtn("SAVE", {60, 120, 200, 255}, menu_selector(MacroMenuLayer::onSave));
        saveBtn->setPosition(-60.f, -70.f);
        buttonMenu->addChild(saveBtn);

        auto loadBtn = createBtn("LOAD", {200, 150, 60, 255}, menu_selector(MacroMenuLayer::onLoad));
        loadBtn->setPosition(60.f, -70.f);
        buttonMenu->addChild(loadBtn);

        refreshMacroList();

        // Обработка касаний для перетаскивания окна
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
        auto rect = CCRect{ -170.f, -105.f, 340.f, 210.f };
        
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
            m_statusLabel->setColor({180, 180, 180});
        }
    }

    void onRecord(CCObject*) {
        if (g_isPlaying) return;
        g_isRecording = !g_isRecording;
        if (g_isRecording) {
            g_macroActions.clear();
            g_checkpoints.clear();
            g_currentFrame = 0;
            geode::Notification::create("Recording started", geode::NotificationIcon::Info)->show();
        } else {
            geode::Notification::create("Recording stopped", geode::NotificationIcon::Info)->show();
        }
        updateStatusLabel();
    }

    void onPlay(CCObject*) {
        if (g_isRecording) return;
        g_isPlaying = !g_isPlaying;
        if (g_isPlaying) {
            g_playbackIndex = 0;
            geode::Notification::create("Playback started", geode::NotificationIcon::Info)->show();
        } else {
            geode::Notification::create("Playback stopped", geode::NotificationIcon::Info)->show();
        }
        updateStatusLabel();
    }

    void onSave(CCObject*) {
        std::string name = m_macroFiles.empty() ? "new_macro" : m_macroFiles[m_currentMacroIndex];
        saveMacro(name);
        refreshMacroList();
        geode::Notification::create(fmt::format("Saved {}", name), geode::NotificationIcon::Success)->show();
    }

    void onLoad(CCObject*) {
        if (m_macroFiles.empty()) return;
        std::string name = m_macroFiles[m_currentMacroIndex];
        loadMacro(name);
        geode::Notification::create(fmt::format("Loaded {}", name), geode::NotificationIcon::Success)->show();
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
        
        // Логика Practice Mode: откатываем кадры и макрос при смерти
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

    void removeLastCheckpoint() {
        PlayLayer::removeLastCheckpoint();
        if (!g_checkpoints.empty()) {
            g_checkpoints.pop_back();
        }
    }
};

class $modify(MyBaseGameLayer, GJBaseGameLayer) {
    // В GD 2.2 физика и логика кадров происходит в GJBaseGameLayer::update
    void update(float dt) {
        GJBaseGameLayer::update(dt);
        
        if (g_isRecording || g_isPlaying) {
            g_currentFrame++;
        }

        // Воспроизведение макроса (именно здесь, в главном цикле)
        if (g_isPlaying) {
            while (g_playbackIndex < g_macroActions.size() && 
                   g_macroActions[g_playbackIndex].frame <= g_currentFrame) {
                auto action = g_macroActions[g_playbackIndex];
                this->handleButton(action.push, action.button, action.isPlayer1);
                g_playbackIndex++;
            }
            if (g_playbackIndex >= g_macroActions.size()) {
                g_isPlaying = false;
                geode::Notification::create("Playback finished", geode::NotificationIcon::Success)->show();
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