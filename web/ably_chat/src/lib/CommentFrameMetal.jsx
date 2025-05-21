import React, { useCallback, useEffect, useRef, useState } from 'react';

import styled from 'styled-components';

import {
    drawImage,
    getIsSupportImageBitmapPattern,
    imageOnLoad,
} from './utils';

const CommentFrame = styled.div`
  position: relative;
  display: inline-flex;
  padding: ${({ borderWidth, borderOffset }) => borderWidth - borderOffset}px;
  max-width: 100%;
`;

const CommentInner = styled.canvas`
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  border-image: ${p => `url(${p.imageURL})`};
  border-image-slice: ${({ pieceHeight }) => pieceHeight}
    ${({ pieceWidth }) => pieceWidth};
  border-image-width: ${({ borderWidth }) => borderWidth}px;
  border-image-repeat: repeat;
  max-width: 100%;
  width: ${p => p.width}px;
  height: ${p => p.height}px;
  pointer-events: none;
`;

/**
 * Type 2
 * 因若將圖片正常切 4x4 塊，邊邊塊會含有間隙，故需要人工校正每邊剪約 1/4 borderWidth
 */
const CommentFrameMetal = ({
                                       imageURL,
                                       width: commentWidth,
                                       height: commentHeight,
                                       borderWidth,
                                       children,
                                   }) => {
    const commentRef = useRef(null);
    const [pieceSize, setPieceSize] = useState({ pieceWidth: 0, pieceHeight: 0 });
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
            const pieceHeight = img.height / 4;
            setPieceSize({ pieceWidth, pieceHeight });

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
             * 參數命名參考：https://developer.mozilla.org/zh-CN/docs/Web/API/CanvasRenderingContext2D/drawImage
             */
            const patternMap = [
                {
                    id: 'material_2', // 右上 --> Top
                    order: 1,
                    sx: pieceWidth * 2,
                    sy: pieceHeight,
                    sw: pieceWidth,
                    sh: pieceHeight,
                    dx: borderWidth,
                    dy: 0,
                    dWidth: commentWidth - borderWidth * 2 - borderOffset * 2,
                    dHeight: borderWidth - borderOffset * 2,
                },
                {
                    id: 'material_4', // 右下 --> Right
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
                    id: 'material_3', // 左下 --> Bottom
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
                    id: 'material_1', // 左上 --> Left
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

            patternMap.forEach(
                ({ id, sx, sy, sw, sh, dx, dy, dWidth, dHeight, order }) => {
                    // 繪製留言框上的圖案
                    if (order === 1 || order === 3 || order === 5 || order === 7) {
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
                }
            );
        },
        [borderWidth, commentHeight, commentWidth]
    );

    useEffect(() => {
        // 在圖片載入後進行繪製
        imageOnLoad(imageURL).then(draw);
    }, [imageURL, draw]);

    return (
        <CommentFrame borderWidth={borderWidth} borderOffset={borderOffset}>
            {children}
            <CommentInner
                ref={commentRef}
                imageURL={imageURL}
                borderWidth={borderWidth}
                pieceWidth={pieceSize.pieceWidth}
                pieceHeight={pieceSize.pieceHeight}
                width={commentWidth - borderOffset * 2}
                height={commentHeight - borderOffset * 2}
            />
        </CommentFrame>
    );
};

export default CommentFrameMetal;
