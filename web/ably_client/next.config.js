/** @type {import('next').NextConfig} */
const nextConfig = {
  reactStrictMode: true
};

if (process.env.NODE_ENV === 'production') {
  nextConfig.output = 'export';
  nextConfig.distDir = '../../data/html/ably';
}

module.exports = nextConfig;
