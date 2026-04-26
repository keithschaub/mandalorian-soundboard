# iPhone 6s App Update Procedure

Every time a new version is pushed to GitHub, the iPhone 6s requires a **manual cache clear** before it will load the new version. Simply deleting the app from the home screen is NOT enough — the service worker cache persists in Safari.

---

## Standard Update Steps

### Step 1 — Push the new version (on PC)
```
git add .
git commit -m "your message"
git push origin main
```
Wait **2–3 minutes** for GitHub Pages to propagate.

### Step 2 — Delete the app from iPhone 6s home screen
Press and hold the app icon → Remove App → Delete App.

### Step 3 — Clear the Safari cache for the site
**Settings → Safari → Advanced → Website Data**
- Search for `keithschaub`
- Swipe left on the entry → **Delete**

> **Why this is required:** iOS Safari's service worker caches all app files locally (HTML, JS, MP3s) under a versioned cache name. Even after deleting the home screen app, the cache remains in Safari's storage. Deleting it forces Safari to download everything fresh, including the new service worker.

### Step 4 — Reload and reinstall
1. Open Safari → go to:
   **https://keithschaub.github.io/mandalorian-soundboard/iphone_6s/**
2. Wait for the page to fully load (all sounds cached)
3. Verify the version number in the heading matches the new release
4. Tap **Share → Add to Home Screen**

---

## Quick Verification

After reinstalling, confirm you're on the right version:
- The app title on screen should show the new version (e.g. `MANDALORIAN v5.9.7`)
- You can also check in Private Mode first — Private Mode bypasses service worker cache and always loads the live version from GitHub

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Still shows old version after cache delete | Wait another 2 min for GitHub Pages CDN to update, then repeat Step 3 |
| Private mode shows new version but regular doesn't | Cache delete in Step 3 wasn't applied — repeat it |
| Sounds don't play offline | The cache didn't fully load — open in regular Safari (not private), wait 30 sec, then add to home screen |
