import React, { PureComponent } from 'react';

import styled from 'styled-components';
import { from } from 'rxjs';

import defaultSVG from './assets/ic-lv-default.svg';
import SVG from './SVG';
import { LEVEL_COLORS } from './constants';
import {
    mapLevelToIconBackground,
    mapNormalizedLevelToIconLevel,
    normalizeLevel,
} from './utils';

export const LevelIconWrapper = styled(SVG).attrs(props => ({
    style: {
        background: props.isIcon
            ? mapLevelToIconBackground(props.normalizedLevel)
            : LEVEL_COLORS[`LEVEL_${props.normalizedLevel}`],
    },
}))`
  display: inline-flex;
  height: 30px;
  width: 30px;
  border-radius: 50%;
  justify-content: center;
  align-items: center;
`;

class LevelIcon extends PureComponent {
    source$;

    state = {
        src: '',
    };

    componentDidMount() {
        this.loadLevelIcon();
    }

    componentDidUpdate(nextProps) {
        if (nextProps.level !== this.props.level) {
            this.loadLevelIcon();
        }
    }

    componentWillUnmount() {
        if (this.source$) {
            this.source$.unsubscribe();
        }
    }

    loadLevelIcon = () => {
        const normalizedLevel = normalizeLevel(this.props.level);

        /**
         * Not expect to load level icon when
         * 1. level 0
         * 2. level undefined when not loaded
         */
        if (Number(normalizedLevel) === 0) {
            return;
        }

        const normalizedIconLevel = mapNormalizedLevelToIconLevel(normalizedLevel);

        if (this.source$) {
            this.source$.unsubscribe();
        }

        // prettier-ignore
        this.source$ = from(
            import(`./assets/levels/ic-lv-${normalizedIconLevel}.svg`)
        ).subscribe((src) => {
            this.setState({
                src: typeof src === 'string' ? src : src.default,
            });
        });
    };

    handleLoaded = () => {
        const { onLoaded } = this.props;
        if (onLoaded) {
            onLoaded();
        }
    };

    render() {
        const { isIcon, level, className } = this.props;
        const { src } = this.state;

        return (
            <LevelIconWrapper
                onLoad={this.handleLoaded}
                className={className}
                key={src}
                src={src || defaultSVG}
                normalizedLevel={normalizeLevel(level)}
                isIcon={isIcon}
            />
        );
    }
}

export default LevelIcon;
