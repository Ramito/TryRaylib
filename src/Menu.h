#pragma once

#include <cstdint>
#include <functional>

class Menu {
public:
	void UpdateMenu(std::function<void(uint32_t)>&& startgameAction);
	void DrawMenu();

private:
	struct SelectionState {
		bool Active;
		int Selection = 0;
	};
	SelectionState mSelectionState;

	bool mMenuActive = true;
	bool mP1Button = false;
	bool mP2Button = false;
	float mAlpha = 1.f;
};
