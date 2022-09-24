#include "Menu.h"

#include "raygui.h"

void Menu::UpdateMenu(std::function<void(uint32_t)>&& startgameAction) {
	mPadState.Active = mMenuActive && IsGamepadAvailable(0);
	if (mPadState.Active) {
		constexpr int SelectionCount = 2;
		if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
			mPadState.Selection += 1;
			if (mPadState.Selection >= SelectionCount) {
				mPadState.Selection -= SelectionCount;
			}
		}
		if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
			mPadState.Selection -= 1;
			if (mPadState.Selection < 0) {
				mPadState.Selection += SelectionCount;
			}
		}
	}

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
	if (mMenuActive) {
		GuiEnable();
		mAlpha = std::min(1.f, mAlpha + 0.05f);
		GuiFade(mMenuActive);
	}
	else {
		mAlpha = std::max(0.f, mAlpha - 0.025f);
		GuiFade(mAlpha);
		if (mAlpha == 0.f) {
			GuiDisable();
		}
	}
	float width = GetScreenWidth();
	float height = GetScreenHeight();
	float buttonWidth = width / 3;
	float buttonHeight = height / 5;

	Rectangle text = { 0, height / 8, width, height / 4 };
	GuiSetStyle(DEFAULT, TEXT_SPACING, 10);

	GuiSetStyle(DEFAULT, TEXT_SIZE, 200);
	Color textColor = GOLD;
	textColor.a = mAlpha * 255;
	GuiDrawText("ACES", text, TEXT_ALIGN_CENTER, textColor);

	GuiSetStyle(DEFAULT, TEXT_SIZE, 50);
	textColor = WHITE;
	textColor.a = mAlpha * 255;
	text.y += height / 16;
	GuiDrawText("ON THE", text, TEXT_ALIGN_CENTER, textColor);

	GuiSetStyle(DEFAULT, TEXT_SIZE, 150);
	textColor = GOLD;
	textColor.a = mAlpha * 255;
	text.y += height / 16;
	GuiDrawText("FIELD", text, TEXT_ALIGN_CENTER, textColor);

	Rectangle button = { (width - buttonWidth) * 0.5f, height * 0.5f, buttonWidth, buttonHeight };

	GuiSetStyle(DEFAULT, TEXT_SIZE, 50);

	if (mPadState.Active) {
		GuiLock();
	}

	if (mPadState.Selection == 0 && mPadState.Active) {
		GuiSetState(STATE_FOCUSED);
		mP1Button = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
	}
	else {
		GuiSetState(STATE_NORMAL);
	}
	if (GuiButton(button, "1 Player")) {
		mP1Button = mMenuActive && true;
	}

	button.y += 1.25f * buttonHeight;

	if (mPadState.Selection == 1 && mPadState.Active) {
		GuiSetState(STATE_FOCUSED);
		mP2Button = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
	}
	else {
		GuiSetState(STATE_NORMAL);
	}
	if (GuiButton(button, "2 Players")) {
		mP2Button = mMenuActive && true;
	}
	mMenuActive = mMenuActive && !mP1Button && !mP2Button;

	GuiUnlock();
}
