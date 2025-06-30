import React from 'react';

import { BorderType, COMMENT_BORDER_PADDING_CANDY_CANE } from './constants';
import CommentFrameCandyCane from './CommentFrameCandyCane';
import CommentFrameGradient from './CommentFrameGradient';
import CommentFrameMetal from './CommentFrameMetal';


const CommentFrameWrapper = ({
                                                          border,
                                                          size: { width, height },
                                                          skipAnimationFrame = false,
                                                          onAnimationEnd,
                                                          children,
                                                      }) => {
    if (!border) {
        return <>{children}</>;
    }

    const url = border.get('URL');
    const borderType = border.get('type');
    let borderWidth = border.get('borderWidth');

    if (borderType === BorderType.CANDY_CANE) {
        borderWidth = COMMENT_BORDER_PADDING_CANDY_CANE;
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
