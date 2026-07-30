"use client";
import React, { useEffect, useMemo, useRef, useState } from 'react';
import styled from 'styled-components';

const Wrapper = styled.div`
  position: relative;
  width: ${(p) => (p.$width ? p.$width : 'auto')};
  min-width: ${(p) => (p.$minWidth ? p.$minWidth : '162px')};
  max-width: ${(p) => (p.$maxWidth ? p.$maxWidth : '320px')};
`;

const Trigger = styled.button`
  display: flex;
  flex-direction: row;
  align-items: center;
  justify-content: space-between;
  gap: 4px;
  isolation: isolate;
  width: 100%;
  height: 32px;
  padding: 6px 12px;
  background: #3C404C;
  border-radius: 6px;
  border: none;
  outline: none;
  color: #A1A9B6;
  font-family: Inter, system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, "Apple Color Emoji", "Segoe UI Emoji";
  font-weight: 400;
  font-size: 14px;
  line-height: 20px;
  position: relative;
  cursor: pointer;
`;

const LabelText = styled.span`
  flex: 1;
  text-align: left;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
`;

const RightChevron = styled.span`
  position: absolute;
  right: 8px;
  top: 50%;
  transform: translateY(-50%) rotate(${(p) => (p.$open ? '-135deg' : '45deg')});
  transition: transform 150ms ease;
  width: 8px;
  height: 8px;
  border-bottom: 1px solid #B3B3B3;
  border-right: 1px solid #B3B3B3;
  pointer-events: none;
`;

const Popover = styled.div`
  position: absolute;
  top: calc(100% + 8px);
  left: 0;
  width: 100%;
  background: #3C404C;
  border: 1px solid #3C404C;
  border-radius: 8px;
  padding: 8px;
  z-index: 50;
`;

const List = styled.div`
  display: flex;
  flex-direction: column;
  gap: 8px;
`;

const Item = styled.button`
  width: 100%;
  text-align: left;
  background: transparent;
  border: none;
  border-radius: 6px;
  padding: 6px 8px;
  color: #A1A9B6;
  font-family: Inter, system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, "Apple Color Emoji", "Segoe UI Emoji";
  font-weight: 400;
  font-size: 14px;
  line-height: 20px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  cursor: pointer;
  &:hover { background: #4A4F5D; }
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
`;

const NameLabel = styled.span`
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
`;

export const PopoverSelect = ({
  ariaLabel,
  options,
  value,
  onChange,
  getOptionValue = (o) => o?.value,
  getOptionLabel = (o) => o?.label,
  renderItemRight,
  renderSelectedRight,
  minWidth,
  maxWidth,
  width,
}) => {
  const [open, setOpen] = useState(false);
  const triggerRef = useRef(null);
  const popoverRef = useRef(null);

  const selected = useMemo(() => options?.find((o) => getOptionValue(o) === value), [options, value, getOptionValue]);

  useEffect(() => {
    const handleClickOutside = (e) => {
      if (!open) return;
      const t = triggerRef.current;
      const p = popoverRef.current;
      if (t && t.contains(e.target)) return;
      if (p && p.contains(e.target)) return;
      setOpen(false);
    };
    const handleKey = (e) => {
      if (e.key === 'Escape') setOpen(false);
    };
    document.addEventListener('mousedown', handleClickOutside);
    document.addEventListener('keyup', handleKey);
    return () => {
      document.removeEventListener('mousedown', handleClickOutside);
      document.removeEventListener('keyup', handleKey);
    };
  }, [open]);

  const updateSelection = (nextValue) => {
    setOpen(false);
    onChange?.(nextValue);
  };

  return (
    <Wrapper $minWidth={minWidth} $maxWidth={maxWidth} $width={width}>
      <Trigger aria-label={ariaLabel} onClick={() => setOpen((prev) => !prev)} ref={triggerRef}>
        <LabelText>{selected ? getOptionLabel(selected) : ''}</LabelText>
        {selected ? renderSelectedRight?.(selected) : null}
        <RightChevron aria-hidden="true" $open={open} />
      </Trigger>
      {open && (
        <Popover ref={popoverRef}>
          <List>
            {(options || []).map((o) => {
              const v = getOptionValue(o);
              return (
                <Item key={v} onClick={() => updateSelection(v)}>
                  <NameLabel>{getOptionLabel(o)}</NameLabel>
                  {renderItemRight?.(o)}
                </Item>
              );
            })}
          </List>
        </Popover>
      )}
    </Wrapper>
  );
};

export default PopoverSelect;
