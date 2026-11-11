const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const fs = require('node:fs');
const { createRequire } = require('node:module');
const path = require('node:path');

const root = path.resolve(__dirname, '..');
const appRequire = createRequire(path.join(root, 'web/ably_chat/package.json'));
const swc = appRequire('@swc/core');

class BasePlatform extends EventEmitter {
  constructor(id) {
    super();
    this.platformId = id;
    this.isConnected = false;
  }
}

function loadModule(file, dependencies) {
  const source = fs.readFileSync(path.join(root, file), 'utf8');
  const { code } = swc.transformSync(source, {
    jsc: { parser: { syntax: 'ecmascript' }, target: 'es2020' },
    module: { type: 'commonjs' },
  });
  const module = { exports: {} };
  const mockedRequire = name => {
    if (Object.hasOwn(dependencies, name)) return dependencies[name];
    throw new Error(`Unexpected dependency: ${name}`);
  };
  new Function('module', 'exports', 'require', code)(module, module.exports, mockedRequire);
  return module.exports;
}

async function main() {
  const emptyAsync = async () => ({});
  const { OneSevenLivePlatform } = loadModule(
    'web/ably_chat/src/platforms/17live/core/OneSevenLivePlatform.js',
    {
      '../../BasePlatform': { BasePlatform },
      nanoid: { nanoid: () => 'test' },
      immutable: { fromJS: value => value },
      '@/lib/constants': {},
      '@/lib/webpDuration': {},
      '../api': {
        getRoomInfo: emptyAsync,
        getEnterAnimationFiles: emptyAsync,
        getI18nConfig: emptyAsync,
      },
    }
  );
  const { MessageAggregator, messageAggregator } = loadModule('web/ably_chat/src/services/MessageAggregator.js', {
    events: { EventEmitter },
    immutable: {},
    '../platforms/17live/core/OneSevenLivePlatform': { OneSevenLivePlatform },
    '../platforms/twitch/core/TwitchPlatform': {},
    '../platforms/youtube/core/YouTubePlatform': {},
    './WSSender': {},
    '@/lib/features': { ENABLE_YOUTUBE: false },
    '@/lib/constants': {},
  });
  clearInterval(messageAggregator.processTimer);

  const platform = new OneSevenLivePlatform();
  const aggregator = Object.create(MessageAggregator.prototype);
  aggregator.activePlatforms = new Set();
  aggregator.platforms = new Map([['17live', platform]]);
  aggregator.emit = () => {};
  platform.on('connected', data => aggregator.handlePlatformConnected({ platformId: '17live', data }));
  let disconnectEvent;
  platform.on('disconnected', data => {
    disconnectEvent = data;
    aggregator.handlePlatformDisconnected({ platformId: '17live', data });
  });

  await aggregator.connectPlatform('17live', { roomID: '42', userID: '7' });
  assert.equal(platform.isConnected, false);
  assert.equal(aggregator.activePlatforms.has('17live'), false);

  platform.handleWsMessage({ type: 'ably_chat_connected', payload: { status: 'connected' } });
  assert.equal(platform.isConnected, true);
  assert.equal(aggregator.activePlatforms.has('17live'), true);
  platform.handleWsMessage({
    type: 'ably_chat_connected',
    payload: { status: 'break', errorCode: 40160 },
  });
  assert.equal(disconnectEvent.errorCode, 40160);
  assert.equal(aggregator.activePlatforms.has('17live'), false);
  platform.handleWsMessage({ type: 'ably_chat_connected', payload: { status: 'connected' } });
  assert.equal(aggregator.activePlatforms.has('17live'), true);
  console.log('Ably frontend state tests passed');
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
