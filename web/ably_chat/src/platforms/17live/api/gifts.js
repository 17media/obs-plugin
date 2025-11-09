// Used to store gift information
let giftsMap = new Map();

export async function getGifts() {
    if (process.env.NODE_ENV === 'development') {
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

export function getGiftByID(giftID) {
    // Get gift information by giftID
    // Assuming giftsMap is a Map where key is giftID and value is gift information
    return giftsMap.get(giftID);
}
