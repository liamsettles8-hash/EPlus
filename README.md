# E#+ Studio 0.2.0

E#+ Studio is a native Windows IDE for the E#+ English-first programming language.

## GitHub updater setup

Open `src/studio/studio.c` and change:

```c
#define EPLUS_REPO_OWNER L"YOUR_GITHUB_USERNAME"
#define EPLUS_REPO_NAME L"EPlus"
```

The updater checks the public GitHub Releases API for the latest non-prerelease release. It looks for an asset named `EPlusStudio-Setup.exe`. GitHub's public latest-release endpoint can be used without authentication.

Also edit `website/config.js` with the same owner/repository.

## Website

The `website/` folder is a static GitHub Pages site. In GitHub: Settings -> Pages -> Source -> GitHub Actions. The included workflow deploys the `website/` folder.

## Release process

1. Change `EPLUS_VERSION` in `src/studio/studio.c`.
2. Change `AppVersion` in `installer/EPlusStudio.iss`.
3. Run `build-studio.bat`.
4. Run `build-installer.bat`.
5. Create a GitHub Release with a tag such as `v0.3.0`.
6. Upload `installer-output\EPlusStudio-Setup.exe` to the release.
7. Publish the release.

E#+ Studio will then detect the new version on startup.

## Important

The standalone installer is not automatically trusted by Windows just because it is hosted on GitHub. A trusted public code-signing certificate is still needed to avoid publisher/security warnings outside the Microsoft Store.
