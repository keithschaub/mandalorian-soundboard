# Deployment Guide - iPhone 6s Soundboard

This guide explains how to deploy the iPhone 6s soundboard to GitHub Pages so it can be accessed and installed on your iPhone.

## Prerequisites

1. GitHub account
2. Git installed on your computer
3. Repository already created on GitHub

## Step 1: Prepare Files

Before deploying, make sure you have:

- ✅ All files in `iphone_6s/` directory
- ✅ `Sounds/` directory with all 20 MP3 files
- ✅ Icon files (`icon-192.png` and `icon-512.png`) - see ICONS_README.md
- ✅ All files committed to git

## Step 2: Push to GitHub

```bash
# Navigate to project root
cd Mandalorian_Sound_Board

# Add all files
git add iphone_6s/
git add Sounds/  # If Sounds directory is in root

# Commit changes
git commit -m "Add iPhone 6s offline PWA version"

# Push to GitHub
git push origin main
```

## Step 3: Enable GitHub Pages

1. Go to your GitHub repository
2. Click **Settings** tab
3. Scroll down to **Pages** section (left sidebar)
4. Under **Source**, select:
   - **Branch**: `main` (or `master`)
   - **Folder**: `/ (root)` or `/docs` depending on your setup
5. Click **Save**

## Step 4: Verify Deployment

1. Wait a few minutes for GitHub Pages to build
2. Visit: `https://[your-username].github.io/Mandalorian_Sound_Board/iphone_6s/`
3. Check that:
   - Page loads correctly
   - All sounds are accessible
   - Service Worker registers (check browser console)

## Step 5: Test on iPhone

1. Connect iPhone 6s to WiFi
2. Open Safari browser
3. Navigate to: `https://[your-username].github.io/Mandalorian_Sound_Board/iphone_6s/`
4. Wait for all files to load
5. Tap Share button → "Add to Home Screen"
6. Test offline functionality by turning off WiFi

## Troubleshooting

### Files Not Loading
- Check that all files are committed and pushed
- Verify file paths are correct (case-sensitive on GitHub)
- Check GitHub Pages build status in repository Settings

### Service Worker Not Registering
- Ensure `sw.js` is in the `iphone_6s/` directory
- Check browser console for errors
- Verify HTTPS is enabled (required for Service Workers)

### Sounds Not Playing Offline
- Make sure you accessed the app via WiFi first
- Check that Service Worker cached all files
- Verify all MP3 files are in `Sounds/` directory

### Icons Not Showing
- Ensure `icon-192.png` and `icon-512.png` exist
- Check file paths in `manifest.json`
- Clear browser cache and try again

## File Structure for GitHub Pages

```
Mandalorian_Sound_Board/
├── iphone_6s/
│   ├── index.html
│   ├── sw.js
│   ├── manifest.json
│   ├── icon-192.png
│   ├── icon-512.png
│   ├── README.md
│   └── Sounds/
│       ├── Mando Theme.mp3
│       ├── Mando Flute.mp3
│       └── ... (18 more sounds)
└── Sounds/  (if shared with other versions)
```

## Custom Domain (Optional)

If you have a custom domain:
1. Add CNAME file to repository root
2. Configure DNS settings
3. Update GitHub Pages settings

## Updating the App

When you make changes:
1. Update files locally
2. Commit and push to GitHub
3. Update `CACHE_NAME` version in `sw.js` to force cache refresh
4. Users will get updates after closing and reopening the app

---

**Note**: GitHub Pages may take a few minutes to update after pushing changes.

