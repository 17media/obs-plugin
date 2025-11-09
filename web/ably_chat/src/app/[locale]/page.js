import MultiPlatformChat from '@/components/MultiPlatformChat';
import {setRequestLocale} from 'next-intl/server';

export default async function Home({params}) {
  const { locale } = await params;
  
  setRequestLocale(locale);
  
  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900">
      <MultiPlatformChat />
    </div>
  );
}
