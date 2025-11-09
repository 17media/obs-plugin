import { NextIntlClientProvider, hasLocale } from 'next-intl';
import {setRequestLocale} from 'next-intl/server';
import { notFound } from 'next/navigation';
import { routing } from '@/i18n/routing';
import { NextUIProvider } from '@nextui-org/react';
import { HeroUIProvider } from '@heroui/react'
import "@/styles/globals.css";

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
    <html lang={locale} className="dark">
      <body style={{backgroundColor: 'black'}}>
        <NextIntlClientProvider>
          <NextUIProvider>
            <HeroUIProvider>
              {children}
            </HeroUIProvider>
          </NextUIProvider>
        </NextIntlClientProvider>
      </body>
    </html>
  );
}
