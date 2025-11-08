# App Icons Required

This PWA requires icon files for proper installation on iPhone. You need to create two PNG icon files:

## Required Icons

1. **icon-192.png** - 192x192 pixels
2. **icon-512.png** - 512x512 pixels

## Creating Icons

### Option 1: Use Online Icon Generator
1. Visit https://realfavicongenerator.net/ or https://www.pwabuilder.com/imageGenerator
2. Upload a 512x512 image (or create one with a Mandalorian helmet emoji 🪖)
3. Download the generated icons
4. Save as `icon-192.png` and `icon-512.png` in this directory

### Option 2: Create Manually
1. Create a 512x512 pixel image with:
   - Background: #1A1A1A (dark gray)
   - Icon: Mandalorian helmet emoji 🪖 or custom design
   - Text: "MANDO" or "MANDALORIAN"
2. Save as `icon-512.png`
3. Resize to 192x192 and save as `icon-192.png`

### Option 3: Use Image Editing Software
- Photoshop, GIMP, or any image editor
- Create square images with transparent or solid backgrounds
- Export as PNG format

## Icon Design Suggestions

- **Background**: Dark (#1A1A1A) to match app theme
- **Foreground**: Blue (#4A90E2) to match accent color
- **Icon**: Mandalorian helmet emoji (🪖) or custom helmet design
- **Style**: Simple, recognizable at small sizes

## Testing Icons

After creating icons:
1. Test locally by serving the app
2. Check that icons appear in browser tab
3. When installing to home screen, verify icon appears correctly

## Note

The manifest.json references these icon files. Make sure they exist before deploying to GitHub Pages.

