#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

static void applyFps() {
	auto fps = Mod::get()->getSettingValue<int64_t>("fps");
	if (fps < 30) fps = 30;
	CCApplication::sharedApplication()->setAnimationInterval(1.0 / static_cast<double>(fps));
}

// Apply when the setting is changed
$on_mod(Loaded) {
	listenForSettingChanges<int64_t>("fps", [](int64_t) {
		applyFps();
	});
}

// The game can reset the FPS, so apply again in the main menu...
class $modify(FpsMenuLayer, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;
		applyFps();
		return true;
	}
};

// ...and when a level starts
class $modify(FpsPlayLayer, PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
		applyFps();
		return true;
	}
};
