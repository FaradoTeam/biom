#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "common/log/log.h"

#include "main_menu_screen.h"

namespace screens
{

using namespace ftxui;

MainMenuScreen::MainMenuScreen(
    ScreenInteractive& screen,
    const std::string& token,
    api::Client& apiClient,
    OnLogout onLogout
)
    : m_screen(screen)
    , m_token(token)
    , m_apiClient(apiClient)
    , m_onLogout(std::move(onLogout))
    , m_entries({ "Проекты", "Задачи", "Доски", "Выйти" })
    , m_selected(0)
{

    auto menu = Menu(&m_entries, &m_selected);
    auto menu_component = CatchEvent(
        menu,
        [this](Event event)
        {
            if (event == Event::Return)
            {
                if (m_selected == 3)
                { // "Выйти"
                    LOG_INFO << "Выбран выход";
                    m_onLogout(); // вызывает выход и завершает цикл
                    return true;
                }
                else
                {
                    LOG_INFO << "Выбран пункт: " << m_entries[m_selected];
                    m_screen.Exit(); // завершает текущий экран
                    return true;
                }
            }
            return false;
        }
    );

    m_menu = Renderer(
        menu_component,
        [this]
        {
            Elements items;
            for (size_t i = 0; i < m_entries.size(); ++i)
            {
                if (i == static_cast<size_t>(m_selected))
                {
                    items.push_back(text(m_entries[i]) | bold | color(Color::Yellow));
                }
                else
                {
                    items.push_back(text(m_entries[i]));
                }
            }
            auto menu_elem = vbox(std::move(items));
            const Elements content(
                {
                    text("Главное меню") | bold | center,
                    separator(),
                    menu_elem,
                }
            );
            return vbox(content)
                | border
                | size(WIDTH, GREATER_THAN, 40)
                | size(HEIGHT, GREATER_THAN, 10);
        }
    );
}

Component MainMenuScreen::component()
{
    return m_menu;
}

} // namespace screens
