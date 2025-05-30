import { PHASE_DEVELOPMENT_SERVER, PHASE_PRODUCTION_BUILD, PHASE_EXPORT } from 'next/constants.js';

import createNextIntlPlugin from 'next-intl/plugin';

const withNextIntl = createNextIntlPlugin({})
/** @type {import('next').NextConfig} */
const nextConfig = (phase, { defaultConfig }) => {
  const config = {
    ...defaultConfig,
  };

  if (phase === PHASE_PRODUCTION_BUILD || phase === PHASE_EXPORT) {
    config.output = 'export';
    config.distDir = '../../data/html/chat';
    config.basePath = '/chat';
  }

  // For development, ensure output and distDir are not set or are default
  // if (phase === PHASE_DEVELOPMENT_SERVER) {
  //   // Next.js defaults handle this, but you can explicitly unset if needed
  //   // delete config.output;
  //   // delete config.distDir;
  // }

  return config;
};

export default withNextIntl(nextConfig);
