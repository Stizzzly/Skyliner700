# Skyliner 700

[English version](README.md)

Небольшой самолётный симулятор на чистом **C** и fixed-function **Direct3D 9**. Проект не использует игровой движок, C++, D3DX или шейдеры и рассчитан в том числе на старое DirectX 9-железо.

## Возможности

- Упрощённая физика: тяга, подъёмная сила, сопротивление, сваливание, взлёт, посадка и контакт с землёй
- Процедурная карта с холмами, ВПП, рулёжной площадкой и low-poly ангарами
- Текстурированный самолёт, sky dome, движущиеся облака, туман и HUD
- Камера от третьего лица и свободная камера
- Главное меню, пауза, автотест полёта и телеметрия
- Детерминированные CTest-проверки физики без окна и D3D9

## Управление

| Клавиша | Действие |
| --- | --- |
| `↑` / `↓` | Тангаж |
| `←` / `→` | Крен и поворот |
| `Shift` / `Ctrl` | Больше / меньше тяги |
| `Q` / `E` | Рыскание |
| `R` | Сброс на ВПП |
| `C` | Камера третьего лица / свободная камера |
| `W` `A` `S` `D` + мышь | Движение и обзор в свободной камере |
| `Page Up` / `Page Down` | Вверх / вниз в свободной камере |
| `F5` | Автоматические взлёт, круг и посадка |
| `F6` | Подробная телеметрия |
| `Esc` | Меню паузы |

## Загрузка и запуск

Готовые сборки находятся в [Releases](../../releases).

| Архив | Назначение |
| --- | --- |
| `Skyliner700-1.0.0-win32-xp.zip` | Windows XP SP2/SP3 и более новые 32-битные Windows |
| `Skyliner700-1.0.0-win64.zip` | 64-битные Windows |

Распакуйте архив целиком. Папка `assets` должна находиться рядом с `Skyliner700.exe`.

## Требования

- Windows XP SP2/SP3 или новее
- DirectX 9.0c-совместимая видеокарта, Shader Model 2.0, 64 МБ VRAM
- 512 МБ ОЗУ
- Экран 1024×768
- 100 МБ свободного места

Версия x86 проверена в Windows XP SP2 в VMware. Финальная цель по реальной производительности — ATI Radeon Xpress 200.

## Сборка

Установите CMake, Ninja и MSYS2 MinGW. Команды выполняются из корня репозитория.

### Windows x64 (Clang)

```powershell
C:\msys64\mingw64\bin\cmake.exe --preset clang-release
C:\msys64\mingw64\bin\cmake.exe --build --preset clang-release
```

Результат: `cmake-build-release\Skyliner700.exe`.

### Windows XP x86 (GCC)

```powershell
C:\msys64\mingw32\bin\cmake.exe --preset gcc-x86-xp-release
C:\msys64\mingw32\bin\cmake.exe --build --preset gcc-x86-xp-release
C:\msys64\mingw32\bin\ctest.exe --preset gcc-x86-xp-release --output-on-failure
```

Результат: `cmake-build-gcc-x86-xp-release\Skyliner700.exe`. Это `pei-i386`-сборка с subsystem 5.1, без зависимости от D3DX. Подробности — в [docs/windows-xp.md](docs/windows-xp.md).

## Linux

Windows-версия должна запускаться через Wine + DXVK, который переводит D3D9 в Vulkan. Нужны Wine, DXVK и 32-битный Vulkan-драйвер видеокарты. Запускайте игру вместе с папкой `assets`.

```bash
WINEPREFIX="$HOME/.wine-skyliner700" wine Skyliner700.exe
```

## Технологии

- C11
- Win32 API
- Direct3D 9 fixed-function pipeline
- CMake + Ninja
- CTest

## Лицензия

Проект распространяется по [лицензии MIT](LICENSE).

Сообщения об ошибках и предложения принимаются во [вкладке Issues](../../issues).
