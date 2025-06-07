import * as React from 'react';

function SvgIcSuperfanLevel(
    props,
    svgRef
) {
    return (
        <svg
            width="1em"
            height="1em"
            viewBox="0 0 10 10"
            fill="none"
            xmlns="http://www.w3.org/2000/svg"
            ref={svgRef}
            {...props}
        >
            <g clipPath="url(#IcSuperfanLevel_svg__clip0_7792_251955)">
                <path
                    fillRule="evenodd"
                    clipRule="evenodd"
                    d="M5.883.894a.938.938 0 00-.938 0L2.094 2.54a.938.938 0 00-.469.812v3.293c0 .335.179.644.469.812l2.851 1.646c.29.167.648.167.938 0l2.851-1.646a.937.937 0 00.469-.812V3.352a.938.938 0 00-.469-.812L5.883.894zm-.663 5.05l-.187 1.555H3.649l.534-4.28a.259.259 0 01.256-.227h2.348l.777-.493-.098.493-.131.76a.388.388 0 01-.382.323H5.446l-.093.825h1.395l-.125 1.043H5.22z"
                    fill={props.fill ?? '#6F6B88'}
                />
            </g>
            <defs>
                <clipPath id="IcSuperfanLevel_svg__clip0_7792_251955">
                    <path fill="#fff" transform="translate(.414)" d="M0 0h10v10H0z" />
                </clipPath>
            </defs>
        </svg>
    );
}
const ForwardRef = React.forwardRef(SvgIcSuperfanLevel);
const MemoForwardRef = React.memo(ForwardRef);
export default MemoForwardRef;
