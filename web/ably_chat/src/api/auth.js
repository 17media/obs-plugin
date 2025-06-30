const apiUrl = process.env.NEXT_PUBLIC_API_URL;
const jwtToken = process.env.NEXT_PUBLIC_JWT_TOKEN;
async function getAblyTokenFromServerByRoomID(roomID) {
    
  const url = `${apiUrl}/api/v1/messenger/token?type=3&roomID=${encodeURIComponent(roomID)}`;
  try {
      const res = await fetch(url, {
      method: "GET",
      headers: {
          "Authorization": 'Bearer ' + jwtToken,
      }
      });

      if (!res.ok) {
          throw new Error(`Invalid status code: ${res.status}`);
      }

      const resBody = await res.json();

      // Structure example: { provider: 3, token: "xxxx" }
      return resBody.token;
  } catch (err) {
      console.error("Failed to get Ably token:", err);
      throw err;
  }
}

export async function getAblyTokenFromServer(roomID = '') {
  if (process.env.NODE_ENV === 'development') {
      // In development, call getAblyTokenFromServerByRoomID
      // You might need to pass roomID and jwtToken if they are not globally available
      // or adjust how they are accessed within this function.
      // Assuming roomID and jwtToken are accessible here as defined in the file scope
      return await getAblyTokenFromServerByRoomID(roomID, jwtToken);
  } else {
      // In production, execute the original logic
      const url = `/lapi`;
      const data = {
          action: 'getAblyToken',
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
              throw new Error(`Invalid status code: ${res.status}`);
          }

          const resBody = await res.json();
        //   console.log(resBody);
          return resBody.token;
      } catch (err) {
          console.error("Failed to get Ably token:", err);
          throw err;
      }
  }
}
