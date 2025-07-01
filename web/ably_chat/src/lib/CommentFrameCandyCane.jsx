import React, { useCallback, useEffect, useRef } from 'react';

import styled from 'styled-components';

import { imageOnLoad } from './utils';
import { PATTERN_IMG_RESIZE, COMMENT_BORDER_RADIUS_CANDY_CANE } from './constants';

const CommentFrame = styled.div`
  max-width: 100%;
  position: relative;
  display: inline-flex;
  padding: ${({ borderWidth }) => borderWidth}px;
`;

const CommentInner = styled.canvas`
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  width: ${p => p.width}px;
  height: ${p => p.height}px;
  border-radius: ${COMMENT_BORDER_RADIUS_CANDY_CANE}px;
  pointer-events: none;
`;

/**
 * Type 1
 */
const CommentFrameCandyCane = ({
                                           imageURL,
                                           children,
                                           width: commentWidth,
                                           height: commentHeight,
                                           borderWidth,
                                       }) => {
    const commentRef = useRef(null);

    const draw = useCallback(
        (img) => {
            const ctx = commentRef?.current?.getContext('2d');
            if (!ctx || commentWidth <= 2 * borderWidth) {
                return;
            }
            ctx.clearRect(0, 0, commentWidth, commentHeight);

            // Image used for fill background is first resized as pattern
            const patternCanvas = document.createElement('canvas');
            const patternCtx = patternCanvas.getContext('2d');
            patternCanvas.width = PATTERN_IMG_RESIZE;
            patternCanvas.height = PATTERN_IMG_RESIZE;
            patternCtx?.drawImage(img, 0, 0, PATTERN_IMG_RESIZE, PATTERN_IMG_RESIZE);

            const pattern = ctx.createPattern(patternCanvas, 'repeat');
            patternCanvas.remove();

            // Make overlapping parts disappear to create hollow rounded border
            ctx.globalCompositeOperation = 'xor';

            ctx.fillStyle = pattern || '';
            ctx.rect(0, 0, commentWidth, commentHeight);
            ctx.fill();

            ctx.beginPath();
            ctx.roundRect(
                borderWidth,
                borderWidth,
                commentWidth - 2 * borderWidth,
                commentHeight - 2 * borderWidth,
                [COMMENT_BORDER_RADIUS_CANDY_CANE]
            );
            ctx.fill();
        },
        [commentWidth, commentHeight]
    );

    useEffect(() => {
        // Draw after image is loaded
        imageOnLoad(imageURL).then(draw);
    }, [imageURL, draw]);

    return (
        <CommentFrame borderWidth={borderWidth}>
            {children}
            <CommentInner
                ref={commentRef}
                width={commentWidth}
                height={commentHeight}
            />
        </CommentFrame>
    );
};

export default CommentFrameCandyCane;
