'use client'

import React, { useState, useEffect, useRef, use } from 'react';
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
    const chatEndRef = useRef(null);

    // Add window width state
    const [windowWidth, setWindowWidth] = useState(typeof window !== 'undefined' ? window.innerWidth : 1200);

    const t = useTranslations('ChatPage');

    // Listen for window resize events
    useEffect(() => {
        const handleResize = () => {
            setWindowWidth(window.innerWidth);
        };

        window.addEventListener('resize', handleResize);
        return () => window.removeEventListener('resize', handleResize);
    }, []);

    // Calculate responsive asideLiveWidth
    const getAsideLiveWidth = () => {
        // Calculate based on window width, minimum 378, maximum 600
        return Math.max(378, windowWidth);
    };

    // Save chat to local storage
    const saveChatToStorage = (roomId, chatData) => {
        try {
            const storageKey = `chat_history_${roomId}`;
            // Convert Immutable object to plain JavaScript object for storage
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

    // load chat from local storage
    const loadChatFromStorage = (roomId) => {
        try {
            const storageKey = `chat_history_${roomId}`;
            const savedData = localStorage.getItem(storageKey);
            if (savedData) {
                const chatHistory = JSON.parse(savedData);
                // check if the data is expired
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

    // clear chat history
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

                // load gifts
                await getGifts();

                // load chat history from local storage
                cleanupExpiredChats();
            } catch (error) {
                console.error("Error fetching initial data:", error);
            }
        };
        fetchInitialData();
    }, []);

    // load chat history from local storage when roomID changes
    useEffect(() => {
        if (roomID) {
            const savedChats = loadChatFromStorage(roomID);
            console.log('savedChats', savedChats);
            setChatList(savedChats);
        }
    }, [roomID]);

    // save chat to local storage
    useEffect(() => {
        if (roomID && chatList.length > 0) {
            saveChatToStorage(roomID, chatList);
        }
    }, [chatList, roomID]);

    // 記錄用戶是否在底部的狀態
    const [isUserNearBottom, setIsUserNearBottom] = useState(true);
    
    // 監聽滾動事件，檢測用戶是否在底部附近
    useEffect(() => {
        const chatContainer = document.querySelector('.chat-list-wrapper');
        if (!chatContainer) return;
        
        const handleScroll = () => {
            // 計算用戶是否已經接近底部（距離底部小於100px）
            const isNearBottom = chatContainer.scrollHeight - chatContainer.scrollTop - chatContainer.clientHeight < 100;
            setIsUserNearBottom(isNearBottom);
        };
        
        // 初始檢查
        handleScroll();
        
        // 添加滾動事件監聽
        chatContainer.addEventListener('scroll', handleScroll);
        
        // 清理函數
        return () => {
            chatContainer.removeEventListener('scroll', handleScroll);
        };
    }, []);
    
    // 僅當用戶在底部附近且聊天記錄更新時，才自動滾動到底部
    useEffect(() => {
        // 確保只有當聊天列表有內容且用戶在底部附近時才滾動
        if (chatEndRef.current && isUserNearBottom && chatList.length > 0) {
            // 使用 smooth 行為確保滾動平滑，且只滾動到底部
            chatEndRef.current.scrollIntoView({ behavior: 'smooth', block: 'end' });
        }
    }, [chatList, isUserNearBottom]);

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
                        // only keep the last 1000 chat messages
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
                    // only keep the last 1000 chat messages
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
        <ChatListWrapper className="chat-list-wrapper">
            {chatList
                .map(chat => (
                    <Chat
                        asideLiveWidth={getAsideLiveWidth()}
                        {...getChatProps(chat)}
                        roomID={roomID}
                        isConcert={false}
                        isGroupCall={false}
                    />
                ))}
            <div ref={chatEndRef} />
        </ChatListWrapper>
    );
}
