# StreamShelf Connector for Firefox: signing

The localized Firefox and Chromium sources are version 1.0.4 in `extension/firefox-development` and `extension/chromium-development`. The signed Firefox package is `extension/firefox-signed/StreamShelf-Connector-1.0.4.xpi`; its extension files match the Firefox development source (the manifest has equivalent JSON formatting). Firefox installed this package successfully.

- The add-on ID is `connector@streamshelf.local`; it must match `allowed_extensions` in the Native Messaging manifest.
- Firefox 140 or later is required. Users confirm installation and permissions in Firefox.
- The extension sends page URLs, resource information, required request headers, and cookies to the local StreamShelf application through Native Messaging. Review the disclosure and Mozilla data-collection declarations before submission.
- For development, open `about:debugging#/runtime/this-firefox`, choose **Load Temporary Add-on**, and select the `manifest.json` inside `extensions/firefox-development`. This temporary installation is removed when Firefox restarts.
- After changing the Firefox source or version, prepare a ZIP containing the *contents* of `extension/firefox-development` at its root and submit it through the Mozilla Developer Hub for a new signature. Do not put credentials or API keys in the repository.
- Never modify a signed XPI or rename an older package to impersonate a new version. Update the signed filename and application version together, and rebuild the application package after adding a newly signed file.

See the official [signing and distribution overview](https://extensionworkshop.com/documentation/publish/signing-and-distribution-overview/) and [distribution for desktop applications](https://extensionworkshop.com/documentation/publish/distribute-for-desktop-apps/).
