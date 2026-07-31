'use client';

import { useState } from 'react';
import { useServerInsertedHTML } from 'next/navigation';
import {
  ServerStyleSheet,
  StyleSheetManager,
  ThemeProvider,
} from 'styled-components';
import GlobalStyle from './GlobalStyle';
import theme from './theme';

export function StyledComponentsProvider({ children }) {
  const [styledComponentsStyleSheet] = useState(() => new ServerStyleSheet());

  useServerInsertedHTML(() => {
    const styles = styledComponentsStyleSheet.getStyleElement();
    styledComponentsStyleSheet.instance.clearTag();
    return <>{styles}</>;
  });

  const content = (
    <ThemeProvider theme={theme}>
      <GlobalStyle />
      {children}
    </ThemeProvider>
  );

  if (typeof window !== 'undefined') {
    return content;
  }

  return (
    <StyleSheetManager sheet={styledComponentsStyleSheet.instance}>
      {content}
    </StyleSheetManager>
  );
}

export default StyledComponentsProvider;
