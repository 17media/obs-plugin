import React, {
    useCallback,
    useEffect,
    useLayoutEffect,
    useMemo,
    useRef,
    useState,
} from 'react';

import { isImmutable } from 'immutable-v4';

import BadgeImage from './BadgeImage';


const transformImmutable = item => {
    if (isImmutable(item)) {
        return item.toJS();
    }
    return item;
};

const useComment = ({
                        levelBadges: originalLevelBadges,
                        prefixBadges,
                        asideLiveWidth,
                    }) => {
    const [isInView, setIsInView] = useState(false);
    const commentRef = useRef(null);
    const [size, setSize] = useState({ width: 0, height: 0 });
    const [skipAnimationFrame, setSkipAnimationFrame] = useState(false);

    useLayoutEffect(() => {
        if (commentRef?.current) {
            setSize({
                width: commentRef.current.clientWidth ?? 0,
                height: commentRef.current.clientHeight ?? 0,
            });
        }
    }, []);

    useEffect(() => {
        const observer = new window.IntersectionObserver(([entry]) => {
            setIsInView(entry.isIntersecting);
        });

        if (commentRef.current) {
            observer.observe(commentRef.current);
        }

        return () => {
            if (commentRef.current) {
                observer.unobserve(commentRef.current);
            }
        };
    }, []);

    useEffect(() => {
        // Redraw comment box size based on asideLiveWidth for visible chat width
        if (!isInView) {
            return;
        }

        if (commentRef.current) {
            setSize({
                width: commentRef.current.clientWidth ?? 0,
                height: commentRef.current.clientHeight ?? 0,
            });
        }
    }, [isInView, asideLiveWidth, commentRef.current]);

    const levelBadges = useMemo(() => transformImmutable(originalLevelBadges), [
        originalLevelBadges,
    ]);

    const prefixBadgeContents = useMemo(
        () =>
            transformImmutable(prefixBadges)?.map(({ URL: prefixBadge }, index) => (
                <BadgeImage key={index} src={prefixBadge} />
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
        levelBadges,
        prefixBadgeContents,
        skipAnimationFrame,
        handleAnimationEnd,
    };
};

export default useComment;
