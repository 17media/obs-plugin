export async function loadDevMockMessages() {
  const modules = await Promise.all([
    import('@/../public/mock/chat_message.json'),
    import('@/../public/mock/chat_new_join.json'),
    import('@/../public/mock/chat_new_gift_2.json'),
    import('@/../public/mock/chat_labor_receive_reward.json'),
    import('@/../public/mock/chat_react_like.json'),
    import('@/../public/mock/chat_ai_cohost.json'),
    import('@/../public/mock/chat_poke.json'),
    import('@/../public/mock/chat_poke_all.json'),
    import('@/../public/mock/chat_poke_back_0.json'),
    import('@/../public/mock/chat_poke_back_1.json'),
    import('@/../public/mock/chat_poke_back_2.json'),
    import('@/../public/mock/chat_poke_back_3.json'),
  ]);

  return modules.flatMap((m) => (Array.isArray(m.default) ? m.default : [m.default]));
}

export async function loadDevEnterAnimationMessages() {
  const m = await import('@/../public/mock/chat_enter_animation_samples.json');
  return Array.isArray(m.default) ? m.default : [];
}
