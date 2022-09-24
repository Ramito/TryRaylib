#pragma once

#include <functional>

class Menu {
public:
	void UpdateMenu(std::function<void(uint32_t)>&& startgameAction);
	void DrawMenu();

private:
	struct GamepadState {
		bool Active;
		int Selection = 0;
	};
	GamepadState mPadState;

	bool mMenuActive = true;
	bool mP1Button = false;
	bool mP2Button = false;
	float mAlpha = 1.f;
};
