# MyMonitoring
Для запуска нужно установить через пкетный менеджер  ./vcpkg install nlohmann-json

Дальше при сборке в visulastudioОткройте свойства проекта (правый клик на проекте в Solution Explorer и выберите "Properties").
Перейдите в раздел "Linker" -> "System".
Измените "Subsystem" на "Windows (/SUBSYSTEM:WINDOWS)".
Из за большого размера проект лежит по ссылке https://disk.yandex.ru/d/jD93qouwp2uPUQ

Сервер для запуска находится в папке pyServer для запуска python pyServer.py 
