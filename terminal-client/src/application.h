#pragma once

#include <memory>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "api_client.h"

namespace terminal
{

class Application final
{
public:
    Application();
    ~Application();

    int run();

private:
    void showLoginScreen();
    void showMainMenuScreen(const std::string& token);
    void clearScreen();

private:
    ftxui::ScreenInteractive m_screen;
    std::string m_token;
    bool m_loggedIn { false };

    std::unique_ptr<api::Client> m_apiClient;
};

} // namespace terminal