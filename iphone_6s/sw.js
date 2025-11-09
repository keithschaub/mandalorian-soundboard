// Enhanced Service Worker for offline functionality - iPhone 6s version
const CACHE_NAME = 'mandalorian-soundboard-iphone-v1.14';
// Get the base path where the service worker is located
const BASE_PATH = self.location.pathname.replace('/sw.js', '');
const urlsToCache = [
  BASE_PATH + '/',
  BASE_PATH + '/index.html',
  BASE_PATH + '/manifest.json',
  // Cache all audio files
  BASE_PATH + '/Sounds/Mando Theme.mp3',
  BASE_PATH + '/Sounds/Mando Flute.mp3',
  BASE_PATH + '/Sounds/This is the way.mp3',
  BASE_PATH + "/Sounds/I'm a Mandalorian.mp3",
  BASE_PATH + '/Sounds/No droids.mp3',
  BASE_PATH + '/Sounds/Great.mp3',
  BASE_PATH + '/Sounds/Thank you.mp3',
  BASE_PATH + '/Sounds/Yes.mp3',
  BASE_PATH + "/Sounds/Don't touch anything.mp3",
  BASE_PATH + '/Sounds/Do not self destruct.mp3',
  BASE_PATH + '/Sounds/I could use a crew.mp3',
  BASE_PATH + "/Sounds/I don't belong here.mp3",
  BASE_PATH + '/Sounds/Where do you live.mp3',
  BASE_PATH + '/Sounds/Why so slow.mp3',
  BASE_PATH + '/Sounds/You want some soup.mp3',
  BASE_PATH + '/Sounds/Grogu Cooing.mp3',
  BASE_PATH + '/Sounds/Grogu giggling.mp3',
  BASE_PATH + '/Sounds/Grogu YES.mp3',
  BASE_PATH + '/Sounds/Grogu NO.mp3',
  BASE_PATH + '/Sounds/Grogu bad baby.mp3'
];

// Install event - cache all resources
self.addEventListener('install', (event) => {
  console.log('Service Worker: Installing...');
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => {
        console.log('Service Worker: Caching files');
        return cache.addAll(urlsToCache.map(url => new Request(url, {cache: 'reload'})));
      })
      .then(() => {
        console.log('Service Worker: All files cached');
        return self.skipWaiting(); // Activate immediately
      })
      .catch((error) => {
        console.error('Service Worker: Cache failed', error);
      })
  );
});

// Activate event - clean up old caches
self.addEventListener('activate', (event) => {
  console.log('Service Worker: Activating...');
  event.waitUntil(
    caches.keys().then((cacheNames) => {
      return Promise.all(
        cacheNames.map((cacheName) => {
          if (cacheName !== CACHE_NAME) {
            console.log('Service Worker: Deleting old cache', cacheName);
            return caches.delete(cacheName);
          }
        })
      );
    })
    .then(() => {
      console.log('Service Worker: Activated');
      return self.clients.claim(); // Take control of all pages immediately
    })
  );
});

// Fetch event - serve from cache, fallback to network
self.addEventListener('fetch', (event) => {
  // Skip cross-origin requests
  if (!event.request.url.startsWith(self.location.origin)) {
    return;
  }

  event.respondWith(
    caches.match(event.request)
      .then((response) => {
        // Return cached version if available
        if (response) {
          console.log('Service Worker: Serving from cache', event.request.url);
          return response;
        }

        // Otherwise fetch from network
        console.log('Service Worker: Fetching from network', event.request.url);
        return fetch(event.request)
          .then((response) => {
            // Don't cache if not a valid response
            if (!response || response.status !== 200 || response.type !== 'basic') {
              return response;
            }

            // Clone the response
            const responseToCache = response.clone();

            // Cache the fetched resource
            caches.open(CACHE_NAME)
              .then((cache) => {
                cache.put(event.request, responseToCache);
              });

            return response;
          })
          .catch((error) => {
            console.error('Service Worker: Fetch failed', error);
            // Return offline fallback if available
            if (event.request.destination === 'document') {
              return caches.match(BASE_PATH + '/index.html');
            }
          });
      })
  );
});

// Message event - handle messages from the app
self.addEventListener('message', (event) => {
  if (event.data && event.data.type === 'SKIP_WAITING') {
    self.skipWaiting();
  }
});

