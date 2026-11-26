#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <algorithm>
#include <sstream>
#include <vector>

using namespace geode::prelude;

// ---------- Shared macro state ----------
enum class Mode { Off, Record, Play };

struct Action {
    int frame;    // physics step when the click happened
    int button;   // which button (1 = jump)
    bool down;    // true = pressed, false = released
    bool player1; // true = player 1, false = player 2
};

static Mode g_mode = Mode::Off;
static std::vector<Action> g_actions;
static size_t g_next = 0; // next action to replay

static auto macroPath() {
    return Mod::get()->getSaveDir() / "macro.txt";
}

static void saveMacro() {
    std::ostringstream out;
    for (auto const& a : g_actions) {
        out << a.frame << ' ' << a.button << ' ' << a.down << ' ' << a.player1 << '\n';
    }
    (void) utils::file::writeString(macroPath(), out.str());
}

static void loadMacro() {
    g_actions.clear();
    auto res = utils::file::readString(macroPath());
    if (!res) return;
    std::istringstream in(res.unwrap());
    int f, b, d, p;
    while (in >> f >> b >> d >> p) {
        g_actions.push_back({f, b, d != 0, p != 0});
    }
}

// ---------- Recording + replaying clicks ----------
class $modify(MacroGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        // While replaying, ignore your real taps
        if (g_mode == Mode::Play) return;

        if (g_mode == Mode::Record) {
            g_actions.push_back({static_cast<int>(m_gameState.m_currentProgress), button, down, isPlayer1});
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (g_mode == Mode::Play) {
            int frame = static_cast<int>(m_gameState.m_currentProgress);
            while (g_next < g_actions.size() && g_actions[g_next].frame <= frame) {
                auto const& a = g_actions[g_next++];
                // Calls the original game function directly
                GJBaseGameLayer::handleButton(a.down, a.button, a.player1);
            }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

// ---------- Handle dying / restarting ----------
class $modify(MacroPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        int f = static_cast<int>(m_gameState.m_currentProgress);

        if (g_mode == Mode::Record) {
            // Throw away anything recorded after the respawn point
            std::erase_if(g_actions, [f](Action const& a) { return a.frame >= f; });
        }
        if (g_mode == Mode::Play) {
            // Start replay again from the respawn point
            g_next = std::lower_bound(
                g_actions.begin(), g_actions.end(), f,
                [](Action const& a, int frame) { return a.frame < frame; }
            ) - g_actions.begin();
        }
    }
};

// ---------- Buttons in the pause menu ----------
class $modify(MacroPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({winSize.width - 60.f, 90.f});

        auto makeBtn = [&](char const* text, float y, std::function<void()> fn) {
            auto spr = ButtonSprite::create(text, 80, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.5f);
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [fn](CCMenuItemSpriteExtra*) { fn(); });
            btn->setPosition({0.f, y});
            menu->addChild(btn);
        };

        makeBtn("Record", 70.f, [] {
            g_mode = Mode::Record;
            g_actions.clear();
            Notification::create("Recording! Restart the level.", NotificationIcon::Success)->show();
        });

        makeBtn("Play", 35.f, [] {
            if (g_actions.empty()) loadMacro();
            g_mode = Mode::Play;
            g_next = 0;
            Notification::create("Playback! Restart the level.", NotificationIcon::Success)->show();
        });

        makeBtn("Stop", 0.f, [] {
            if (g_mode == Mode::Record) saveMacro();
            g_mode = Mode::Off;
            Notification::create("Stopped (macro saved)", NotificationIcon::Info)->show();
        });

        this->addChild(menu);
    }
};
