#pragma once

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "api_client.h"

namespace screens
{

class MainMenuScreen final
{
public:
    using OnLogout = std::function<void()>;

    MainMenuScreen(
        ftxui::ScreenInteractive& screen,
        const std::string& token,
        api::Client& apiClient,
        OnLogout onLogout
    );

    ftxui::Component component();

private:
    ftxui::ScreenInteractive& m_screen;
    std::string m_token;
    api::Client& m_apiClient;

    OnLogout m_onLogout;
    ftxui::Component m_menu;

    std::vector<std::string> m_entries;
    int m_selected;
};

} // namespace screens