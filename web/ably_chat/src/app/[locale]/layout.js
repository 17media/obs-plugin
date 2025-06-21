import { NextIntlClientProvider, hasLocale } from 'next-intl';
import {setRequestLocale} from 'next-intl/server';
import { notFound } from 'next/navigation';
import { routing } from '@/i18n/routing';

export const metadata = {
  title: '17Live Chatroom',
  description: 'chatroom for 17Live',
};

export function generateStaticParams() {
  return routing.locales.map((locale) => ({locale}));
}
export default async function RootLayout({ children, params }) {
  const { locale } = await params;
  
  if (!hasLocale(routing.locales, locale)) {
    notFound();
  }

  setRequestLocale(locale);

  return (
    <html lang="{locale}">
      <body style={{backgroundColor: 'black'}}>
        <NextIntlClientProvider>
          {children}
        </NextIntlClientProvider>
      </body>
    </html>
  );
}
