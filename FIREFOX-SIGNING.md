# StreamShelf Connector для Firefox: подготовка подписи

Исходники: `extension/firefox-development`. Текущая версия расширений Firefox и Chromium — 1.0.3. Подписанный пакет Firefox: `extension/firefox-signed/StreamShelf-Connector-1.0.3.xpi`. Текст для страницы и рецензента: `FIREFOX-AMO-DESCRIPTION.md`.

- ID дополнения: `connector@streamshelf.local`. Он должен совпадать с `allowed_extensions` в манифесте Native Messaging.
- Минимальная версия: Firefox 140.0. Пользователь подтверждает установку и заявленные разрешения в Firefox.
- Расширение передаёт URL, сведения о ресурсах, необходимые HTTP-заголовки и cookies локальному приложению StreamShelf через Native Messaging. Сбор данных ограничен функциями приложения; расширение не заявляет отправку этих данных на сервер разработчика.
- Манифест заявляет категории `browsingActivity`, `websiteContent` и `authenticationInfo`, поскольку данные покидают браузер и могут содержать данные авторизации. Перед публикацией проверьте актуальность описания и политики Mozilla.
- Для подписи нужен аккаунт разработчика на addons.mozilla.org. Пройдите регистрацию и отправьте ZIP через Developer Hub; не передавайте секреты, пароли и API-ключи в чат. Канал распространения и описание необходимо проверить перед публикацией.
- Подписанный XPI версии 1.0.3 извлечён из профиля Firefox. Код и данные манифеста соответствуют исходникам, внутри присутствуют файлы подписи Mozilla; криптографическая проверка отдельно не проводилась. Мастер использует этот XPI из папки программы.
- Обычный Firefox не поддерживает незаметную установку дополнения сторонней программой. Пользователь должен подтвердить установку; корпоративные политики представляют собой отдельный сценарий.

Ссылки: https://extensionworkshop.com/documentation/publish/signing-and-distribution-overview/ и https://extensionworkshop.com/documentation/publish/distribute-for-desktop-apps/.
