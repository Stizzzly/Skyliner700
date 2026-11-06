# DeviceInventory 0.1.1 — Nokia 6600 / EKA1

Диагностика гипотезы об отдельном LDD клавиатуры. UID 0x0F700662.
[Установщик](dist/DeviceInventory-0.1.1.sis).

Откройте приложение: список собирается автоматически. Вверх/вниз или 2/8 —
страницы, 5 — повторный сбор, правая софт-клавиша/# — выход.
7 или левая софт-клавиша — отправить отчёт через Bluetooth.
9 — скопировать `dhal.ldd` и `helper.ldd` из ROM в `C:\DriverStudy\` как
`dhal.bin` / `helper.bin` и предложить их отправку вместе с отчётом через
штатный Bluetooth Send UI. Получателя выбирает пользователь.
На компьютере заранее запустите fsquirt.exe → «Принять файлы».
Эти копии нужны для локального анализа; vendor binaries не публикуются в Git.
Если отправка/копирование не поддержаны, код ошибки остаётся в списке.
Полный отчёт: `C:\DeviceInventory.log` (перезаписывается при новом Scan).

Собираются имена:
- зарегистрированных логических устройств через TFindLogicalDevice;
- физических устройств через TFindPhysicalDevice;
- логических каналов через TFindLogicalChannel;
- ROM-файлов `Z:\system\libs\*.ldd`, `*.pdd`, `*key*`, `*kbd*`, `*kdata*`.

Next status -1 — обычное окончание перечисления (KErrNotFound).
GetDir -1/-12 может означать отсутствие совпадений/пути; это не отказ открытия
клавиатуры. Предел перечисления каждого типа — 512 объектов, с отметкой LIMIT.
Пустой список не доказывает отсутствие клавиатурного кода: он может быть
расширением ядра/базового порта, не отдельным логическим устройством.

## Чего этот результат ещё не доказывает

Имена `Keyboard` и `EKDATA.LDD` на Nokia 6600 пока не установлены. В SDK 2.0
найдена `ekdata.dll` с экспортами KeyDataConvTable/KeyDataFuncTable/KeyDataSettings:
это свидетельство таблиц преобразования, не готового канала аппаратного ввода.
Существование объекта LDD не доказывает поддержку второго канала, формат
запросов, shared memory или отсутствие влияния на поток событий WSERV.
DoCreate в RBusLogicalChannel защищённый метод: реальный интерфейс обычно
предоставляет производный R-класс с точной версией и кодами запросов.

Программа не загружает/выгружает драйверы, не открывает каналы, не вызывает
DoControl/DoRequest и не захватывает kernel event hook. Она записывает только
свой отчёт и показывает перечисленные имена. Версия 0.1.1 с Bluetooth Send UI ещё не проверена на телефоне.

## Сборка

Оригинальный S60 2nd Edition SDK 2.0 + Windows Perl, короткие пути без пробелов.
`build-s60.ps1 -SdkRoot C:\S60SDK` создаёт DeviceInventory.sis.
`python tests/verify_package.py DeviceInventory.sis SDK` проверяет UID,
APP/RSC и их соответствие результатам SDK. SDK не включён в репозиторий.
Нативная сборка ARMI UREL и проверка двухфайлового SIS проходят.

SHA256 SIS: `ce5bf9e32be671e126c86a6b408f2361119626b3ef2e4cbd75097dcfac94354d`

## Наблюдение на реальном телефоне (0.1.0)

Пользователь предоставил фотографии всех LDD/PDD и всего ROM-списка LDD.
9 логических устройств: HelperDrv, ISA_IF_PROTOCOL_BASE, ISA_IF_DRIVER,
RHalDriver, Lcd, bt_driver, camdrv, Comm, Audioldd.
5 физических устройств: Media.Lffs, Media.IRam, Media.MmcF, camdrv.pdd, Comm.PDD.
9 LDD-файлов: audio.ldd, btldd.ldd, camldd.ldd, Dhal.ldd, ecomm.ldd,
enosdriver.ldd, helper.ldd, isa_if_driver.ldd, lcd.ldd.
Log open 0; Next status -1. Явно названного Keyboard-устройства нет.
Назначение HelperDrv/RHalDriver и их протокол не установлены: следующий шаг —
анализ actual ROM binaries, не перебор кодов DoControl.
Файлы *.bin и каталог extracted исключены из Git; копии драйверов на телефон
не загружаются как LDD и используются только как attachments.
