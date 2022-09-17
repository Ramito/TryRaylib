#include "Menu.h"

#include "raygui.h"

void Menu::UpdateMenu(std::function<void(uint32_t)>&& startgameAction) {
	if (mP1Button) {
		startgameAction(1);
		mP1Button = false;
	}
	if (mP2Button) {
		startgameAction(2);
		mP2Button = false;
	}
	if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))
	{
		mMenuActive = !mMenuActive;
	}
}
void Menu::DrawMenu() {
	GuiSetStyle(DEFAULT, TEXT_SIZE, 50);
	if (mMenuActive) {
		GuiEnable();
		mAlpha = std::min(1.f, mAlpha + 0.05f);
		GuiFade(mMenuActive);
	}
	else {
		mMenuActive = std::max(0.f, mMenuActive - 0.025f);
		GuiFade(mMenuActive);
		if (mMenuActive == 0.f) {
			GuiDisable();
		}
	}
	float width = GetScreenWidth();
	float height = GetScreenHeight();
	if (GuiButton({ width * 0.25f, height * 0.5f, width * 0.5f, height * 0.2f }, "1 Player")) {
		mP1Button = mMenuActive && true;
	}
	if (GuiButton({ width * 0.25f, height * 0.75f, width * 0.5f, height * 0.2f }, "2 Players")) {
		mP2Button = mMenuActive && true;
	}
	mMenuActive = mMenuActive && !mP1Button && !mP2Button;
}
