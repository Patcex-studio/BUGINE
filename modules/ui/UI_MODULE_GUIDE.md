# UI Module Guide

## Overview

Модуль `ui` обеспечивает работу пользовательского интерфейса на основе ImGui. Архитектура построена вокруг:

- `UIContext` — главный менеджер жизненного цикла UI
- `IPanel` — интерфейс панели, отвечающей за рендер и логику
- `ServiceLocator` — доступ к движку и сервисам
- `widgets` — повторно используемые элементы UI

## Слои отрисовки

Каждая панель возвращает свой слой через `IPanel::GetLayer()`.
Порядок рендеринга:

- `UILayer::HUD`
- `UILayer::GameUI`
- `UILayer::Editor`
- `UILayer::Modal`

Это означает, что модальные окна находятся поверх всего, а HUD рисуется первым.

## Как добавить новую панель

1. Создайте заголовок `modules/ui/include/ui/panels/MyPanel.h`.
2. Наследуйте `ui::IPanel`.
3. Реализуйте методы:
   - `OnAttach(const ServiceLocator& locator)` — получаем сервисы
   - `OnDetach()` — очищаем указатели
   - `OnUpdate(float deltaTime)` — обновляем данные
   - `OnUIRender()` — рисуем ImGui
   - `GetLayer() const` — возвращаем слой
   - `IsVisible() const` / `SetVisible(bool)` — управление видимостью
4. Добавьте реализацию в `modules/ui/src/panels/MyPanel.cpp`.
5. В `modules/ui/src/CMakeLists.txt` добавьте `panels/MyPanel.cpp`.
6. Зарегистрируйте панель в вашем коде:

```cpp
uiContext.AddPanel(std::make_unique<ui::panels::MyPanel>());
```

### Пример шаблона панели

```cpp
class MyPanel : public ui::IPanel {
public:
    void OnAttach(const ui::ServiceLocator& locator) override {
        rendering_engine_ = locator.GetRenderingEngine();
    }

    void OnDetach() override {
        rendering_engine_ = nullptr;
    }

    void OnUpdate(float deltaTime) override {
        // Обновление данных без рендера
    }

    void OnUIRender() override {
        if (!visible_) return;
        if (!ImGui::Begin("My Panel", &visible_)) {
            ImGui::End();
            return;
        }
        ImGui::Text("Hello from MyPanel");
        ImGui::End();
    }

    UILayer GetLayer() const override {
        return UILayer::GameUI;
    }

    bool IsVisible() const override { return visible_; }
    void SetVisible(bool visible) override { visible_ = visible; }

private:
    rendering_engine::RenderingEngine* rendering_engine_ = nullptr;
    bool visible_ = true;
};
```

## Как создать виджет

В модуле `modules/ui/include/widgets` лежат небольшие UI-компоненты.

1. Наследуйте `ui::IWidget`.
2. Реализуйте `void Draw(const WidgetContext& ctx)`.
3. Если виджет должен поддерживать внешний стиль — добавьте сеттеры и параметры конструктора.
4. Используйте `WidgetContext` для передачи настроек и состояния.

### Пример виджета

```cpp
class StatusIcon : public ui::IWidget {
public:
    void Draw(const ui::WidgetContext& ctx) override {
        ImGui::Text("Status: OK");
    }
};
```

## Как биндить в Lua

Модуль `ui` уже содержит систему `ui::scripting::LuaUIBinding`.

1. Зарегистрируйте функции в Lua через `LuaUIBinding::Register(lua)`.
2. Используйте `UIEventSystem` для вызова событий из Lua.
3. Выносите логику UI в Lua только для быстрой прототипировки.

### Пример биндинга

```cpp
lua["create_ui_panel"] = []() {
    // Создать панель из Lua
};
```

## Чек-лист для ревью UI-кода

- [ ] Нет глобальных переменных
- [ ] Данные передаются по `const&` или через `std::span`
- [ ] Нет аллокаций в `OnUIRender()`
- [ ] Все цвета и шрифты через `ThemeManager` или `ImGuiStyle`
- [ ] Панель корректно обрабатывает `nullptr` от `ServiceLocator`
- [ ] В `OnDetach()` все указатели обнуляются
- [ ] Панель не обращается к движку напрямую, а использует `ServiceLocator`

## Как добавить панель за 30 минут

1. Скопируйте шаблон панели.
2. Определите интерфейс данных и зависимости через `ServiceLocator`.
3. Зарегистрируйте панель в `UIContext`.
4. Напишите простой `OnUIRender()` с одним окном и несколькими полями.
5. Проверьте, что `GetLayer()` возвращает нужный слой.
6. Убедитесь, что панель корректно скрывается/показывается.

## Советы по тестированию

- Запустите игру с UI и убедитесь, что HUD-слой виден поверх других элементов.
- Откройте редактор или модальное окно и убедитесь, что HUD больше не мешает.
- Проверьте, что при отключении UI модуль собирается без ошибок.
- Используйте `UIContext::GetServiceLocator()` для передачи реальных сервисов в тестах.
