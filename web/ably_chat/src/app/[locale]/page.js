import Ably from './Ably';
import {setRequestLocale} from 'next-intl/server';
export default function Home({params: {locale}}) {
  setRequestLocale(locale);
  
  return (
      <div>
        <Ably />
      </div>
  );
}
