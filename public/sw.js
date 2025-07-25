const CACHE_NAME = 'tandon-air-cache-v1';
const urlsToCache = [
    '/',
'/index.html',
// Tambahkan path ke file CSS dan JS jika Anda memisahkannya
'https://cdn.tailwindcss.com',
'https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css'
];

self.addEventListener('install', event => {
    event.waitUntil(
        caches.open(CACHE_NAME)
        .then(cache => {
            console.log('Opened cache');
            return cache.addAll(urlsToCache);
        })
    );
});

self.addEventListener('fetch', event => {
    event.respondWith(
        caches.match(event.request)
        .then(response => {
            // Cache hit - return response
            if (response) {
                return response;
            }
            return fetch(event.request);
        }
        )
    );
});
