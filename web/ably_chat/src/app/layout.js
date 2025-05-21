export const metadata = {
  title: 'Ably test',
  description: 'Ably test',
};

export default function RootLayout({ children }) {
  return (
    <html lang="en">
      <body>
        {children}
      </body>
    </html>
  );
}
