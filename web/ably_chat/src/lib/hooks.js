import React, {
    useCallback,
    useLayoutEffect,
    useMemo,
    useRef,
    useState,
} from 'react';

// Immutable v3 does not provide isImmutable; use generic toJS detection instead

import BadgeImage from './BadgeImage';
import { getChatAssetProxyUrl } from './utils';


const transformImmutable = item => {
    if (item && typeof item.toJS === 'function') {
        return item.toJS();
    }
    return item;
};

const getCommentSize = node => ({
    width: Math.ceil(node?.getBoundingClientRect?.().width ?? node?.clientWidth ?? 0),
    height: Math.ceil(node?.getBoundingClientRect?.().height ?? node?.clientHeight ?? 0),
});

const getAvailableWidth = node => {
    const container = node?.closest?.('[data-chat-message-content="true"]');
    return Math.max(0, Math.floor(container?.getBoundingClientRect?.().width ?? container?.clientWidth ?? 0));
};

const useComment = ({
                        levelBadges: originalLevelBadges,
                        prefixBadges,
                        asideLiveWidth,
                        layoutVersion,
                    }) => {
    const commentRef = useRef(null);
    const [size, setSize] = useState({ width: 0, height: 0 });
    const [availableWidth, setAvailableWidth] = useState(0);
    const [skipAnimationFrame, setSkipAnimationFrame] = useState(false);
    const updateSize = useCallback(() => {
        const nextSize = getCommentSize(commentRef.current);
        setSize(prevSize =>
            prevSize.width === nextSize.width && prevSize.height === nextSize.height
                ? prevSize
                : nextSize
        );
    }, []);
    const updateAvailableWidth = useCallback(() => {
        const nextWidth = getAvailableWidth(commentRef.current);
        setAvailableWidth(prevWidth => (prevWidth === nextWidth ? prevWidth : nextWidth));
    }, []);

    useLayoutEffect(() => {
        updateAvailableWidth();
        updateSize();

        if (typeof window === 'undefined' || typeof ResizeObserver !== 'function' || !commentRef.current) {
            return undefined;
        }

        const commentNode = commentRef.current;
        const containerNode = commentNode.closest?.('[data-chat-message-content="true"]');
        const observer = new ResizeObserver(() => {
            updateAvailableWidth();
            updateSize();
        });

        observer.observe(commentNode);
        if (containerNode) {
            observer.observe(containerNode);
        }

        return () => observer.disconnect();
    }, [updateAvailableWidth, updateSize]);

    useLayoutEffect(() => {
        updateAvailableWidth();
        updateSize();
    }, [asideLiveWidth, layoutVersion, updateAvailableWidth, updateSize]);

    const levelBadges = useMemo(() => transformImmutable(originalLevelBadges), [
        originalLevelBadges,
    ]);

    const prefixBadgeContents = useMemo(
        () =>
            transformImmutable(prefixBadges)?.map(({ URL: prefixBadge }, index) => (
                <BadgeImage key={index} src={getChatAssetProxyUrl(prefixBadge)} />
            )),
        [prefixBadges]
    );

    const handleAnimationEnd = useCallback(() => {
        if (!skipAnimationFrame) {
            setSkipAnimationFrame(true);
        }
    }, [skipAnimationFrame]);

    return {
        commentRef,
        size,
        availableWidth,
        levelBadges,
        prefixBadgeContents,
        skipAnimationFrame,
        handleAnimationEnd,
    };
};

export default useComment;
