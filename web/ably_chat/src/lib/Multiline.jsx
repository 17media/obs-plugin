import React from 'react';

const Multiline = ({ className, children }) => (
    <span className={className}>
    {typeof children === 'string'
        ? children.split(/\n|\\n/).map((str, i) => (
            <span key={i}>
            {str}
                <br />
          </span>
        ))
        : children}
  </span>
);

export default Multiline;
