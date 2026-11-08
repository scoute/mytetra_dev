# msvcr100.dll для Windows-портабля (см. fix/571)

Что: Microsoft Visual C++ 2010 SP1 Redistributable, x86,
версия 10.0.40219.325, файл `msvcr100.dll` (773 968 байт, PE32).

Зачем: OpenSSL 1.1 (`libssl-1_1.dll`, `libcrypto-1_1.dll`), который
`build_portable_windows.sh` кладет в портабль из `QtCreator/bin`,
собран MSVC 2010 и линкуется именно к MSVCR100. Весь остальной
портабль (exe, Qt, MinGW-рантайм) живет на `msvcrt.dll`, который
есть в любой Windows. Без `msvcr100.dll` рядом с exe OpenSSL
не грузится и https-качалки (картинки из интернета, веб-клиппер)
молча не работают на машинах без установленного VC++ 2010.

Откуда взят: официальный пакет `Microsoft.VCRedist.2010.x86`
(установлен через `winget install -e --id Microsoft.VCRedist.2010.x86`
на тестовой Win10, хеш установщика проверен winget; файл скопирован
из `C:\Windows\SysWOW64`). Файлы этого пакета Microsoft разрешает
распространять вместе с приложением (redist-лицензия пакета).

Использование: `build_portable_windows.sh` копирует файл рядом
с `mytetra.exe` (Windows ищет DLL рядом с exe сам). Отсутствие
файла при наличии OpenSSL в сборке — жесткая ошибка упаковки,
а не предупреждение: молча сломанный https уже проходили.
