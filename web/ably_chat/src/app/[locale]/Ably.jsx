'use client'

import React, { useState, useEffect, use } from 'react';
import { useTranslations } from 'next-intl';

import * as Ably from 'ably';
import shortid from 'shortid';
import { fromJS } from 'immutable';

import Chat from '@/lib/Chat';
import { getAblyDecodeData } from '@/util/getAblyDecodeData';
import { getChatProps } from '@/util/getChatProps';
import { ChatListWrapper } from '@/lib/ChatListWrapper';
import {
    getAblyTokenFromServer,
    getGifts,
    getGiftByID,
    getRoomInfo
} from '../../api';

import {
    MsgType_COMMENT,
    MsgType_NEW_GIFT,
    MsgType_NEW_LUCKYBAG,
    MsgType_JOIN_ROOM,
    MsgType_AI_COHOST_MESSAGE,
    DEFAULT_STREAMER_COMMENT_BG_COLOR_1,
} from '@/lib/constants';

// import giftdata from './chat_new_gift_2.json';
// import comment from './chat_message.json';
// import newjoin from './chat_new_join.json';
// import aicohost from './chat_ai_cohost.json';

export default function AblyComponent() {

    const [chatList, setChatList] = useState([]);

    const [roomID, setRoomID] = useState('');
    const [userID, setUserID] = useState('');

    const [roomInfo, setRoomInfo] = useState(null);

    const t = useTranslations('ChatPage');

    // 保存对话到本地存储
    const saveChatToStorage = (roomId, chatData) => {
        try {
            const storageKey = `chat_history_${roomId}`;
            // 将Immutable对象转换为普通JavaScript对象进行存储
            const plainChats = chatData.map(chat => chat.toJS ? chat.toJS() : chat);
            const chatHistory = {
                roomId,
                timestamp: Date.now(),
                chats: plainChats
            };
            localStorage.setItem(storageKey, JSON.stringify(chatHistory));
        } catch (error) {
            console.error('Error saving chat to storage:', error);
        }
    };

    // 从本地存储加载对话
    const loadChatFromStorage = (roomId) => {
        try {
            const storageKey = `chat_history_${roomId}`;
            const savedData = localStorage.getItem(storageKey);
            if (savedData) {
                const chatHistory = JSON.parse(savedData);
                // 检查数据是否过期（可选：设置24小时过期）
                const isExpired = Date.now() - chatHistory.timestamp > 24 * 60 * 60 * 1000;
                if (!isExpired && chatHistory.chats) {
                    return chatHistory.chats.map(chat => fromJS(chat));
                }
            }
        } catch (error) {
            console.error('Error loading chat from storage:', error);
        }
        return [];
    };

    // 清理过期的聊天记录
    const cleanupExpiredChats = () => {
        try {
            const keys = Object.keys(localStorage);
            keys.forEach(key => {
                if (key.startsWith('chat_history_')) {
                    const savedData = localStorage.getItem(key);
                    if (savedData) {
                        const chatHistory = JSON.parse(savedData);
                        const isExpired = Date.now() - chatHistory.timestamp > 24 * 60 * 60 * 1000;
                        if (isExpired) {
                            localStorage.removeItem(key);
                        }
                    }
                }
            });
        } catch (error) {
            console.error('Error cleaning up expired chats:', error);
        }
    };

    const prepareIndexedChat = (message) => {
        const id = shortid.generate();
        const { userInfo } = roomInfo;
        const streamerInfo = userInfo;

        if (message.type === MsgType_NEW_GIFT
            || message.type === MsgType_NEW_LUCKYBAG) {
            const { displayUser, barrage, ...restGift } = message?.giftMsg;
            const gift = getGiftByID(restGift.giftID);

            if (message.type === MsgType_NEW_LUCKYBAG && restGift.extID) {
                const luckyBag = getGiftByID(restGift.extID);
                const indexedGift = fromJS({
                    ...restGift,
                    ...displayUser,
                    barrage,
                    id,
                    messageType: message.type,
                    gift,
                    luckyBag,
                    streamerInfo,
                });
                return indexedGift;
            }


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
                displayName: t('AI_COHOST'),
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
        const { isStreamer } = displayUser;
        let restChat1 = restChat;
        if (isStreamer) {
            restChat1 = {
                ...restChat,
                backgroundColor: DEFAULT_STREAMER_COMMENT_BG_COLOR_1,
            }
        }

        const indexedChat = fromJS({
            ...restChat1,
            ...displayUser,
            barrage,
            id,
            messageType: message.type,
            streamerInfo,
        });
        return indexedChat;
    }

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

                // 清理过期的聊天记录
                cleanupExpiredChats();
            } catch (error) {
                console.error("Error fetching initial data:", error);
            }
        };
        fetchInitialData();
    }, []);

    // 当roomID变化时加载历史对话
    useEffect(() => {
        if (roomID) {
            const savedChats = loadChatFromStorage(roomID);
            console.log('savedChats', savedChats);
            setChatList(savedChats);
        }
    }, [roomID]);

    // 当对话列表更新时保存到本地存储
    useEffect(() => {
        if (roomID && chatList.length > 0) {
            saveChatToStorage(roomID, chatList);
        }
    }, [chatList, roomID]);

    useEffect(() => {
        if (!roomID || !userID) {
            return;
        }

        // setTimeout(() => {
        //     setChatList([
        //         prepareIndexedChat(comment),
        //         prepareIndexedChat(newjoin),
        //         prepareIndexedChat(giftdata),
        //         prepareIndexedChat(aicohost),
        //     ]);
        // }, 1000);

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
                    setChatList(prevChatList => {
                        const newChatList = [...prevChatList, indexedChat];
                        // 限制聊天记录数量，避免内存过多占用
                        return newChatList.length > 1000 ? newChatList.slice(-1000) : newChatList;
                    });
                }
            } else if (decodeMessage?.type === MsgType_NEW_GIFT
                || decodeMessage?.type === MsgType_NEW_LUCKYBAG
                || decodeMessage?.type === MsgType_AI_COHOST_MESSAGE
            ) {
                const indexedChat = prepareIndexedChat(decodeMessage);
                setChatList(prevChatList => {
                    const newChatList = [...prevChatList, indexedChat];
                    // 限制聊天记录数量，避免内存过多占用
                    return newChatList.length > 1000 ? newChatList.slice(-1000) : newChatList;
                });
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
