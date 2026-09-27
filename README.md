# Book Reader

Приложение для чтения FB2-книг на C++ и Qt Widgets. Данные библиотеки
хранятся в SQLite.

## Структура

```text
src/
  app/          # Точка входа
  data/         # База данных и модели хранения
  parsers/      # Разбор FB2
  ui/
    pages/      # Главная, библиотека, информация о книге, аккаунт
    widgets/    # Боковая и верхняя панели
    windows/    # Главное окно и окно чтения
assets/
  icons/        # Изображения и иконки интерфейса
resources/      # Qt Resource Collection (.qrc)
third_party/
  sqlite/       # Исходники SQLite
installer/      # Установщик Inno Setup
CMakeLists.txt
book project.slnx
book project.vcxproj
```

Заголовки лежат рядом с реализациями. Пути в `#include` задаются относительно
`src/`, например `#include "data/database.h"`. Изображения включены в
исполняемый файл через `.qrc` и доступны по путям `:/icons/...`.

## Сборка в Visual Studio

Откройте `book project.slnx`. Нужны инструменты C++ Visual Studio,
Windows SDK, Qt 6 и расширение Qt Visual Studio Tools.
Проект настроен на Qt `6.11.0_msvc2022_64` и toolset `v145`;
при другой установленной версии выберите её в настройках проекта.
Поддерживаются конфигурации Debug и Release для x64.

## Сборка через CMake

Нужны CMake 3.21 или новее, Qt 6 (Core, Gui, Widgets) и совместимый
компилятор C/C++. Для Windows запускайте команды из Developer PowerShell
Visual Studio. Укажите путь к своей сборке Qt:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/msvc2022_64"
cmake --build build --config Release
```

Имя исполняемого файла сохранено: `book project.exe`.
Иконки отдельно копировать не требуется. Для распространения на Windows
подготовьте папку `deploy/` с помощью `windeployqt`, затем соберите
`installer/book-reader.iss` в Inno Setup.

База `library.db` создаётся приложением в рабочем каталоге и не включается
в Git. Для запуска используйте каталог, доступный для записи.
