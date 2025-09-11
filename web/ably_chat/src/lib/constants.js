export const LEVEL_COLORS = {
    LEVEL_00: '#71CB40',
    LEVEL_01: '#71CB40',
    LEVEL_02: '#33CD88',
    LEVEL_03: '#33CEB0',
    LEVEL_04: '#36DBDA',
    LEVEL_05: '#67C6FA',
    LEVEL_06: '#769AE9',
    LEVEL_07: '#9E7BFF',
    LEVEL_08: '#C368F3',
    LEVEL_09: '#FF68F9',
    LEVEL_10: '#F750BC',
    LEVEL_11: 'linear-gradient(to right, #F750BC, #769AE9)',
    LEVEL_12: 'linear-gradient(to right, #F750BC, #9E7BFF)',
    LEVEL_13: 'linear-gradient(to right, #F750BC, #C368F3)',
    LEVEL_14: 'linear-gradient(to right, #F750BC, #FF68F9)',
    LEVEL_15: 'linear-gradient(to right, #36DBDA, #F750BC)',
    LEVEL_16: 'linear-gradient(to right, #36DBDA, #769AE9)',
    LEVEL_17: 'linear-gradient(to right, #36DBDA, #9E7BFF)',
    LEVEL_18: 'linear-gradient(to right, #36DBDA, #C368F3)',
    LEVEL_19: 'linear-gradient(to right, #36DBDA, #FF68F9)',
    LEVEL_20: 'linear-gradient(to right, #FFADAD, #F32FE5)',
    LEVEL_21: 'linear-gradient(to right, #FFF213, #B183FF)',
    LEVEL_22: 'linear-gradient(to right, #FFF213, #F17EFB)',
    LEVEL_23: 'linear-gradient(to right, #FFF213, #EE7171)',
    LEVEL_24: 'linear-gradient(to right, #FED549, #7BFF1A)',
    LEVEL_25: 'linear-gradient(to right, #FFF833, #FF876B 52%, #D071FF)',
    LEVEL_26: 'linear-gradient(to right, #FF803D, #FFC2A1 49%, #A93AFF)',
    LEVEL_27: 'linear-gradient(to right, #D8AD2E, #81E7D1 46%, #774DE8)',
    LEVEL_28: 'linear-gradient(to right, #C7CD4D, #BE78FF 47%, #761AC9)',
};

export const TEXT_COLORS = {
    LEVEL_11: '#B775D3',
    LEVEL_12: '#CD64DC',
    LEVEL_13: '#DE5BD5',
    LEVEL_14: '#FB5CD9',
    LEVEL_15: '#909ACC',
    LEVEL_16: '#55BCE1',
    LEVEL_17: '#65B0EB',
    LEVEL_18: '#79A4E6',
    LEVEL_19: '#99A3E9',
    LEVEL_20: '#FA7AC3',
    LEVEL_21: '#C49FC2',
    LEVEL_22: '#F8BC7F',
    LEVEL_23: '#F7B143',
    LEVEL_24: '#CBF817',
    LEVEL_25: '#FF886A',
    LEVEL_26: '#DD8CC6',
    LEVEL_27: '#9FB7ED',
    LEVEL_28: '#B970FB',
};

export const MAX_LEVEL = Object.keys(LEVEL_COLORS).length; // level 280

export const CDN_URL = 'https://cdn.17app.co';

export const GCP_BASE_HOST = 'webcdn.17app.co';
export const GCP_BASE_URL = `https://${GCP_BASE_HOST}`;
export const GCP_URL = `${GCP_BASE_URL}/17live`;

export const DEFAULT_AVATAR_IMAGE =
    'images/ig_default_profile_image.svg';
export const DEFAULT_EMPTY_IMAGE = 'images/ig_empty_image.svg';
export const THUMBNAIL_PREFIX = 'THUMBNAIL_';

/**
 * status
 */
export const STATUS = {
    IDLE: 'IDLE',
    LOADING: 'LOADING',
    COMPLETED: 'COMPLETED',
    ERROR: 'ERROR',
};

export const HTTP_PREFIX = 'http://';
export const HTTPS_PREFIX = 'https://';

export const IncentiveSuperfanLevelBadgeType = {
    SUPERFAN:  1,
    USER: 2,
}

export const COMMENT_BORDER_PADDING_CANDY_CANE = 4;
export const COMMENT_BORDER_PAINT_INTERVAL_TIME = 50; // ms

export const BorderType = {
    CANDY_CANE: 1,
    METAL: 2,
    GRADIENT: 3,
}

export const PATTERN_IMG_RESIZE = 36;
export const COMMENT_BORDER_RADIUS_CANDY_CANE = 8;

export const IncentiveSuperfanLevelBadgeSize = {
    NORMAL: 'normal',
    SMALL: 'small',
}

export const IncentiveSuperfanLevel = {
    LEVEL0: 0,
    LEVEL1: 1,
    LEVEL2: 2,
    LEVEL3: 3,
    LEVEL4: 4,
    LEVEL5: 5,
}

export const MAP_INCENTIVE_SUPERFAN_BADGE_COLOR = {
    [IncentiveSuperfanLevel.LEVEL0]: '#A6A6A6',
    [IncentiveSuperfanLevel.LEVEL1]: '#F16C0C',
    [IncentiveSuperfanLevel.LEVEL2]: '#8FBDCC',
    [IncentiveSuperfanLevel.LEVEL3]: '#FFBB0B',
    [IncentiveSuperfanLevel.LEVEL4]: '#AA80FF',
    [IncentiveSuperfanLevel.LEVEL5]: '#FF737E',
};

export const LevelBadgeSize = {
    NORMAL: 'Normal',
    SMALL: 'Small',
}

export const LevelBadgeSizeToSuperFanBadgeSize = {
    [LevelBadgeSize.NORMAL]: IncentiveSuperfanLevelBadgeSize.NORMAL,
    [LevelBadgeSize.SMALL]: IncentiveSuperfanLevelBadgeSize.SMALL,
};

/**
 * styles
 */
export const BD_WHITE = '#ffffff'
export const STREAMER_COLOR = '#33ceb0';
export const VIP_COLOR = '#ff15d3';
export const GUARDIAN_COLOR = '#33cccc';
export const SYSTEM_COLOR = '#6f6777';

export const COLOR_PRIMARY_WHITE = '#ffffff';
export const COLOR_BLACK = '#000000';

export const VIP_BANNER_BACKGROUND = VIP_COLOR;
export const GUARDIAN_BANNER_BACKGROUND = '#42f1f6';

// Pubnub provides #5928232D, FE will use transfer8bitHexCode to convert
export const DEFAULT_COMMENT_BG_COLOR = '#28232D59';
export const DEFAULT_STREAMER_COMMENT_BG_COLOR = '#00A38EB3';
export const DEFAULT_GUARDIAN_COMMENT_BG_COLOR = '#1DCBE4B3';

export const DEFAULT_STREAMER_COMMENT_BG_COLOR_1 = '#33CEB0B3'; // hardcode green transparent background
export const DEFAULT_GUARDIAN_COMMENT_BG_COLOR_1 = '#42F1F6CC'; // hardcode blue transparent background
export const DEFAULT_VIP_COMMENT_BG_COLOR_1 = '#FF15D3B3'; // hardcode pink transparent background

/**
 * user types
 */
export const USER_VIP = 'USER_VIP';
export const USER_STREAMER = 'USER_STREAMER';
export const USER_GUARDIAN = 'USER_GUARDIAN';
export const USER_NORMAL = 'USER_NORMAL';
export const USER_SYSTEM = 'USER_SYSTEM';
export const USER_AI = 'USER_AI';

export const REACTION_TYPE = 'REACTION';
export const ReactionTypes = {
    SHARE_FB: 0,
    SHARE_17: 1,
    LIKE: 2,
    FOLLOW: 3,
    SHARE_TWITTER: 4,
    SHARE_LINE: 5,
    SHARE_WHATSAPP: 6,
    SHARE_WECHAT: 7,
    SHARE_MESSENGER: 8,
    SHARE_COPY_LINK: 12,
}

export const CommentShadowType = {
    NONE: 'none',
    LIGHT: 'light',
    DARK: 'dark',
}

export const mapCheckingLevelImage = {
    1: '',
    2: 'ig-bg-checking-green.png',
    3: 'ig-bg-checking-blue.png',
    4: 'ig-bg-checking-purple.png',
    5: 'ig-bg-checking-orange.png',
    6: 'ig-bg-checking-silver.png',
    7: 'ig-bg-checking-golden.png',
    8: 'ig-bg-checking-black.png',
};

export const MsgType_COMMENT = 3; // General comment message
export const MsgType_NEW_GIFT =13; // Gift animation message
export const MsgType_JOIN_ROOM = 18; // Audience join room message
export const MsgType_NEW_LUCKYBAG = 32; // Random gift message
export const MsgType_AI_COHOST_MESSAGE = 120; // AI co-host message
export const MsgType_POKE = 47; // Poke message
