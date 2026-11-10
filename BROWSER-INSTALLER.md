# StreamShelf Connector setup

### English | [Русский](BROWSER-INSTALLER_RU.md)

The guided extension setup is available for Google Chrome, Yandex Browser, and Mozilla Firefox. In the Connection page, select the browser and profile, then choose **Install with guide**. Other compatible browsers require manual installation.

For Chrome and Yandex Browser, the guide walks through developer mode, loading the unpacked extension, checking installation, and connecting the tab. For Firefox, it guides you through installing the signed XPI in the add-ons manager and checking the connection.

The guide waits until it confirms that the extension has appeared. If you cancel folder selection or the browser reports an error, use **Retry loading**. If StreamShelf Connector is disabled, the guide highlights the switch or the enable button. Review and confirm browser prompts yourself; an already enabled extension skips that step.

The extension folder opens in the system file dialog. Select the final **Select Folder** button yourself. The guide's overlay lets clicks pass through and remains visible while switching pages and windows. **Open page** returns to the extension setup page. Closing the guide does not close the browser.

To connect, open StreamShelf Connector from the browser's extensions menu and select **Connect extension**. Read the browser's request for access to all websites and decide whether to grant it. Then open the page you want to analyze: the active tab is selected automatically. StreamShelf verifies the connection through Native Messaging rather than relying only on the extension card.

The guide uses C++20, WinAPI, and UI Automation. The Chromium extension is loaded from `extensions/chromium-development` beside the application. The guide does not rewrite browser installation or enablement settings. As a fallback it reads the state of its own fixed extension ID in Preferences/Secure Preferences; live interface state takes precedence, and empty leftover entries are not treated as an installation.

To build, run `cmake --preset dev` followed by `cmake --build --preset dev`. Native desktop tests for the guide can be enabled with the CMake option `STREAMSHELF_NATIVE_GUIDE_TESTS=ON`.

The end-to-end installation check with manual confirmations can be run with `scripts/Test-BrowserGuide.ps1 -Browser chrome -InstallationFlow` (or `yandex`). It records the initially absent extension, the observed steps, and confirmed enablement; simply opening the page is not considered a successful installation.

The guide checks the extension connection and uses an animated outline and dimmed overlay around the target. Reduced motion keeps a static outline. Connection state is checked through Native Messaging; multiple profiles of the same browser require additional testing. The ChromeExtensionInstaller approach was adapted into a native module; its MIT notice is in `licenses/ChromeExtensionInstaller-LICENSE.txt`.
