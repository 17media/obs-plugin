export const metadata = {
  title: '17Live Chatroom',
  description: 'chatroom for 17Live',
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
