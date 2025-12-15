#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

// Get build output directory
const buildDir = path.join(__dirname, '../../../data/html/chat');
const mockDir = path.join(buildDir, 'mock');

console.log('Checking for mock directory in:', buildDir);

if (fs.existsSync(mockDir)) {
  console.log('Removing mock directory:', mockDir);
  fs.rmSync(mockDir, { recursive: true, force: true });
  console.log('Mock directory removed successfully');
} else {
  console.log('Mock directory not found, nothing to remove');
}