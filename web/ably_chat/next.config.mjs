import createNextIntlPlugin from 'next-intl/plugin';

const withNextIntl = createNextIntlPlugin({
  requestConfig: './src/i18n/request.js',
  experimental: {
    createMessagesDeclaration: './messages/en.json'
  }
});

const enableYouTube = process.env.ENABLE_YOUTUBE === 'true';

/** @type {import('next').NextConfig} */
const nextConfig = {
  compiler: {
    styledComponents: true
  },
  env: {
    NEXT_PUBLIC_ENABLE_YOUTUBE: enableYouTube ? 'true' : 'false'
  }
};

if ( process.env.NODE_ENV=== 'production' ) {
  nextConfig.output = 'export';
  nextConfig.distDir = '../../data/html/chat';
  // nextConfig.basePath = '/chat';
}

export default withNextIntl(nextConfig);
