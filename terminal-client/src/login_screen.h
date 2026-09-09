#pragma once

#include <functional>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "api_client.h"

namespace screens
{

class LoginScreen final
{
public:
    using OnLoginSuccess = std::function<void(const std::string& token)>;

    LoginScreen(
        ftxui::ScreenInteractive& screen,
        api::Client& apiClient,
        OnLoginSuccess onSuccess
    );

    ftxui::Component component();

private:
    ftxui::ScreenInteractive& m_screen;
    api::Client& m_apiClient;
    OnLoginSuccess m_onSuccess;

    std::string m_login;
    std::string m_password;
    std::string m_error;
    bool m_loading { false };

    ftxui::Component m_loginInput;
    ftxui::Component m_passwordInput;
    ftxui::Component m_loginButton;
    ftxui::Component m_layout;

    void onLoginClicked();
};

} // namespace screens