'use client'

import React, { useState, useEffect } from 'react';

import * as Ably from 'ably';
import shortid from 'shortid';
import { fromJS } from 'immutable';

import Chat from '@/lib/Chat';
import { getAblyDecodeData } from '@/util/getAblyDecodeData';
import { getChatProps } from '@/util/getChatProps';
import { ChatListWrapper } from '@/lib/ChatListWrapper';
import { getAblyTokenFromServer,
    getGifts,
    getGiftByID,
    getRoomInfo
} from '../api';

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

const prepareIndexedChat = (message, streamerInfo = null) => {
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
            streamerInfo,
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
            streamerInfo,
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
        streamerInfo,
    });
    return indexedChat;
}
export default function AblyComponent() {
    const [chatList, setChatList] = useState([]);

    const [roomID, setRoomID] = useState('');
    const [userID, setUserID] = useState('');

    const [roomInfo, setRoomInfo] = useState(null);

    useEffect(() => {
        const fetchInitialData = async () => {
            const urlParams = new URLSearchParams(window.location.search);
            const roomIDFromUrl = urlParams.get('roomID');
            const userIDFromUrl = urlParams.get('userID');
            if (roomIDFromUrl) {
                setRoomID(roomIDFromUrl);
            }
            if (userIDFromUrl) {
                setUserID(userIDFromUrl);
            }

            try {
                const roomInfo = await getRoomInfo();
                setRoomInfo(roomInfo);

                // 初次加载时获取礼物信息
                await getGifts();
            } catch (error) {
                console.error("Error fetching initial data:", error);
            }
        };
        fetchInitialData();
    }, []);
        
    useEffect(() => {
        if (!roomID || !userID) {
            return;
        }

        setTimeout(() => {
            setChatList([
                prepareIndexedChat(comment),
                prepareIndexedChat(newjoin),
                prepareIndexedChat(giftdata),
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
                const token = await getAblyTokenFromServer(roomID);
                cb(null, token);
            },
        })
        const channel = ably.channels.get(roomID);

        channel.subscribe((message) => {
            const decodeMessage = getAblyDecodeData(message);
            const streamerInfo = roomInfo.userInfo;

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
                    const indexedChat = prepareIndexedChat(decodeMessage, streamerInfo);
                    setChatList(prevChatList => [...prevChatList, indexedChat]);
                }
            } else if (decodeMessage?.type === MsgType_NEW_GIFT 
                || decodeMessage?.type === MsgType_NEW_LUCKYBAG
                || decodeMessage?.type === MsgType_AI_COHOST_MESSAGE
            ) { 
                const indexedChat = prepareIndexedChat(decodeMessage, streamerInfo);
                setChatList(prevChatList => [...prevChatList, indexedChat]);
            }
        });

        // Cleanup on unmount
        return () => {
            channel.unsubscribe();
        };
    }, [roomID, userID, roomInfo]);

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
