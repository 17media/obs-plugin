"use client";
import React, { useState, useEffect } from 'react';
import { Settings, Play, Pause, RefreshCw, AlertCircle } from 'lucide-react';

/**
 * 平台选择器组件
 * 允许用户选择和管理多个平台连接
 */

export const PlatformSelector = ({ onPlatformChange, messageAggregator }) => {
  const [platforms, setPlatforms] = useState({});
  const [isConnecting, setIsConnecting] = useState({});
  const [platformConfigs, setPlatformConfigs] = useState({
    '17live': {
      enabled: false,
      config: {}
    },
    'youtube': {
      enabled: false,
      config: {}
    },
    'twitch': {
      enabled: false,
      config: {}
    }
  });
  
  const [showConfig, setShowConfig] = useState({});
  const [errors, setErrors] = useState({});

  // 监听平台状态变化
  useEffect(() => {
    if (!messageAggregator) return;

    const handleStatusChange = ({ platformId, status }) => {
      setPlatforms(prev => ({
        ...prev,
        [platformId]: {
          ...prev[platformId],
          status
        }
      }));
      setIsConnecting(prev => ({
        ...prev,
        [platformId]: false
      }));
    };

    const handlePlatformError = ({ platformId, error }) => {
      setErrors(prev => ({
        ...prev,
        [platformId]: error.message || '连接失败'
      }));
      setIsConnecting(prev => ({
        ...prev,
        [platformId]: false
      }));
    };

    messageAggregator.on('status_change', handleStatusChange);
    messageAggregator.on('platform_error', handlePlatformError);

    return () => {
      messageAggregator.off('status_change', handleStatusChange);
      messageAggregator.off('platform_error', handlePlatformError);
    };
  }, [messageAggregator]);

  // 平台配置
  const platformDefinitions = {
    '17live': {
      name: '17LIVE',
      description: '17LIVE直播平台',
      color: 'bg-gradient-to-r from-pink-500 to-rose-500',
      icon: '🎥',
      configFields: []
    },
    'youtube': {
      name: 'YouTube',
      description: 'YouTube直播聊天',
      color: 'bg-gradient-to-r from-red-500 to-red-600',
      icon: '📺',
      configFields: [
        { key: 'apiKey', label: 'API密钥', type: 'password', required: true },
        { key: 'liveChatId', label: '直播聊天ID', type: 'text', required: true }
      ]
    },
    'twitch': {
      name: 'Twitch',
      description: 'Twitch聊天',
      color: 'bg-gradient-to-r from-purple-500 to-purple-600',
      icon: '🎮',
      configFields: [
        { key: 'channel', label: '频道名称', type: 'text', required: true },
        { key: 'username', label: '用户名（可选）', type: 'text', required: false },
        { key: 'oauth', label: 'OAuth令牌（可选）', type: 'password', required: false }
      ]
    }
  };

  // 切换平台启用状态
  const togglePlatform = async (platformId) => {
    const platform = platformDefinitions[platformId];
    const currentConfig = platformConfigs[platformId];
    
    if (!currentConfig.enabled) {
      // 启用平台
      if (platform.configFields.length > 0) {
        // 需要配置，显示配置界面
        setShowConfig(prev => ({
          ...prev,
          [platformId]: true
        }));
        return;
      } else {
        // 直接启用
        await connectPlatform(platformId, {});
      }
    } else {
      // 禁用平台
      await disconnectPlatform(platformId);
    }
  };

  // 连接平台
  const connectPlatform = async (platformId, config) => {
    if (!messageAggregator) return;

    setIsConnecting(prev => ({
      ...prev,
      [platformId]: true
    }));
    
    setErrors(prev => ({
      ...prev,
      [platformId]: null
    }));

    try {
      // 添加平台到聚合器
      await messageAggregator.addPlatform(platformId, config);

      // 无论是否需要配置，都尝试连接（17LIVE无需配置，其他平台需提供配置）
      await messageAggregator.connectPlatform(platformId, config);

      // 更新配置状态
      setPlatformConfigs(prev => ({
        ...prev,
        [platformId]: {
          ...prev[platformId],
          enabled: true,
          config
        }
      }));

      // 隐藏配置界面
      setShowConfig(prev => ({
        ...prev,
        [platformId]: false
      }));

      // 通知父组件
      if (onPlatformChange) {
        onPlatformChange(platformId, true);
      }

    } catch (error) {
      console.error(`连接平台 ${platformId} 失败:`, error);
      setErrors(prev => ({
        ...prev,
        [platformId]: error.message
      }));
    } finally {
      setIsConnecting(prev => ({
        ...prev,
        [platformId]: false
      }));
    }
  };

  // 断开平台连接
  const disconnectPlatform = async (platformId) => {
    if (!messageAggregator) return;

    try {
      await messageAggregator.disconnectPlatform(platformId);
      
      setPlatformConfigs(prev => ({
        ...prev,
        [platformId]: {
          ...prev[platformId],
          enabled: false
        }
      }));

      if (onPlatformChange) {
        onPlatformChange(platformId, false);
      }

    } catch (error) {
      console.error(`断开平台 ${platformId} 失败:`, error);
    }
  };

  // 获取平台状态显示
  const getPlatformStatus = (platformId) => {
    const platform = platforms[platformId];
    const isEnabled = platformConfigs[platformId].enabled;
    const isConnectingNow = isConnecting[platformId];
    const error = errors[platformId];

    if (error) {
      return { text: '连接失败', color: 'text-red-500', icon: <AlertCircle className="w-4 h-4" /> };
    }
    
    if (isConnectingNow) {
      return { text: '连接中...', color: 'text-yellow-500', icon: <RefreshCw className="w-4 h-4 animate-spin" /> };
    }
    
    if (!isEnabled) {
      return { text: '未启用', color: 'text-gray-400', icon: null };
    }
    
    if (platform?.status === 'connected') {
      return { text: '已连接', color: 'text-green-500', icon: <Play className="w-4 h-4" /> };
    }
    
    return { text: '未连接', color: 'text-gray-400', icon: <Pause className="w-4 h-4" /> };
  };

  // 配置表单提交
  const handleConfigSubmit = (platformId, formData) => {
    const config = {};
    platformDefinitions[platformId].configFields.forEach(field => {
      config[field.key] = formData.get(field.key);
    });
    
    connectPlatform(platformId, config);
  };

  return (
    <div className="platform-selector bg-white dark:bg-gray-800 rounded-lg shadow-lg p-6">
      <div className="flex items-center justify-between mb-6">
        <h3 className="text-lg font-semibold text-gray-800 dark:text-gray-200">
          平台管理
        </h3>
        <Settings className="w-5 h-5 text-gray-400" />
      </div>

      <div className="space-y-4">
        {Object.entries(platformDefinitions).map(([platformId, platform]) => {
          const isEnabled = platformConfigs[platformId].enabled;
          const status = getPlatformStatus(platformId);
          const showConfigForm = showConfig[platformId];

          return (
            <div key={platformId} className="platform-card border border-gray-200 dark:border-gray-700 rounded-lg p-4">
              <div className="flex items-center justify-between">
                <div className="flex items-center space-x-3">
                  <div className={`w-10 h-10 rounded-lg ${platform.color} flex items-center justify-center text-white text-lg`}>
                    {platform.icon}
                  </div>
                  <div>
                    <h4 className="font-medium text-gray-800 dark:text-gray-200">
                      {platform.name}
                    </h4>
                    <p className="text-sm text-gray-500 dark:text-gray-400">
                      {platform.description}
                    </p>
                  </div>
                </div>

                <div className="flex items-center space-x-3">
                  <div className={`flex items-center space-x-1 ${status.color}`}>
                    {status.icon}
                    <span className="text-sm">{status.text}</span>
                  </div>

                  <button
                    onClick={() => togglePlatform(platformId)}
                    className={`px-3 py-1 rounded text-sm font-medium transition-colors ${
                      isEnabled
                        ? 'bg-red-500 hover:bg-red-600 text-white'
                        : 'bg-green-500 hover:bg-green-600 text-white'
                    }`}
                    disabled={isConnecting[platformId]}
                  >
                    {isEnabled ? '断开' : '连接'}
                  </button>
                </div>
              </div>

              {/* 配置表单 */}
              {showConfigForm && platform.configFields.length > 0 && (
                <div className="mt-4 p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
                  <h5 className="font-medium mb-3 text-gray-800 dark:text-gray-200">
                    配置 {platform.name}
                  </h5>
                  <form
                    onSubmit={(e) => {
                      e.preventDefault();
                      handleConfigSubmit(platformId, new FormData(e.target));
                    }}
                    className="space-y-3"
                  >
                    {platform.configFields.map(field => (
                      <div key={field.key}>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
                          {field.label}
                          {field.required && <span className="text-red-500 ml-1">*</span>}
                        </label>
                        <input
                          type={field.type}
                          name={field.key}
                          required={field.required}
                          className="w-full px-3 py-2 border border-gray-300 dark:border-gray-600 rounded-md bg-white dark:bg-gray-800 text-gray-900 dark:text-gray-100 focus:outline-none focus:ring-2 focus:ring-blue-500"
                          placeholder={`输入${field.label}...`}
                        />
                      </div>
                    ))}
                    
                    <div className="flex space-x-2">
                      <button
                        type="submit"
                        className="px-4 py-2 bg-blue-500 hover:bg-blue-600 text-white rounded text-sm font-medium transition-colors"
                        disabled={isConnecting[platformId]}
                      >
                        {isConnecting[platformId] ? '连接中...' : '连接'}
                      </button>
                      <button
                        type="button"
                        onClick={() => setShowConfig(prev => ({ ...prev, [platformId]: false }))}
                        className="px-4 py-2 bg-gray-500 hover:bg-gray-600 text-white rounded text-sm font-medium transition-colors"
                      >
                        取消
                      </button>
                    </div>
                  </form>
                  
                  {errors[platformId] && (
                    <div className="mt-2 p-2 bg-red-50 dark:bg-red-900/20 border border-red-200 dark:border-red-800 rounded text-red-600 dark:text-red-400 text-sm">
                      {errors[platformId]}
                    </div>
                  )}
                </div>
              )}
            </div>
          );
        })}
      </div>

      <div className="mt-6 p-4 bg-blue-50 dark:bg-blue-900/20 rounded-lg">
        <div className="flex items-start space-x-2">
          <AlertCircle className="w-4 h-4 text-blue-500 mt-0.5" />
          <div className="text-sm text-blue-600 dark:text-blue-400">
            <p className="font-medium mb-1">使用提示：</p>
            <ul className="space-y-1 text-xs">
              <li>• 17LIVE: 自动连接，无需额外配置</li>
              <li>• YouTube: 需要API密钥和直播聊天ID</li>
              <li>• Twitch: 需要频道名称，可选OAuth令牌</li>
            </ul>
          </div>
        </div>
      </div>
    </div>
  );
};

export default PlatformSelector;