import Ably from './Ably';
import {Locale, useTranslations} from 'next-intl';
import {setRequestLocale} from 'next-intl/server';
export default async function Home({ params } ) {
  const { locale } = await params;
  
  return (
      <div>
        <Ably locale={locale} />
      </div>
  );
}
