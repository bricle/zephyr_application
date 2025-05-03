const app = getApp()

function inArray(arr, key, val) {
  for (let i = 0; i < arr.length; i++) {
    if (arr[i][key] === val) {
      return i;
    }
  }
  return -1;
}

// ArrayBuffer转16进度字符串示例
function ab2hex(buffer) {
  var hexArr = Array.prototype.map.call(
    new Uint8Array(buffer),
    function (bit) {
      return ('00' + bit.toString(16)).slice(-2)
    }
  )
  return hexArr.join('');
}

Page({
  data: {
    devices: [], // 添加设备数组初始化
    deviceId: '',
    deviceName: '',
    connected: false,
    serviceId: '',
    characteristicId: '',
    sensorValue: 0,  // 添加传感器数值
    displayText: '', // 添加显示文本状态
    rxCharacteristicId: '' // 添加接收特征值ID
  },

  // 扫描并连接设备
  scanDevice() {
    this.clearDevices(); // 清理之前的设备列表
    // 检查蓝牙是否可用
    wx.openBluetoothAdapter({
      success: (res) => {
        console.log('蓝牙适配器初始化成功');
        this.startBluetoothDevicesDiscovery();
      },
      fail: (res) => {
        console.log('蓝牙适配器初始化失败', res);
        if (res.errCode === 10001) {
          wx.showModal({
            title: '提示',
            content: '请打开手机蓝牙'
          });
        }
      }
    });
  },
  startBluetoothDevicesDiscovery() {
    wx.startBluetoothDevicesDiscovery({
      allowDuplicatesKey: false,
      success: (res) => {
        console.log('开始搜索蓝牙设备');
        this.onBluetoothDeviceFound();
      },
      fail: (res) => {
        console.log('搜索蓝牙设备失败', res);
      }
    });
  },
  onBluetoothDeviceFound() {
    wx.onBluetoothDeviceFound((res) => {
      console.log('发现新设备:', res);
      const newDevice = res.devices[0]; // 获取发现的设备
      
      // 检查设备是否已存在
      const idx = this.data.devices.findIndex(device => device.deviceId === newDevice.deviceId);
      
      if (idx === -1) {
        console.log('新设备:', newDevice);
        // 将新设备添加到设备列表
        this.setData({
          devices: [...this.data.devices, newDevice]
        });
      }
    });
  },
  // 连接设备
  connectDevice() {
    wx.createBLEConnection({
      deviceId: this.data.deviceId,
      success: () => {
        console.log('蓝牙连接成功');
        this.setData({ connected: true });
        // 获取服务
        wx.getBLEDeviceServices({
          deviceId: this.data.deviceId,
          success: (res) => {
            console.log('获取服务成功:', res.services);
            for (let service of res.services) {
              if (service.uuid.toLowerCase().includes('1532')) {
                this.setData({ serviceId: service.uuid });
                // 获取特征值
                wx.getBLEDeviceCharacteristics({
                  deviceId: this.data.deviceId,
                  serviceId: this.data.serviceId,
                  success: (res) => {
                    console.log('获取特征值成功:', res.characteristics);
                    for (let char of res.characteristics) {
                      if (char.uuid.toLowerCase().includes('1534')) {
                        this.setData({ characteristicId: char.uuid });
                      }
                      // 添加对传感器特征值的处理
                      if (char.uuid.toLowerCase().includes('1535')) {
                        // 启用传感器数据通知
                        wx.notifyBLECharacteristicValueChange({
                          deviceId: this.data.deviceId,
                          serviceId: this.data.serviceId,
                          characteristicId: char.uuid,
                          state: true,
                          success: () => {
                            console.log('光照传感器通知启用成功');
                            // 监听传感器数据
                            wx.onBLECharacteristicValueChange((result) => {
                              if(result.characteristicId.toLowerCase().includes('1535')) {
                                const value = new DataView(result.value).getUint32(0, true);
                                this.setData({
                                  sensorValue: value
                                });
                              }
                            });
                          },
                          fail: (error) => console.log('启用传感器通知失败:', error)
                        });
                      }
                      // 添加对RX特征值的处理
                      if (char.uuid.toLowerCase().includes('1537')) {
                        console.log('找到RX特征值:', char.uuid);
                        console.log('RX特征值完整属性:', char);
                        // 检查写入权限
                        if (char.properties.write || char.properties.writeWithoutResponse) {
                          console.log('RX特征值具有写入权限');
                          this.setData({
                            rxCharacteristicId: char.uuid
                          });
                        } else {
                          console.log('RX特征值不具备写入权限');
                        }
                      }
                    }
                  },
                  fail: (res) => console.log('获取特征值失败:', res)
                });
              }
            }
          },
          fail: (res) => console.log('获取服务失败:', res)
        });
      },
      fail: (res) => {
        console.log('蓝牙连接失败:', res);
        wx.showToast({
          title: '连接失败',
          icon: 'none'
        });
      }
    });
  },

  // LED控制
  turnOnLED() {
    const buffer = new ArrayBuffer(1)
    const dataView = new DataView(buffer)
    dataView.setUint8(0, 1)
    wx.writeBLECharacteristicValue({
      deviceId: this.data.deviceId,
      serviceId: this.data.serviceId, 
      characteristicId: this.data.characteristicId,
      value: buffer
    })
  },

  turnOffLED() {
    const buffer = new ArrayBuffer(1)
    const dataView = new DataView(buffer)
    dataView.setUint8(0, 0)
    wx.writeBLECharacteristicValue({
      deviceId: this.data.deviceId,
      serviceId: this.data.serviceId,
      characteristicId: this.data.characteristicId, 
      value: buffer
    })
  },

  // 添加清理设备列表的方法
  clearDevices() {
    this.setData({
      devices: []
    });
  },

  // 添加处理设备点击的方法
  createBLEConnection(e) {
    const deviceId = e.currentTarget.dataset.deviceId;
    const deviceName = e.currentTarget.dataset.name;
    
    console.log('尝试连接设备:', deviceId, deviceName);
    
    // 停止搜索
    wx.stopBluetoothDevicesDiscovery();
    
    this.setData({
      deviceId: deviceId,
      deviceName: deviceName
    });
    
    // 调用已有的连接方法
    this.connectDevice();
  },

  // 处理文本输入
  onTextInput(e) {
    this.setData({
      displayText: e.detail.value
    });
  },

  // 发送显示文本
  sendDisplayText() {
    if (!this.data.displayText) {
      wx.showToast({
        title: '请输入文字',
        icon: 'none'
      });
      return;
    }

    if (!this.data.rxCharacteristicId) {
      console.log('RX特征值未找到');
      wx.showToast({
        title: 'RX特征值未找到',
        icon: 'none'
      });
      return;
    }

    console.log('准备发送文本:', this.data.displayText);
    console.log('使用特征值:', this.data.rxCharacteristicId);
    console.log('服务ID:', this.data.serviceId);
    
    const str = this.data.displayText;
    const buffer = new ArrayBuffer(str.length + 1);
    const dataView = new DataView(buffer);
    
    for (let i = 0; i < str.length; i++) {
      dataView.setUint8(i, str.charCodeAt(i));
    }
    // 添加字符串结束符
    dataView.setUint8(str.length, 0);
    
    console.log('编码后的数据:', new Uint8Array(buffer));

    // 移除 writeType 参数，使用默认写入方式
    wx.writeBLECharacteristicValue({
      deviceId: this.data.deviceId,
      serviceId: this.data.serviceId,
      characteristicId: this.data.rxCharacteristicId,
      value: buffer,
      success: () => {
        console.log('发送显示文本成功');
        wx.showToast({
          title: '发送成功',
          icon: 'success'
        });
      },
      fail: (res) => {
        console.log('发送显示文本失败:', res);
        // 直接重试发送
        setTimeout(() => {
          wx.writeBLECharacteristicValue({
            deviceId: this.data.deviceId,
            serviceId: this.data.serviceId,
            characteristicId: this.data.rxCharacteristicId,
            value: buffer,
            success: () => {
              console.log('重试发送成功');
              wx.showToast({
                title: '发送成功',
                icon: 'success'
              });
            },
            fail: (error) => {
              console.log('重试发送也失败:', error);
              // wx.showToast({
              //   title: '发送失败，请重试',
              //   icon: 'none'
              // });
            }
          });
        }, 200);
      }
    });
  },
})
