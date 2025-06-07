import createNextIntlPlugin from 'next-intl/plugin';

const withNextIntl = createNextIntlPlugin({
  requestConfig: './src/i18n/request.js',
  experimental: {
    createMessagesDeclaration: './messages/en.json'
  }
});

/** @type {import('next').NextConfig} */
const nextConfig = {};

if ( process.env.NODE_ENV=== 'production' ) {
  nextConfig.output = 'export';
  nextConfig.distDir = '../../data/html/chat';
  // nextConfig.basePath = '/chat';
}

export default withNextIntl(nextConfig);
