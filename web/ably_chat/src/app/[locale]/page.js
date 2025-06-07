import Ably from './Ably';
import {setRequestLocale} from 'next-intl/server';
export default async function Home({params}) {
  const { locale } = await params;
  
  setRequestLocale(locale);
  
  return (
      <div>
        <Ably />
      </div>
  );
}
