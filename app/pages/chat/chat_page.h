// venv-vrequency — the Chat page: a terminal-style conversation with the copilot (see app/chat/chat.h).
#pragma once
#include "app/pages/page.h"

class Chat;

class ChatPage : public Page {
public:
    ChatPage(Shell &s, Chat &c) : Page(s), chat(c) {}
    void paint() override;
    bool key(int qtKey, int plainAct) override;

private:
    Chat &chat;
    qreal dragLastY = 0;
};
