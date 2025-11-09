'use client';
import styled from 'styled-components';
import MultiPlatformChat from '@/components/MultiPlatformChat';

const PageContainer = styled.div`
  min-height: 100vh;
  background-color: #f9fafb;
  
  &.dark {
    background-color: #111827;
  }
`;

export default function Home({params}) {
  return (
    <PageContainer>
      <MultiPlatformChat />
    </PageContainer>
  );
}
