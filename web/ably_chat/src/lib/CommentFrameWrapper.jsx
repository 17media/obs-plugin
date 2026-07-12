import React, { useEffect, useMemo, useState } from 'react';

import { BorderType, COMMENT_BORDER_PADDING_CANDY_CANE } from './constants';
import CommentFrameCandyCane from './CommentFrameCandyCane';
import CommentFrameGradient from './CommentFrameGradient';
import CommentFrameMetal from './CommentFrameMetal';
import { getChatAssetProxyUrl, imageOnLoad, isImageLoaded } from './utils';


const CommentFrameWrapper = ({
                                                          border,
                                                          size: { width, height },
                                                          skipAnimationFrame = false,
                                                          onAnimationEnd,
                                                          children,
                                                      }) => {
    const hasBorder = !!border;
    const url = useMemo(() => (border ? getChatAssetProxyUrl(border.get('URL')) : ''), [border]);
    const borderType = border?.get('type');
    let borderWidth =
        borderType === BorderType.CANDY_CANE
            ? COMMENT_BORDER_PADDING_CANDY_CANE
            : border?.get('borderWidth');
    const [assetReady, setAssetReady] = useState(false);

    useEffect(() => {
        let disposed = false;

        if (!url) {
            setAssetReady(false);
            return undefined;
        }

        setAssetReady(false);
        imageOnLoad(url).then(image => {
            if (!disposed) {
                setAssetReady(isImageLoaded(image));
            }
        });

        return () => {
            disposed = true;
        };
    }, [url]);

    if (
        !hasBorder ||
        !url ||
        !Number.isFinite(borderWidth) ||
        borderWidth <= 0 ||
        width <= 0 ||
        height <= 0 ||
        !assetReady
    ) {
        return <>{children}</>;
    }

    if (borderType === BorderType.CANDY_CANE) {
        return (
            <CommentFrameCandyCane
                imageURL={url}
                width={width + borderWidth * 2}
                height={height + borderWidth * 2}
                borderWidth={borderWidth}
            >
                {children}
            </CommentFrameCandyCane>
        );
    }

    if (borderType === BorderType.METAL) {
        return (
            <CommentFrameMetal
                imageURL={url}
                width={width + borderWidth * 2}
                height={height + borderWidth * 2}
                borderWidth={borderWidth}
            >
                {children}
            </CommentFrameMetal>
        );
    }

    if (borderType === BorderType.GRADIENT) {
        return (
            <CommentFrameGradient
                imageURL={url}
                width={width + borderWidth * 2}
                height={height + borderWidth * 2}
                borderWidth={borderWidth}
                skipAnimationFrame={skipAnimationFrame}
                onAnimationEnd={onAnimationEnd}
            >
                {children}
            </CommentFrameGradient>
        );
    }

    return <>{children}</>;
};

export default CommentFrameWrapper;
