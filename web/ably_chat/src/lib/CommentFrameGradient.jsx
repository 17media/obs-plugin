import React, { useCallback, useEffect, useRef } from 'react';

import styled from 'styled-components';

import {
    drawGradientFrame,
    getIsSupportImageBitmapPattern,
    imageOnLoad,
} from './utils';

const CommentFrame = styled.div`
  max-width: 100%;
  position: relative;
  display: inline-flex;
  padding: ${({ borderWidth, borderOffset }) => borderWidth - borderOffset}px;
`;

const CommentInner = styled.canvas`
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  width: ${p => p.width}px;
  height: ${p => p.height}px;
  pointer-events: none;
`;

const CommentFrameShineEffect = styled.canvas`
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  width: ${p => p.width}px;
  height: ${p => p.height}px;
  pointer-events: none;
`;

/**
 * Type 3
 * If the image is normally cut into 4x4 blocks, edge blocks will contain gaps, so manual correction is needed to trim about 1/4 borderWidth from each edge
 */
const CommentFrameGradient = ({
                                          imageURL,
                                          children,
                                          width: commentWidth,
                                          height: commentHeight,
                                          borderWidth,
                                          skipAnimationFrame,
                                          onAnimationEnd,
                                      }) => {
    const commentRef = useRef(null);
    const shineEffectRef = useRef(null);
    const borderOffset = borderWidth / 4;

    const draw = useCallback(
        async (img) => {
            const ctx = commentRef?.current?.getContext('2d');
            if (!ctx || commentWidth <= 2 * borderWidth) {
                return;
            }
            const isSupportCreateImageBitmapWithPattern = await getIsSupportImageBitmapPattern(
                img,
                ctx
            );

            ctx.clearRect(0, 0, commentWidth, commentHeight);
            const pieceWidth = img.width / 4;
            const pieceHeight = img.height / 8;

            // Since the original image is divided into upper and lower images, need to get the Y axis of the lower image
            const imgHalfHeight = img.height / 2;

            /**
             * order:
             * 0 | 1 | 2
             * 7 |   | 3
             * 6 | 5 | 4
             *
             * sx: sourceX
             * sy: sourceY
             * sw: source width
             * sh: source height
             * dx: destinationX
             * dy: destinationY
             * dWidth: destination width
             * sHeight: destination height
             * Parameter naming reference: https://developer.mozilla.org/zh-CN/docs/Web/API/CanvasRenderingContext2D/drawImage
             */
            const patternMap = [
                {
                    id: 'left_top',
                    order: 0,
                    sx: 0,
                    sy: 0,
                    sw: pieceWidth,
                    sh: pieceHeight,
                    dx: 0,
                    dy: 0,
                    dWidth: borderWidth,
                    dHeight: borderWidth,
                },
                {
                    id: 'top',
                    order: 1,
                    sx: pieceWidth,
                    sy: 0,
                    sw: pieceWidth * 2,
                    sh: pieceHeight,
                    dx: borderWidth,
                    dy: 0,
                    dWidth: commentWidth - borderWidth * 2 - borderOffset * 2,
                    dHeight: borderWidth,
                },
                {
                    id: 'material_2', // Top right --> Top
                    order: 1,
                    sx: pieceWidth * 2,
                    sy: pieceHeight,
                    sw: pieceWidth,
                    sh: pieceHeight,
                    dx: borderWidth,
                    dy: 0,
                    dWidth: commentWidth - borderWidth * 2 - borderOffset * 2,
                    dHeight: borderWidth,
                },
                {
                    id: 'right_top',
                    order: 2,
                    sx: pieceWidth * 3,
                    sy: 0,
                    sw: pieceWidth,
                    sh: pieceHeight,
                    dx: commentWidth - borderWidth - borderOffset * 2,
                    dy: 0,
                    dWidth: borderWidth,
                    dHeight: borderWidth,
                },
                {
                    id: 'right',
                    order: 3,
                    sx: pieceWidth * 3,
                    sy: pieceHeight,
                    sw: pieceWidth,
                    sh: pieceHeight * 2,
                    dx: commentWidth - borderWidth - borderOffset * 2,
                    dy: borderWidth,
                    dWidth: borderWidth,
                    dHeight: commentHeight - borderWidth * 2 - borderOffset * 2,
                },
                {
                    id: 'material_4', // Bottom right --> Right
                    order: 3,
                    sx: pieceWidth * 2,
                    sy: pieceHeight * 2,
                    sw: pieceWidth,
                    sh: pieceHeight,
                    dx: commentWidth - borderWidth - borderOffset * 2,
                    dy: borderWidth,
                    dWidth: borderWidth,
                    dHeight: commentHeight - borderWidth * 2 - borderOffset * 2,
                },
                {
                    id: 'right_bottom',
                    order: 4,
                    sx: pieceWidth * 3,
                    sy: pieceHeight * 3,
                    sw: pieceWidth,
                    sh: pieceHeight,
                    dx: commentWidth - borderWidth - borderOffset * 2,
                    dy: commentHeight - borderWidth - borderOffset * 2,
                    dWidth: borderWidth,
                    dHeight: borderWidth,
                },
                {
                    id: 'bottom',
                    order: 5,
                    sx: pieceWidth,
                    sy: pieceHeight * 3,
                    sw: pieceWidth * 2,
                    sh: pieceHeight,
                    dx: borderWidth,
                    dy: commentHeight - borderWidth - borderOffset * 2,
                    dWidth: commentWidth - borderWidth * 2 - borderOffset * 2,
                    dHeight: borderWidth,
                },
                {
                    id: 'material_3', // Bottom left --> Bottom
                    order: 5,
                    sx: pieceWidth,
                    sy: pieceHeight * 2,
                    sw: pieceWidth,
                    sh: pieceHeight,
                    dx: borderWidth,
                    dy: commentHeight - borderWidth - borderOffset * 2,
                    dWidth: commentWidth - borderWidth * 2 - borderOffset * 2,
                    dHeight: borderWidth,
                },
                {
                    id: 'left_bottom',
                    order: 6,
                    sx: 0,
                    sy: pieceHeight * 3,
                    sw: pieceWidth,
                    sh: pieceHeight,
                    dx: 0,
                    dy: commentHeight - borderWidth - borderOffset * 2,
                    dWidth: borderWidth,
                    dHeight: borderWidth,
                },
                {
                    id: 'left',
                    order: 7,
                    sx: 0,
                    sy: pieceHeight,
                    sw: pieceWidth,
                    sh: pieceHeight * 2,
                    dx: 0,
                    dy: borderWidth,
                    dWidth: borderWidth,
                    dHeight: commentHeight - borderWidth * 2 - borderOffset * 2,
                },
                {
                    id: 'material_1', // Top left --> Left
                    order: 7,
                    sx: pieceWidth,
                    sy: pieceHeight,
                    sw: pieceWidth,
                    sh: pieceHeight,
                    dx: 0,
                    dy: borderWidth,
                    dWidth: borderWidth,
                    dHeight: commentHeight - borderWidth * 2 - borderOffset * 2,
                },
            ];

            // draw transform frame with animation
            drawGradientFrame({
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
            });
        },
        [commentWidth, commentHeight, skipAnimationFrame]
    );

    useEffect(() => {
        // Draw after image is loaded
        imageOnLoad(imageURL).then(draw);
    }, [imageURL, draw]);

    return (
        <CommentFrame borderWidth={borderWidth} borderOffset={borderOffset}>
            {children}
            <CommentInner
                ref={commentRef}
                width={commentWidth - borderOffset * 2}
                height={commentHeight - borderOffset * 2}
            />
            <CommentFrameShineEffect
                ref={shineEffectRef}
                width={commentWidth - borderOffset * 2}
                height={commentHeight - borderOffset * 2}
            />
        </CommentFrame>
    );
};

export default CommentFrameGradient;
