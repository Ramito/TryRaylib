#include "Menu.h"

#include "raygui.h"

void Menu::UpdateMenu(std::function<void(uint32_t)>&& startgameAction)
{
    mSelectionState.Active = mMenuActive;
    if (mSelectionState.Active) {
        constexpr int SelectionCount = 2;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN) || IsKeyPressed(KEY_S)) {
            mSelectionState.Selection += 1;
            if (mSelectionState.Selection >= SelectionCount) {
                mSelectionState.Selection -= SelectionCount;
            }
        }
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP) || IsKeyPressed(KEY_W)) {
            mSelectionState.Selection -= 1;
            if (mSelectionState.Selection < 0) {
                mSelectionState.Selection += SelectionCount;
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
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) || IsKeyPressed(KEY_ENTER)) {
        mMenuActive = !mMenuActive;
    }
}
void Menu::DrawMenu()
{
    if (mMenuActive) {
        mAlpha = std::min(1.f, mAlpha + 0.05f);
        GuiFade(BLANK, mAlpha);
        if (mAlpha) {
            GuiEnable();
        }
    } else {
        mAlpha = std::max(0.f, mAlpha - 0.025f);
        GuiFade(BLANK, mAlpha);
        if (mAlpha == 0.f) {
            GuiDisable();
            return;
        }
    }
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    float buttonWidth = width / 3.f;
    float buttonHeight = height / 5.f;

    Rectangle text = {0, height / 8.f, width * 1.f, height / 4.f};
    GuiSetStyle(DEFAULT, TEXT_SPACING, 10);

    const int alpha = static_cast<int>(mAlpha * 255);

    GuiSetStyle(DEFAULT, TEXT_SIZE, 200);
    Color textColor = GOLD;
    textColor.a = alpha;
    GuiDrawText("ACES", text, TEXT_ALIGN_CENTER, textColor);

    GuiSetStyle(DEFAULT, TEXT_SIZE, 50);
    textColor = WHITE;
    textColor.a = alpha;
    text.y += height / 16;
    GuiDrawText("ON THE", text, TEXT_ALIGN_CENTER, textColor);

    GuiSetStyle(DEFAULT, TEXT_SIZE, 150);
    textColor = GOLD;
    textColor.a = alpha;
    text.y += height / 16;
    GuiDrawText("FIELD", text, TEXT_ALIGN_CENTER, textColor);

    Rectangle button = {(width - buttonWidth) * 0.5f, height * 0.5f, buttonWidth, buttonHeight};

    GuiSetStyle(DEFAULT, TEXT_SIZE, 50);

    if (mSelectionState.Active) {
        GuiLock();
    }

    if (mSelectionState.Selection == 0 && mSelectionState.Active) {
        GuiSetState(STATE_FOCUSED);
        mP1Button = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsKeyPressed(KEY_SPACE);
    } else {
        GuiSetState(STATE_NORMAL);
    }
    if ((mAlpha == 1.f) && GuiButton(button, "1 Player")) {
        mP1Button = true;
    }

    button.y += 1.25f * buttonHeight;

    if (mSelectionState.Selection == 1 && mSelectionState.Active) {
        GuiSetState(STATE_FOCUSED);
        mP2Button = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsKeyPressed(KEY_SPACE);
    } else {
        GuiSetState(STATE_NORMAL);
    }
    if ((mAlpha == 1.f) && GuiButton(button, "2 Players")) {
        mP2Button = true;
    }
    mMenuActive = mMenuActive && !mP1Button && !mP2Button;

    GuiUnlock();
}
