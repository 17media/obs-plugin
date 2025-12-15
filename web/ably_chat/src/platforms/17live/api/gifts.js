// Used to store gift information
let giftsMap = new Map();

async function loadMockGifts() {
    try {
        // In development environment, read gift information from local JSON file
        const response = await fetch('/mock/get_gifts_response.json');
        if (!response.ok) {
            throw new Error(`Failed to fetch gifts: ${response.status}`);
        }
        const giftsData = await response.json();
        if (giftsData && giftsData.gifts) {
            giftsData.gifts.forEach(gift => {
                giftsMap.set(gift.giftID, gift);
            });
            // console.log('Gifts loaded from local JSON:', giftsMap.size);
        } else {
            console.error('Invalid gifts data structure in local JSON');
        }
    } catch (error) {
        console.error('Error loading gifts from local JSON:', error);
    }
}

if (process.env.NODE_ENV === 'development') {
    loadMockGifts();
}

export async function getGifts() {
    if (process.env.NODE_ENV === 'development') {
        await loadMockGifts();
    } else {
        const url = `/lapi`;
        const data = {
            action: 'getGifts',
        }
        try {
            const res = await fetch(url, {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                },
                body: JSON.stringify(data)
            });

            if (!res.ok) {
                throw new Error(`Failed to fetch gifts from server: ${res.status}`);
            }

            const giftsData = await res.json();
            if (giftsData && giftsData.gifts) {
                giftsData.gifts.forEach(gift => {
                    giftsMap.set(gift.giftID, gift);
                });
                console.log('Gifts loaded from server:', giftsMap.size);
            } else {
                console.error('Invalid gifts data structure from server');
            }
        } catch (err) {
            console.error('Error loading gifts from server:', err);
            throw err;
        }
    }
}

export async function getGiftByID(giftID) {
    if (!giftID) return null;

    // Check if gift information is already loaded
    if (giftsMap.has(giftID)) {
        // console.log('Gift already loaded:', giftID);
        return giftsMap.get(giftID);
    }
    
    // If not development environment, try to fetch from server
    if (process.env.NODE_ENV !== 'development') {
        const url = `/lapi`;
        const data = {
            action: 'getGift',
            giftID: giftID
        }
        try {
            const res = await fetch(url, {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                },
                body: JSON.stringify(data)
            });

            if (!res.ok) {
                console.warn(`Failed to fetch gift ${giftID}: ${res.status}`);
                return null;
            }

            const giftData = await res.json();
            if (giftData && giftData.giftID) {
                giftsMap.set(giftData.giftID, giftData);
                console.log('Gift loaded from server:', giftData.giftID);
                return giftData;
            } else {
                console.warn('Invalid gift data structure from server:', giftData);
                return null;
            }
        } catch (err) {
            console.error('Error loading gift from server:', err);
            return null;
        }
    }
    
    return null;
}
