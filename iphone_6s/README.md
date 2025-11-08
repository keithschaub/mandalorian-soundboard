# 🪖 Mandalorian Soundboard - iPhone 6s Edition

A fully offline-capable Progressive Web App (PWA) designed specifically for iPhone 6s. This version runs completely locally on your iPhone without requiring WiFi or cellular connectivity after the initial installation.

## Features

- ✅ **100% Offline Operation** - Works without WiFi or cellular after initial setup
- ✅ **PWA Installation** - Install like a native app on your iPhone home screen
- ✅ **20 Mandalorian Sounds** - All your favorite Mando and Grogu sounds
- ✅ **Background Music Toggle** - Loop theme music while playing other sounds
- ✅ **Volume Control** - Adjustable volume slider
- ✅ **Color-Coded Buttons** - Easy visual identification
- ✅ **Optimized for iPhone 6s** - Perfect screen size and touch responsiveness

## Installation Instructions

### First-Time Setup (Requires WiFi)

1. **Access the App via GitHub Pages**
   - Connect your iPhone 6s to WiFi
   - Open Safari browser
   - Navigate to: `https://[your-username].github.io/Mandalorian_Sound_Board/iphone_6s/`
   - Wait for all files to load (you'll see "OFFLINE MODE" indicator when ready)

2. **Install to Home Screen**
   - Tap the Share button (square with arrow pointing up) at the bottom of Safari
   - Scroll down and tap "Add to Home Screen"
   - Tap "Add" in the top right
   - The app icon will appear on your home screen

3. **Verify Offline Functionality**
   - Turn off WiFi and cellular data
   - Open the app from your home screen
   - Tap "ENABLE AUDIO" button
   - Test playing sounds - everything should work offline!

### Using the App

1. **Launch the App**
   - Tap the app icon on your home screen
   - The app opens in fullscreen mode (no Safari browser UI)

2. **Enable Audio**
   - Tap the "🔊 ENABLE AUDIO" button (required for iOS audio to work)
   - The soundboard will appear

3. **Play Sounds**
   - Tap any button to play a sound
   - Orange buttons (MANDO THEME, MANDO FLUTE) can be toggled on/off for looping background music
   - Blue buttons are Mando's phrases
   - Green buttons are Grogu sounds

4. **Adjust Volume**
   - Use the volume slider at the bottom
   - Volume affects all sounds including background music

## Color Coding

- 🔵 **BLUE** = Mando (Din Djarin's signature phrases)
- 🟢 **GREEN** = Grogu (Baby Yoda sounds)
- 🟠 **ORANGE** = Themes (Music that can loop in background)

## Technical Details

### Offline Functionality

This app uses a Service Worker to cache all resources locally:
- HTML, CSS, JavaScript files
- All 20 MP3 audio files
- Manifest and icons

Once cached, the app works completely offline. The service worker automatically updates when you have internet connection.

### File Structure

```
iphone_6s/
├── index.html          # Main app file
├── sw.js              # Service Worker for offline caching
├── manifest.json      # PWA manifest
├── icon-192.png      # App icon (192x192)
├── icon-512.png      # App icon (512x512)
├── README.md         # This file
└── Sounds/           # Audio files directory
    ├── Mando Theme.mp3
    ├── Mando Flute.mp3
    └── ... (18 more sounds)
```

### Browser Compatibility

- ✅ Safari iOS 11.3+ (iPhone 6s and newer)
- ✅ Chrome iOS (uses Safari engine)
- ⚠️ Older iOS versions may have limited PWA support

## Troubleshooting

### Audio Not Playing
- Make sure you tapped "ENABLE AUDIO" button first
- Check that volume is not set to 0
- Try closing and reopening the app

### App Not Working Offline
- Make sure you accessed the app via WiFi first to cache all files
- Check that Service Worker is enabled in Safari settings
- Try reinstalling the app to home screen

### Sounds Not Loading
- Ensure you have internet connection for first-time setup
- Wait for all files to download before going offline
- Check Safari's storage settings (Settings > Safari > Advanced > Website Data)

### Can't Install to Home Screen
- Make sure you're using Safari (not Chrome or other browsers)
- The "Add to Home Screen" option appears in the Share menu
- Some corporate/managed devices may restrict PWA installation

## Development

### Local Testing

1. Serve the files using a local web server:
   ```bash
   # Using Python
   python -m http.server 8000
   
   # Using Node.js http-server
   npx http-server -p 8000
   ```

2. Access via `http://localhost:8000/iphone_6s/`

3. Test Service Worker in browser DevTools:
   - Application tab > Service Workers
   - Check "Offline" checkbox to simulate offline mode

### Updating the App

When you update files:
1. Update the `CACHE_NAME` version in `sw.js`
2. The service worker will automatically update on next visit
3. Users will get the new version after closing and reopening the app

## Deployment to GitHub Pages

1. Push the `iphone_6s` folder to your GitHub repository
2. Enable GitHub Pages in repository settings
3. Set source to `/root` or `/docs` depending on your setup
4. Access via: `https://[username].github.io/Mandalorian_Sound_Board/iphone_6s/`

**Note:** Make sure the `Sounds/` directory is included in the repository and accessible via the web server.

## License

This is a personal project for costume performances. Feel free to adapt for your own use!

---

**This is the Way.** 🪖

