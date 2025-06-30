import React from 'react';

import InlineSVG from 'react-inlinesvg';

const SVG = ({ src, ...props }) =>
    src ? <InlineSVG src={src} {...props} /> : null;

export default SVG;
