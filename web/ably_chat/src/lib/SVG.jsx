import React from 'react';

import InlineSVG from 'react-inlinesvg';

const SVG = ({ src, style, ...props }) =>
    src ? (
        <InlineSVG
            src={src}
            style={{
                display: 'inline-block',
                verticalAlign: 'middle',
                ...style,
            }}
            {...props}
        />
    ) : null;

export default SVG;
