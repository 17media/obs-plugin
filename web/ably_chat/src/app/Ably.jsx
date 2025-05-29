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
import { getGifts, getGiftByID } from './gifts';

import { 
    MsgType_COMMENT, 
    MsgType_NEW_GIFT, 
    MsgType_NEW_LUCKYBAG, 
    MsgType_JOIN_ROOM,
    MsgType_AI_COHOST_MESSAGE, 
} from '@/lib/constants';

import giftdata from './chat_new_gift_2.json';
import comment from './chat_message.json';
import newjoin from './chat_new_join.json';
import aicohost from './chat_ai_cohost.json';

const apiUrl = process.env.NEXT_PUBLIC_API_URL;
const jwtToken = process.env.NEXT_PUBLIC_JWT_TOKEN;

async function getAblyTokenFromServerByRoomID(roomID, jwtToken) {
    
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

async function getAblyTokenFromServer() {
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
            console.log(resBody);
            return resBody.token;
        } catch (err) {
            console.error("Failed to get Ably token:", err);
            throw err;
        }
    }
}

const prepareIndexedChat = (message) => {
    const id = shortid.generate();
    
    if (message.type === MsgType_NEW_GIFT 
        || message.type === MsgType_NEW_LUCKYBAG) {
        const { displayUser, barrage, ...restGift } = message?.giftMsg;
        const gift = getGiftByID(restGift.giftID);
        const indexedGift = fromJS({
           ...restGift,
           ...displayUser,
            barrage,
            id,
            messageType: message.type,
            gift,
        });
        return indexedGift; // Return the gift message
    } else if (message.type === MsgType_AI_COHOST_MESSAGE) {
        const { commentTxt } = message?.aiCohostMsg;
        const indexedChat = fromJS({
            content: commentTxt,
            comment: {
                textColor: "#333333",
            },
            displayName: "AI 助理",
            name: {
                textColor: "#527fff",
            },
            backgroundColor: "#FFFFFFE6",
            id,
            messageType: message.type,
        });
        return indexedChat; // Return the AI cohost message
    }

    const { displayUser, barrage, ...restChat } = message?.commentMsg;

    const indexedChat = fromJS({
        ...restChat,
        ...displayUser,
        barrage,
        id,
        messageType: message.type,
    });
    return indexedChat;
}
export default function AblyComponent() {
    const [chatList, setChatList] = useState([]);

    useEffect(() => {
        // 初次加载时获取礼物信息
        getGifts();
        setTimeout(() => {
            setChatList([
                // prepareIndexedChat(comment),
                // prepareIndexedChat(newjoin),
                // prepareIndexedChat(giftdata),
                prepareIndexedChat(aicohost),
            ]);
        }, 1000);
        
        const ably = new Ably.Realtime({
            environment: '17media',
            fallbackHosts: [
                '17-media-a-fallback.ably-realtime.com',
                '17-media-b-fallback.ably-realtime.com',
                '17-media-c-fallback.ably-realtime.com',
            ],

            authCallback: async (data, cb) => {
                const token = await getAblyTokenFromServer();
                cb(null, token);
            },
        })
        const channel = ably.channels.get(roomID);

        channel.subscribe((message) => {
            const decodeMessage = getAblyDecodeData(message);

            if (decodeMessage?.type === MsgType_COMMENT 
                || decodeMessage?.type === MsgType_JOIN_ROOM
            ) {
                const chat = decodeMessage?.commentMsg;
                // block rendering if is dirty word/user *and* not yourself
                if (
                    (!chat.isDirty && !chat.isDirtyWord && !chat.isDirtyUser) ||
                    (chat.displayUser.userID &&
                        chat.displayUser.userID === userID)
                ) {
                    const indexedChat = prepareIndexedChat(decodeMessage);
                    setChatList(prevChatList => [...prevChatList, indexedChat]);
                }
            } else if (decodeMessage?.type === MsgType_NEW_GIFT 
                || decodeMessage?.type === MsgType_NEW_LUCKYBAG
                || decodeMessage?.type === MsgType_AI_COHOST_MESSAGE
            ) { 
                const indexedChat = prepareIndexedGift(decodeMessage);
                setChatList(prevChatList => [...prevChatList, indexedChat]);
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
