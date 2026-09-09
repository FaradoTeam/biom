#include <cstdlib>

#include "common/log/log.h"

#include "application.h"
#include "login_screen.h"
#include "main_menu_screen.h"

namespace terminal
{

void Application::clearScreen()
{
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

Application::Application()
    : m_screen(ftxui::ScreenInteractive::TerminalOutput())
    , m_loggedIn(false)
    , m_apiClient(std::make_unique<api::Client>("http://localhost:8090")) // TODO: брать из конфига или командной строки
{
    LOG_INFO << "Application создан";
}

Application::~Application()
{
    LOG_INFO << "Application уничтожен";
}

int Application::run()
{
    LOG_INFO << "Запуск приложения";

    while (true)
    {
        if (!m_loggedIn)
        {
            LOG_INFO << "Показываем экран логина";
            showLoginScreen();
            if (!m_loggedIn)
            {
                LOG_INFO << "Вход не выполнен, завершаем";
                break;
            }
        }
        else
        {
            LOG_INFO << "Показываем главное меню";
            showMainMenuScreen(m_token);
            if (!m_loggedIn)
            {
                LOG_INFO << "Выход из системы, возврат к логину";
                continue;
            }
            LOG_INFO << "Выход из приложения";
            break;
        }
    }

    LOG_INFO << "Приложение завершено";
    return 0;
}

void Application::showLoginScreen()
{
    screens::LoginScreen loginScreen(
        m_screen,
        *m_apiClient,
        [this](const std::string& token)
        {
            LOG_INFO << "Успешный вход, token получен";
            m_token = token;
            m_loggedIn = true;
            m_screen.Exit();
        }
    );

    auto component = loginScreen.component();
    m_screen.Loop(component);
    clearScreen();
    LOG_INFO << "Экран логина завершён";
}

void Application::showMainMenuScreen(const std::string& token)
{
    screens::MainMenuScreen mainMenuScreen(
        m_screen,
        token,
        *m_apiClient,
        [this]()
        {
            LOG_INFO << "Выход из системы";
            // Асинхронно вызываем logout, не дожидаясь ответа
            m_apiClient->logout(m_token);
            m_token.clear();
            m_loggedIn = false;
            m_screen.Exit();
        }
    );

    auto component = mainMenuScreen.component();
    m_screen.Loop(component);
    clearScreen();
    LOG_INFO << "Главное меню завершено";
}

} // namespace terminal
