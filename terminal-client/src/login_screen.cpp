#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "common/log/log.h"

#include "api_client.h"
#include "login_screen.h"

namespace screens
{

using namespace ftxui;

LoginScreen::LoginScreen(ScreenInteractive& screen, api::Client& apiClient, OnLoginSuccess onSuccess)
    : m_screen(screen)
    , m_apiClient(apiClient)
    , m_onSuccess(std::move(onSuccess))
{

    m_loginInput = Input(&m_login, "Логин");
    m_passwordInput = Input(&m_password, "Пароль");
    m_loginButton = Button(
        "Войти",
        [this]
        {
            onLoginClicked();
        }
    );

    m_layout = Container::Vertical({
        m_loginInput,
        m_passwordInput,
        m_loginButton,
    });
}

Component LoginScreen::component()
{
    return Renderer(
        m_layout,
        [&]
        {
            const Elements content(
                {
                    text("Вход в аккаунт") | bold | center,
                    separator(),
                    hbox({ text("Логин: "), m_loginInput->Render() }) | flex,
                    hbox({ text("Пароль: "), m_passwordInput->Render() }) | flex,
                    m_loginButton->Render() | center,
                    text(m_error) | color(Color::Red) | center,
                }
            );
            return vbox(content)
                | border
                | size(WIDTH, GREATER_THAN, 50)
                | size(HEIGHT, GREATER_THAN, 15)
                | center;
        }
    );
}

void LoginScreen::onLoginClicked()
{
    if (m_loading)
        return;

    m_loading = true;
    m_error.clear();
    LOG_INFO << "Начинаем вход для " << m_login;

    // Синхронный запрос — блокирует UI, но безопасно
    try
    {
        auto result = m_apiClient.login(m_login, m_password).get();
        if (result.success)
        {
            LOG_INFO << "Вход успешен";
            m_onSuccess(result.token);
            // exit будет вызван из колбэка в основном потоке
        }
        else
        {
            m_error = "Ошибка: " + result.errorMessage;
            LOG_ERROR << "Ошибка входа: " << result.errorMessage;
            m_loading = false;
        }
    }
    catch (const std::exception& e)
    {
        m_error = "Ошибка: " + std::string(e.what());
        LOG_ERROR << "Исключение: " << e.what();
        m_loading = false;
    }
}

} // namespace screens
