# Beta: Firefox connector is not included

This beta package does not contain a Mozilla-signed StreamShelf Connector
1.0.5 XPI. Permanent Firefox installation and the Firefox setup guide will not
work until the signed package is supplied. Chromium-family browser setup is
available. See [FIREFOX-SIGNING.md](FIREFOX-SIGNING.md) for details.

Other known beta limitations:

- Closing the entire application can discard an unsaved builder draft without a separate warning.
- Queue-history validation does not yet detect every field with an invalid type.
- Some builder scrolling and navigation cannot yet be completed using only a keyboard.

# Бета: расширение Firefox не включено

В этот бета-пакет не входит подписанный Mozilla XPI-файл StreamShelf Connector
1.0.5. Постоянная установка в Firefox и мастер подключения Firefox заработают
после добавления подписанного пакета. Для браузеров семейства Chromium установка
доступна. Подробности: [FIREFOX-SIGNING.md](FIREFOX-SIGNING.md).

Другие известные ограничения беты:

- Закрытие всего приложения может удалить несохранённый черновик конструктора без отдельного предупреждения.
- Проверка истории очереди пока обнаруживает не все поля с неверным типом.
- Некоторые области конструктора пока нельзя прокрутить и полностью использовать только с клавиатуры.
