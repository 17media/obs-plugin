'use client'

import React, { useState, useEffect } from 'react';

import * as Ably from 'ably';
import shortid from 'shortid';
import { fromJS } from 'immutable';

import Chat from '@/lib/Chat';
import { getAblyDecodeData } from '@/util/getAblyDecodeData';
import { getChatProps } from '@/util/getChatProps';
import { ChatListWrapper } from '@/lib/ChatListWrapper';
import { roomID, userID } from './config';

const apiUrl = process.env.NEXT_PUBLIC_API_URL;
const jwtToken = process.env.NEXT_PUBLIC_JWT_TOKEN;

const MsgType_COMMENT = 3; // 一般留言訊息

async function getAblyTokenFromServer(roomID, jwtToken) {
    
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

        // 结构示例：{ provider: 3, token: "xxxx" }
        return resBody.token;
    } catch (err) {
        console.error("Failed to get Ably token:", err);
        throw err;
    }
}
export default function AblyComponent() {
    const [chatList, setChatList] = useState([]);

    useEffect(() => {
        // const ably = new Ably.Realtime(options);
        const ably = new Ably.Realtime({
            environment: '17media',
            fallbackHosts: [
                '17-media-a-fallback.ably-realtime.com',
                '17-media-b-fallback.ably-realtime.com',
                '17-media-c-fallback.ably-realtime.com',
            ],

            authCallback: async (data, cb) => {
                const token = await getAblyTokenFromServer(roomID, jwtToken);
                // const token = await getAblyToken();
                cb(null, token);
            },
        })
        const channel = ably.channels.get(roomID);

        channel.subscribe((message) => {
            const decodeMessage = getAblyDecodeData(message);

            if (decodeMessage?.type === MsgType_COMMENT) {
                const chat = decodeMessage?.commentMsg;

                // block rendering if is dirty word/user *and* not yourself
                if (
                    (!chat.isDirty && !chat.isDirtyWord && !chat.isDirtyUser) ||
                    (chat.displayUser.userID &&
                        chat.displayUser.userID === userID)
                ) {
                    const id = shortid.generate();
                    const { displayUser, barrage, ...restChat } = chat;

                    const indexedChat = fromJS({
                        ...restChat,
                        ...displayUser,
                        barrage,
                        id,
                    });

                    setChatList(prevChatList => [...prevChatList, indexedChat]);
                }
            }


        });

        // Cleanup on unmount
        return () => {
            channel.unsubscribe();
        };
    }, []);

    return (
        <ChatListWrapper>
            {chatList
                .map(chat => (
                    <Chat
                        asideLiveWidth={378}
                        {...getChatProps(chat)}
                        roomID={roomID}
                        isConcert={false}
                        isGroupCall={false}
                    />
                ))}
        </ChatListWrapper>
    );
}
