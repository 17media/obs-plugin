export async function getRoomInfo() {
    if (process.env.NODE_ENV === 'development') {
        try {
            // In development environment, read room information from local JSON file
            const response = await fetch('/get_room_info_response.json');
            if (!response.ok) {
                throw new Error(`Failed to fetch room info: ${response.status}`);
            }
            const roomInfo = await response.json();
            return roomInfo;
        } catch (error) {
            console.error('Error loading roomInfo from local JSON:', error);
            throw error;
        }
    } else {
        const url = `/lapi`;
        const data = {
            action: 'getRoomInfo',
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
                throw new Error(`Failed to fetch roominfo from server: ${res.status}`);
            }

            const roomInfo = await res.json();
            return roomInfo;
        } catch (err) {
            console.error('Error loading roomInfo from server:', err);
            throw err;
        }
    }
}
