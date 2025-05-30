// 用於存儲禮物信息
let giftsMap = new Map();

export async function getGifts() {
    if (process.env.NODE_ENV === 'development') {
        try {
            // 在开发环境中，从本地 JSON 文件读取礼物信息
            const response = await fetch('/get_gifts_response.json');
            if (!response.ok) {
                throw new Error(`Failed to fetch gifts: ${response.status}`);
            }
            const giftsData = await response.json();
            if (giftsData && giftsData.gifts) {
                giftsData.gifts.forEach(gift => {
                    giftsMap.set(gift.giftID, gift);
                });
                console.log('Gifts loaded from local JSON:', giftsMap.size);
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
    // 根据 giftID 获取礼物信息
    // 假设 giftsMap 是一个 Map，其中 key 是 giftID，value 是礼物信息
    return giftsMap.get(giftID);
}
