# Skyliner 700

Небольшой самолётный симулятор на чистом **C** и **Direct3D 9**.  Никакого
игрового движка, C++, D3DX или шейдеров: рендерер использует fixed-function
pipeline и рассчитан в том числе на старые видеокарты уровня ATI Radeon Xpress
200.

![Language: C](https://img.shields.io/badge/language-C-00599C?logo=c&logoColor=white)
![Graphics: Direct3D 9](https://img.shields.io/badge/graphics-Direct3D%209-107C10)
![Platform: Windows](https://img.shields.io/badge/platform-Windows-0078D4?logo=windows&logoColor=white)

## Что есть в игре

- управляемый самолёт с упрощённой физикой: тяга, подъёмная сила, сваливание,
  сопротивление, взлёт и посадка;
- небольшая процедурная карта: холмы, ВПП, рулёжная площадка и low-poly ангары;
- гражданская ливрея самолёта, облачный sky dome, туман и HUD;
- камера от третьего лица и свободная камера;
- главное меню и пауза;
- встроенный тестовый полёт (`F5`) и детальная телеметрия (`F6`);
- отдельные детерминированные CTest-проверки физики без окна и D3D9.

## Управление

| Клавиша | Действие |
| --- | --- |
| `↑` / `↓` | Тангаж |
| `←` / `→` | Крен и поворот самолёта |
| `Shift` / `Ctrl` | Больше / меньше тяги |
| `Q` / `E` | Рыскание |
| `R` | Сброс на начало ВПП |
| `C` | Камера третьего лица / свободная камера |
| `W` `A` `S` `D`, мышь | Движение и обзор в свободной камере |
| `Page Up` / `Page Down` | Вверх / вниз в свободной камере |
| `F5` | Автотест: взлёт, круг и посадка |
| `F6` | Расширенная телеметрия |
| `Esc` | Пауза, возврат в меню или выход |

## Скачать и запустить

В [Releases](../../releases) доступны два ZIP-архива:

| Архив | Для чего |
| --- | --- |
| `Skyliner700-1.0.0-win32-xp.zip` | Windows XP SP2/SP3 и более новые 32-битные Windows |
| `Skyliner700-1.0.0-win64.zip` | 64-битные Windows |

Распакуйте архив целиком и запускайте `Skyliner700.exe`.  Папка `assets` должна
оставаться рядом с `.exe`: в ней находятся текстуры самолёта, облаков, травы и
ВПП.

### Минимальные требования

- Windows XP SP2/SP3 или новее;
- DirectX 9.0c-совместимая видеокарта с Shader Model 2.0 и 64 МБ видеопамяти;
- 512 МБ ОЗУ;
- экран 1024×768;
- около 100 МБ свободного места.

Версия x86 проверена в Windows XP SP2 в VMware. Финальная проверка на реальном
железе — ноутбук с ATI Radeon Xpress 200.

## Сборка

Нужны CMake, Ninja и MinGW из MSYS2. Все команды выполняются из корня
репозитория.

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

Результат: `cmake-build-gcc-x86-xp-release\Skyliner700.exe`.
Сборка создаётся как `pei-i386`, с subsystem версии 5.1, и не требует D3DX.
Подробности по XP — в [docs/windows-xp.md](docs/windows-xp.md).

## Linux

Игра также должна запускаться через Wine с DXVK: DXVK реализует D3D9 поверх
Vulkan. Нужны 32-битные библиотеки Wine/DXVK и Vulkan-драйвер для видеокарты.
Запускайте Windows-версию вместе с папкой `assets`.

```bash
WINEPREFIX="$HOME/.wine-skyliner700" wine Skyliner700.exe
```

## Технологии

- C11;
- Win32 API;
- Direct3D 9 fixed-function pipeline;
- CMake + Ninja;
- CTest для физики полёта.

---

Проект находится в активной разработке. Проблемы и предложения можно оставить
во [вкладке Issues](../../issues).
