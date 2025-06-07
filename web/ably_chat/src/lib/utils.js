import padStart from 'lodash/padStart';
import isArray from 'lodash/isArray';
import range from 'lodash/range';
import { rgba } from 'polished';

import {
    CDN_URL,
    DEFAULT_AVATAR_IMAGE,
    DEFAULT_EMPTY_IMAGE,
    GCP_URL,
    HTTP_PREFIX,
    HTTPS_PREFIX,
    LEVEL_COLORS,
    MAX_LEVEL,
    TEXT_COLORS,
    THUMBNAIL_PREFIX,
    COMMENT_BORDER_PAINT_INTERVAL_TIME,
    USER_VIP,
    USER_GUARDIAN,
    USER_STREAMER,
    USER_AI,
    VIP_BANNER_BACKGROUND,
    GUARDIAN_BANNER_BACKGROUND,
    STREAMER_COLOR,
    BD_WHITE,
    ReactionTypes,
    USER_SYSTEM,
    USER_NORMAL,
    SYSTEM_COLOR,
    GUARDIAN_COLOR,
    VIP_COLOR,
    CommentShadowType,
    COLOR_PRIMARY_WHITE,
    COLOR_BLACK,
    GCP_BASE_URL,
} from './constants';

import VIPIcon from './assets/ic-bdg-subs-m.svg';
import GuardianIcon from './assets/ic-msg-guardian.svg';

export const mapLevelToTextColor = normalizedLevel =>
    TEXT_COLORS[`LEVEL_${normalizedLevel}`] ||
    LEVEL_COLORS[`LEVEL_${normalizedLevel}`];

export const normalizeLevel = level => {
    // 獎勵帳號例如 17gift 的 level 會是 0, 為了不讓 LevelBadge 全白讓他返回 `00`
    if (level === 0) {
        return '00';
    }

    if (!level) {
        return '';
    }

    const normalized = Math.floor((level - 1) / 10) + 1;
    const ranged = normalized > MAX_LEVEL ? MAX_LEVEL : normalized;

    return padStart(String(ranged), 2, '0');
};

export const getDefaultImage = () => `${GCP_URL}/${DEFAULT_EMPTY_IMAGE}`;

const defaultAvatarImagePath = `${GCP_URL}/${DEFAULT_AVATAR_IMAGE}`;

export const getPicture = (picture, options = {}) => {
    const {
        shouldUseDefault = true,
        isThumbnail = true,
        isAvatar = true,
    } = options;

    if (picture) {
        return picture.indexOf(HTTP_PREFIX) === 0 ||
        picture.indexOf(HTTPS_PREFIX) === 0
            ? picture
            : `${CDN_URL}/${isThumbnail ? THUMBNAIL_PREFIX : ''}${picture}`;
    }

    if (shouldUseDefault) {
        if (isAvatar) {
            return defaultAvatarImagePath;
        }
        return getDefaultImage();
    }

    return '';
};


const cacheStore = new Map();

export const imageOnLoad = (
        src,
    timeout = 5000
) => {
    const handleOnLoad = (image) =>
        new Promise(resolve => {
            // handle timeouts of request default to 5 seconds
            const timer = setTimeout(resolve, timeout);

            const onLoad = () => {
                clearTimeout(timer);
                cacheStore.set(image.src, image);
                resolve(image);
            };

            const onError = () => {
                clearTimeout(timer);
                image.setAttribute('error', 'true');
                resolve(image); // we don't want some error to block whole list
            };

            image.onload = onLoad;
            image.onerror = onError;
        });

    if (isArray(src) && src.length) {
        // array of src
        const images = range(src.length).map(i => {
            if (cacheStore.has(src[i])) {
                return Promise.resolve(cacheStore.get(src[i]));
            }

            const img = new Image();
            img.src = src[i];

            return handleOnLoad(img);
        });

        // wait for all to be loaded
        return Promise.all(images);
    } else if (typeof src === 'string') {
        // single src of string
        if (cacheStore.has(src)) {
            return Promise.resolve(cacheStore.get(src));
        }

        const image = new Image();
        image.src = src;

        return handleOnLoad(image);
    }

    // other type of src just return
    return Promise.resolve();
};

/**
 * 來用判斷要用新還舊的以 image 為 pattern 的 canvas 繪製方式
 */
export const getIsSupportImageBitmapPattern = async (
    img,
    ctx
) => {
    try {
        const imageBitmap = createImageBitmap(img);
        ctx.createPattern(await imageBitmap, 'repeat');
        return true;
    } catch (_error) {
        return false;
    }
};

const drawImagePattern = ({
                              ctx,
                              pattern,
                              dx,
                              dy,
                              dWidth,
                              dHeight,
                          }) => {
    if (!pattern) {
        console.warn('[Canvas]', "Can't create pattern");
        return;
    }
    // Translate repeat origin by offset
    pattern.setTransform(new DOMMatrix([1, 0, 0, 1, dx, dy]));
    ctx.fillStyle = pattern ?? '';
    ctx.rect(dx, dy, dWidth, dHeight);
    ctx.closePath();
    ctx.fill();
};

/**
 * 取 image 特定範圍做為素材繪製留言框或框上圖案
 * 依 os 是否支援 bitmap image + create pattern 分成新舊寫法
 */
export const drawImage = ({
                              ctx,
                              id,
                              sx,
                              sy,
                              sw,
                              sh,
                              dx,
                              dy,
                              dWidth,
                              dHeight,
                              borderWidth,
                              img,
                              isSupportCreateImageBitmapWithPattern,
                          }) => {
    ctx.beginPath();

    /**
     *  0 |  1 |  2 |  4
     *  5 |  6 |  7 |  8
     *  9 | 10 | 11 | 12
     * 13 | 14 | 15 | 16
     * 框上圖案取 6/7/10/11 其中一塊重複，都是 borderWidth x borderWidth
     * 但框底要取 1&2/8&12/14&15/5&9 其中兩塊重複
     */
    const isMaterial = id?.includes('material');
    const resizeWidth = isMaterial
        ? borderWidth
        : borderWidth * (dWidth > dHeight ? 2 : 1); // 橫邊寬兩塊
    const resizeHeight = isMaterial
        ? borderWidth
        : borderWidth * (dWidth < dHeight ? 2 : 1); // 直邊高兩塊

    if (isSupportCreateImageBitmapWithPattern) {
        createImageBitmap(img, sx, sy, sw, sh, {
            resizeWidth,
            resizeHeight,
        }).then(imageBitmap => {
            ctx.beginPath();
            const pattern = ctx.createPattern(imageBitmap, 'repeat');
            drawImagePattern({ ctx, pattern, dx, dy, dWidth, dHeight });
        });
        return;
    }

    // 若 createImageBitmap 的新寫法支援度不足
    const patternCanvas = document.createElement('canvas');
    patternCanvas.width = resizeWidth;
    patternCanvas.height = resizeHeight;
    const patternCtx = patternCanvas.getContext('2d');
    if (patternCtx) {
        patternCtx.drawImage(img, sx, sy, sw, sh, 0, 0, resizeWidth, resizeHeight);
        const pattern = ctx.createPattern(patternCanvas, 'repeat');
        drawImagePattern({ ctx, pattern, dx, dy, dWidth, dHeight });
        patternCanvas.remove();
    }
};

/**
 * order:
 * 0 | 1 | 2
 * 7 |   | 3
 * 6 | 5 | 4

 * sx: sourceX
 * sy: sourceY
 * sw: source width
 * sh: source height
 * dx: destinationX
 * dy: destinationY
 * dWidth: destination width
 * sHeight: destination height
 * 參數命名參考：https://developer.mozilla.org/zh-CN/docs/Web/API/CanvasRenderingContext2D/drawImage
 */
const drawPattern = ({
                         ctx,
                         id,
                         sx,
                         sy,
                         sw,
                         sh,
                         dx,
                         dy,
                         dWidth,
                         dHeight,
                         borderWidth,
                         order,
                         img,
                         isSupportCreateImageBitmapWithPattern,
                     }) => {
    if (order === 0 || order === 2 || order === 4 || order === 6) {
        // 繪製留言框四個角
        ctx.drawImage(img, sx, sy, sw, sh, dx, dy, dWidth, dHeight);
    } else if (order === 1 || order === 3 || order === 5 || order === 7) {
        drawImage({
            ctx,
            id,
            sx,
            sy,
            sw,
            sh,
            dx,
            dy,
            dWidth,
            dHeight,
            borderWidth,
            img,
            isSupportCreateImageBitmapWithPattern,
        });
    }
};

const drawShineEffect = ({
                             shineEffectRef,
                             commentRef,
                             commentWidth,
                             commentHeight,
                         }) => {
    const ctx = commentRef?.current?.getContext('2d');
    const shineCtx = shineEffectRef?.current?.getContext('2d');
    if (!ctx || !shineCtx) {
        return;
    }

    const totalFrames = 30; // = 0.5s
    const shineWidth = commentHeight;
    const endX = commentWidth + shineWidth;
    shineCtx.fillStyle = '#fff';
    function drawFrame(frame) {
        if (!shineCtx) {
            return;
        }
        shineCtx?.clearRect(0, 0, endX, commentHeight);

        const progress = frame / totalFrames;
        const rectX = progress * endX;

        // 繪製閃光
        shineCtx.beginPath();
        shineCtx.moveTo(rectX, 0);
        shineCtx.lineTo(rectX + shineWidth, 0);
        shineCtx.lineTo(rectX, commentHeight);
        shineCtx.lineTo(rectX - shineWidth, commentHeight);
        shineCtx.closePath();
        shineCtx.fill();

        if (frame < totalFrames) {
            requestAnimationFrame(drawFrame.bind(null, frame + 1));
        }
    }
    requestAnimationFrame(drawFrame.bind(null, 0));
};

/**
 * 繪製 ChatRoomCommentFrameGradient 留言框主要 function
 * 有兩階段的 drawPattern，以圖上半一次繪製完基底框，再以圖下半動畫依序繪製變化後的框
 * 最後再繪製白色矩形左至右滑過，模仿閃爍效果
 */
export const drawGradientFrame = ({
                                      ctx,
                                      patternMap,
                                      shineEffectRef,
                                      commentRef,
                                      commentWidth,
                                      commentHeight,
                                      borderWidth,
                                      imgHalfHeight,
                                      img,
                                      isSupportCreateImageBitmapWithPattern,
                                      skipAnimationFrame,
                                      onAnimationEnd,
                                  }) => {
    const drawPatternMap = (
        patterns,
        isTransitioned
) => {
        patterns.forEach(
            ({ id, sx, sy, sw, sh, dx, dy, dWidth, dHeight, order }) => {
                drawPattern({
                    ctx,
                    id,
                    sx,
                    sy: sy + (isTransitioned ? imgHalfHeight : 0), // 繪製漸變動畫需要使用原圖的下半部，所以加上 imgHalfHeight 調整 sy 的高度
                    sw,
                    sh,
                    dx,
                    dy,
                    dWidth,
                    dHeight,
                    borderWidth,
                    order,
                    img,
                    isSupportCreateImageBitmapWithPattern,
                });
            }
        );
    };

    if (skipAnimationFrame) {
        // 直接繪製漸變後的結果
        for (let drawOrder = 0; drawOrder <= 7; drawOrder++) {
            drawPatternMap(
                patternMap.filter(pattern => pattern.order === drawOrder),
                true
            );
        }
        return;
    }

    // 繪製漸變前
    drawPatternMap(patternMap);

    const drawFrame = (drawOrder) => {
        if (drawOrder > 7) {
            // 繪製完漸變後再閃爍
            drawShineEffect({
                shineEffectRef,
                commentRef,
                commentWidth,
                commentHeight,
            });
            onAnimationEnd?.();
            return;
        }
        // 依序繪製漸變後
        drawPatternMap(
            patternMap.filter(pattern => pattern.order === drawOrder),
            true
        );
        setTimeout(
            () => requestAnimationFrame(() => drawFrame(drawOrder + 1)),
            COMMENT_BORDER_PAINT_INTERVAL_TIME
        );
    };

    // 由左上角開始順時鐘依序轉換成原圖下半漸變後的樣式
    setTimeout(
        () => requestAnimationFrame(() => drawFrame(0)),
        COMMENT_BORDER_PAINT_INTERVAL_TIME
    );
};

export const mapLevelToIconBackground = normalizedLevel =>
    (LEVEL_COLORS[`LEVEL_${normalizedLevel}`] || '').replace(
        'to right',
        'to bottom'
    );

export const mapNormalizedLevelToIconLevel = normalizedLevel =>
    ({
        /* eslint-disable quote-props */
        '11': '10',
        '21': '20',
        /* eslint-enable quote-props */
    }[normalizedLevel] || normalizedLevel);

export const mapReactionToBackgroundColor = type =>
    ({
        [ReactionTypes.SHARE_FB]: rgba(BD_WHITE, 0.7),
        [ReactionTypes.SHARE_17]: rgba(BD_WHITE, 0.7),
    }[type] || '');

// used in mobile
export const mapUserTypeToBackgroundColor = type =>
    ({
        [USER_VIP]: rgba(VIP_BANNER_BACKGROUND, 0.7),
        [USER_GUARDIAN]: rgba(GUARDIAN_BANNER_BACKGROUND, 0.8),
        [USER_STREAMER]: rgba(STREAMER_COLOR, 0.7),
        [USER_AI]: rgba(BD_WHITE, 0.9),
    }[type] || '');

export const getUserType = (
        user,
        streamerInfo
) => {
    // this order is important (streamer -> guardian -> vip -> normal)
    if (streamerInfo && user.userID === streamerInfo.get('userID')) {
        return USER_STREAMER;
    } else if (user.isGuardian) {
        return USER_GUARDIAN;
    } else if (user.isVIP) {
        return USER_VIP;
    } else if (user.isSystem) {
        return USER_SYSTEM;
    } else if (user.isAiCohost) {
        return USER_AI;
    }

    return USER_NORMAL;
};

export const mapUserTypeToColor = type =>
    ({
        [USER_VIP]: VIP_COLOR,
        [USER_GUARDIAN]: GUARDIAN_COLOR,
        [USER_STREAMER]: STREAMER_COLOR,
        [USER_SYSTEM]: SYSTEM_COLOR,
    }[type]);

export const mapUserTypeToIcon = type =>
    ({
        [USER_VIP]: VIPIcon,
        [USER_GUARDIAN]: GuardianIcon,
    }[type] || '');

export const mapCommentShadowColor = type => {
    switch (type) {
        case CommentShadowType.LIGHT:
            return COLOR_PRIMARY_WHITE;
        case CommentShadowType.DARK:
            return COLOR_BLACK;
        case CommentShadowType.NONE:
        default:
            break;
    }
};

const createGCSImage = folders => {
    return (file) => `${GCP_BASE_URL}/17live/${folders.join('/')}/${file}`;
};

export const getCheckingLevelImage = createGCSImage([
    'checking',
]);

const isRetina = () => {
    return window.devicePixelRatio >= 2;
};

const getPathName = path => {
    const [ext] = path.match(/([^.])+$/) ?? [];
    const base = path
        .split('.')
        .slice(0, -1)
        .join('.');

    return { ext, base };
};

export const getWebp2xURL = (src, { has2x = true } = {}) => {
    const ratio = isRetina() && has2x ? '@2x' : '';

    const { base, ext } = getPathName(src);

    const extName = window.Modernizr?.webp ? `webp` : ext;

    return `${base}${ratio}.${extName}`;
};