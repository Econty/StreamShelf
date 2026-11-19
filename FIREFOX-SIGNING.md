# StreamShelf Connector for Firefox: signing

The shared implementation lives in `extension/shared`. Edit it there, then run `cmake -P scripts/Prepare-Extensions.cmake` to regenerate both browser packages. Their manifests remain browser-specific; `extension/firefox-development` is the prepared input for Mozilla signing.

The Firefox and Chromium development sources are version 1.0.6. The repository includes the Mozilla-signed Firefox package at `extension/firefox-signed/StreamShelf-Connector-1.0.6.xpi`. An already installed 1.0.5 add-on can still connect, but it does not include the new page and cookie handling. Keep its original XPI intact; it cannot be used as the 1.0.6 release package.

Normal Release installation, including a direct `cmake --install`, requires Python 3 and Windows PowerShell. It checks the XPI's version, add-on ID, correspondence to the current source, signing-manifest hashes, PKCS#7 signature, and certificate chain to the Mozilla production root pinned in `scripts/mozilla-addons-public.pem`. Text line endings and a final manifest newline may differ; other content must match. Verification is offline and does not check certificate revocation. The explicit `STREAMSHELF_BETA_WITHOUT_FIREFOX` option stages a beta without the signed XPI, as described in [BUILDING.md](BUILDING.md).

- The add-on ID is `connector@streamshelf.local`; it must match `allowed_extensions` in the Native Messaging manifest.
- Firefox 140 or later is required. Users confirm installation and permissions in Firefox.
- The extension sends page URLs, resource information, required request headers, and cookies to the local StreamShelf application through Native Messaging. Review the disclosure and Mozilla data-collection declarations before submission.
- For development, open `about:debugging#/runtime/this-firefox`, choose **Load Temporary Add-on**, and select the `manifest.json` inside `extensions/firefox-development`. This temporary installation is removed when Firefox restarts.
- After changing the Firefox source or version, prepare a ZIP containing the *contents* of `extension/firefox-development` at its root and submit it through the Mozilla Developer Hub for a new signature. Do not put credentials or API keys in the repository.
- Never modify a signed XPI or rename an older package to impersonate a new version. Update the signed filename and application version together, and rebuild the application package after adding a newly signed file.

See the official [signing and distribution overview](https://extensionworkshop.com/documentation/publish/signing-and-distribution-overview/) and [distribution for desktop applications](https://extensionworkshop.com/documentation/publish/distribute-for-desktop-apps/).
