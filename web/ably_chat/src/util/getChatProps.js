/**
 * hex 為 8 碼時，api response 前兩碼為透明度，需調整順序讓 web style 正常
 */
export const transfer8bitHexCode = hexCode => {
    if (!hexCode) {
        return;
    }
    const hex = hexCode.replace('#', '');
    return hex.length === 6
        ? `#${hex}`
        : `#${hex.substring(2, 8)}${hex.substring(0, 2)}`;
};

export const getChatProps = chat => {
    return {
        key: chat.get('id'),
        id: chat.get('id'),
        messageType: chat.get('messageType'),
        displayName: chat.get('displayName'),
        openID: chat.get('openID'),
        userID: chat.get('userID'),
        content: chat.get('content'),
        level: chat.get('level'),
        levelBadges: chat.get('levelBadges'),
        isGuardian: chat.get('isGuardian'),
        isVIP: chat.get('isVIP'),
        isSendAll: chat.get('isSendAll'),
        type: chat.get('type'),
        checkingLevel: chat.get('checkinLevel'),
        gift: chat.get('gift'),
        luckyBag: chat.get('luckyBag'),
        pokeInfo: chat.get('pokeInfo'),
        giftType: chat.get('giftType'),
        giftMetas: chat.get('giftMetas'),
        receiver: chat.get('receiver'),
        value: chat.get('value'),
        armyGiftFromPubnub: chat.get('armyGiftFromPubnub'),
        armySubscription: chat.get('armySubscription'),
        isInvisible: chat.get('isInvisible'),
        snacks: chat.get('snacks'),
        border: chat.get('border'),
        nameColor: transfer8bitHexCode(chat.getIn(['name', 'textColor'])),
        textColor: transfer8bitHexCode(chat.getIn(['comment', 'textColor'])),
        commentShadowColor: chat.get('commentShadowColor'),
        backgroundColor: transfer8bitHexCode(chat.get('backgroundColor')),
        prefixBadges: chat.get('prefixBadges'),
        middleBadge: chat.getIn(['middleBadge', 'URL']),
        topRightBadge: chat.getIn(['topRightBadge', 'URL']),
    };
};