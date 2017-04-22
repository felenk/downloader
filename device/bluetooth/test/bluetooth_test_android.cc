// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_android.h"

#include <iterator>
#include <sstream>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "device/bluetooth/android/wrappers.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_device_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "jni/Fakes_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace device {

BluetoothTestAndroid::BluetoothTestAndroid() {
}

BluetoothTestAndroid::~BluetoothTestAndroid() {
}

void BluetoothTestAndroid::SetUp() {
  // Register in SetUp so that ASSERT can be used.
  ASSERT_TRUE(RegisterNativesImpl(AttachCurrentThread()));
}

void BluetoothTestAndroid::TearDown() {
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (auto& device : devices) {
    DeleteDevice(device);
  }
  EXPECT_EQ(0, gatt_open_connections_);
  BluetoothTestBase::TearDown();
}

bool BluetoothTestAndroid::PlatformSupportsLowEnergy() {
  return true;
}

void BluetoothTestAndroid::InitWithDefaultAdapter() {
  adapter_ =
      BluetoothAdapterAndroid::Create(
          BluetoothAdapterWrapper_CreateWithDefaultAdapter().obj()).get();
}

void BluetoothTestAndroid::InitWithoutDefaultAdapter() {
  adapter_ = BluetoothAdapterAndroid::Create(NULL).get();
}

void BluetoothTestAndroid::InitWithFakeAdapter() {
  j_fake_bluetooth_adapter_.Reset(Java_FakeBluetoothAdapter_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));

  adapter_ =
      BluetoothAdapterAndroid::Create(j_fake_bluetooth_adapter_.obj()).get();
}

bool BluetoothTestAndroid::DenyPermission() {
  Java_FakeBluetoothAdapter_denyPermission(AttachCurrentThread(),
                                           j_fake_bluetooth_adapter_.obj());
  return true;
}

BluetoothDevice* BluetoothTestAndroid::DiscoverLowEnergyDevice(
    int device_ordinal) {
  TestBluetoothAdapterObserver observer(adapter_);
  Java_FakeBluetoothAdapter_discoverLowEnergyDevice(
      AttachCurrentThread(), j_fake_bluetooth_adapter_.obj(), device_ordinal);
  return observer.last_device();
}

void BluetoothTestAndroid::ForceIllegalStateException() {
  Java_FakeBluetoothAdapter_forceIllegalStateException(
      AttachCurrentThread(), j_fake_bluetooth_adapter_.obj());
}

void BluetoothTestAndroid::SimulateGattConnection(BluetoothDevice* device) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_connectionStateChange(
      AttachCurrentThread(), device_android->GetJavaObject().obj(),
      0,      // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      true);  // connected
}

void BluetoothTestAndroid::SimulateGattConnectionError(
    BluetoothDevice* device,
    BluetoothDevice::ConnectErrorCode) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_connectionStateChange(
      AttachCurrentThread(), device_android->GetJavaObject().obj(),
      // TODO(ortuno): Add all types of errors Android can produce. For now we
      // just return a timeout error.
      // http://crbug.com/578191
      0x08,    // Connection Timeout from Bluetooth Spec.
      false);  // connected
}

void BluetoothTestAndroid::SimulateGattDisconnection(BluetoothDevice* device) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_connectionStateChange(
      AttachCurrentThread(), device_android->GetJavaObject().obj(),
      0x13,    // Connection terminate by peer user from Bluetooth Spec.
      false);  // disconnected
}

void BluetoothTestAndroid::SimulateGattServicesDiscovered(
    BluetoothDevice* device,
    const std::vector<std::string>& uuids) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);
  JNIEnv* env = base::android::AttachCurrentThread();

  // Join UUID strings into a single string.
  std::ostringstream uuids_space_delimited;
  std::copy(uuids.begin(), uuids.end(),
            std::ostream_iterator<std::string>(uuids_space_delimited, " "));

  Java_FakeBluetoothDevice_servicesDiscovered(
      env, device_android->GetJavaObject().obj(),
      0,  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      base::android::ConvertUTF8ToJavaString(env, uuids_space_delimited.str())
          .obj());
}

void BluetoothTestAndroid::SimulateGattServicesDiscoveryError(
    BluetoothDevice* device) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_servicesDiscovered(
      AttachCurrentThread(), device_android->GetJavaObject().obj(),
      0x00000101,  // android.bluetooth.BluetoothGatt.GATT_FAILURE
      nullptr);
}

void BluetoothTestAndroid::SimulateGattCharacteristic(
    BluetoothGattService* service,
    const std::string& uuid,
    int properties) {
  BluetoothRemoteGattServiceAndroid* service_android =
      static_cast<BluetoothRemoteGattServiceAndroid*>(service);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattService_addCharacteristic(
      env, service_android->GetJavaObject().obj(),
      base::android::ConvertUTF8ToJavaString(env, uuid).obj(), properties);
}

void BluetoothTestAndroid::RememberCharacteristicForSubsequentAction(
    BluetoothGattCharacteristic* characteristic) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);

  Java_FakeBluetoothGattCharacteristic_rememberCharacteristicForSubsequentAction(
      base::android::AttachCurrentThread(),
      characteristic_android->GetJavaObject().obj());
}

void BluetoothTestAndroid::RememberCCCDescriptorForSubsequentAction(
    BluetoothGattCharacteristic* characteristic) {
  remembered_ccc_descriptor_ =
      characteristic
          ->GetDescriptorsByUUID(
              BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid())
          .at(0);
  DCHECK(remembered_ccc_descriptor_);
  RememberDescriptorForSubsequentAction(remembered_ccc_descriptor_);
}

void BluetoothTestAndroid::SimulateGattNotifySessionStarted(
    BluetoothGattCharacteristic* characteristic) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android = nullptr;
  if (characteristic) {
    descriptor_android = static_cast<BluetoothRemoteGattDescriptorAndroid*>(
        characteristic
            ->GetDescriptorsByUUID(BluetoothGattDescriptor::
                                       ClientCharacteristicConfigurationUuid())
            .at(0));
  }
  Java_FakeBluetoothGattDescriptor_valueWrite(
      base::android::AttachCurrentThread(),
      descriptor_android ? descriptor_android->GetJavaObject().obj() : nullptr,
      0);  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
}

void BluetoothTestAndroid::SimulateGattNotifySessionStartError(
    BluetoothGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android = nullptr;
  if (characteristic) {
    descriptor_android = static_cast<BluetoothRemoteGattDescriptorAndroid*>(
        characteristic
            ->GetDescriptorsByUUID(BluetoothGattDescriptor::
                                       ClientCharacteristicConfigurationUuid())
            .at(0));
  }
  Java_FakeBluetoothGattDescriptor_valueWrite(
      base::android::AttachCurrentThread(),
      descriptor_android ? descriptor_android->GetJavaObject().obj() : nullptr,
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code));
}

void BluetoothTestAndroid::
    SimulateGattCharacteristicSetNotifyWillFailSynchronouslyOnce(
        BluetoothGattCharacteristic* characteristic) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattCharacteristic_setCharacteristicNotificationWillFailSynchronouslyOnce(
      env, characteristic_android->GetJavaObject().obj());
}

void BluetoothTestAndroid::SimulateGattCharacteristicChanged(
    BluetoothGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattCharacteristic_valueChanged(
      env,
      characteristic_android ? characteristic_android->GetJavaObject().obj()
                             : nullptr,
      base::android::ToJavaByteArray(env, value).obj());
}

void BluetoothTestAndroid::SimulateGattCharacteristicRead(
    BluetoothGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattCharacteristic_valueRead(
      env,
      characteristic_android ? characteristic_android->GetJavaObject().obj()
                             : nullptr,
      0,  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      base::android::ToJavaByteArray(env, value).obj());
}

void BluetoothTestAndroid::SimulateGattCharacteristicReadError(
    BluetoothGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<uint8_t> empty_value;

  Java_FakeBluetoothGattCharacteristic_valueRead(
      env, characteristic_android->GetJavaObject().obj(),
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code),
      base::android::ToJavaByteArray(env, empty_value).obj());
}

void BluetoothTestAndroid::
    SimulateGattCharacteristicReadWillFailSynchronouslyOnce(
        BluetoothGattCharacteristic* characteristic) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattCharacteristic_setReadCharacteristicWillFailSynchronouslyOnce(
      env, characteristic_android->GetJavaObject().obj());
}

void BluetoothTestAndroid::SimulateGattCharacteristicWrite(
    BluetoothGattCharacteristic* characteristic) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  Java_FakeBluetoothGattCharacteristic_valueWrite(
      base::android::AttachCurrentThread(),
      characteristic_android ? characteristic_android->GetJavaObject().obj()
                             : nullptr,
      0);  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
}

void BluetoothTestAndroid::SimulateGattCharacteristicWriteError(
    BluetoothGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  Java_FakeBluetoothGattCharacteristic_valueWrite(
      base::android::AttachCurrentThread(),
      characteristic_android->GetJavaObject().obj(),
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code));
}

void BluetoothTestAndroid::
    SimulateGattCharacteristicWriteWillFailSynchronouslyOnce(
        BluetoothGattCharacteristic* characteristic) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  Java_FakeBluetoothGattCharacteristic_setWriteCharacteristicWillFailSynchronouslyOnce(
      base::android::AttachCurrentThread(),
      characteristic_android->GetJavaObject().obj());
}

void BluetoothTestAndroid::SimulateGattDescriptor(
    BluetoothGattCharacteristic* characteristic,
    const std::string& uuid) {
  BluetoothRemoteGattCharacteristicAndroid* characteristic_android =
      static_cast<BluetoothRemoteGattCharacteristicAndroid*>(characteristic);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattCharacteristic_addDescriptor(
      env, characteristic_android->GetJavaObject().obj(),
      base::android::ConvertUTF8ToJavaString(env, uuid).obj());
}

void BluetoothTestAndroid::RememberDescriptorForSubsequentAction(
    BluetoothGattDescriptor* descriptor) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);

  Java_FakeBluetoothGattDescriptor_rememberDescriptorForSubsequentAction(
      base::android::AttachCurrentThread(),
      descriptor_android->GetJavaObject().obj());
}

void BluetoothTestAndroid::SimulateGattDescriptorRead(
    BluetoothGattDescriptor* descriptor,
    const std::vector<uint8_t>& value) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattDescriptor_valueRead(
      env,
      descriptor_android ? descriptor_android->GetJavaObject().obj() : nullptr,
      0,  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      base::android::ToJavaByteArray(env, value).obj());
}

void BluetoothTestAndroid::SimulateGattDescriptorReadError(
    BluetoothGattDescriptor* descriptor,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<uint8_t> empty_value;

  Java_FakeBluetoothGattDescriptor_valueRead(
      env, descriptor_android->GetJavaObject().obj(),
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code),
      base::android::ToJavaByteArray(env, empty_value).obj());
}

void BluetoothTestAndroid::SimulateGattDescriptorReadWillFailSynchronouslyOnce(
    BluetoothGattDescriptor* descriptor) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_FakeBluetoothGattDescriptor_setReadDescriptorWillFailSynchronouslyOnce(
      env, descriptor_android->GetJavaObject().obj());
}

void BluetoothTestAndroid::SimulateGattDescriptorWrite(
    BluetoothGattDescriptor* descriptor) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  Java_FakeBluetoothGattDescriptor_valueWrite(
      base::android::AttachCurrentThread(),
      descriptor_android ? descriptor_android->GetJavaObject().obj() : nullptr,
      0);  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
}

void BluetoothTestAndroid::SimulateGattDescriptorWriteError(
    BluetoothGattDescriptor* descriptor,
    BluetoothGattService::GattErrorCode error_code) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  Java_FakeBluetoothGattDescriptor_valueWrite(
      base::android::AttachCurrentThread(),
      descriptor_android->GetJavaObject().obj(),
      BluetoothRemoteGattServiceAndroid::GetAndroidErrorCode(error_code));
}

void BluetoothTestAndroid::SimulateGattDescriptorWriteWillFailSynchronouslyOnce(
    BluetoothGattDescriptor* descriptor) {
  BluetoothRemoteGattDescriptorAndroid* descriptor_android =
      static_cast<BluetoothRemoteGattDescriptorAndroid*>(descriptor);
  Java_FakeBluetoothGattDescriptor_setWriteDescriptorWillFailSynchronouslyOnce(
      base::android::AttachCurrentThread(),
      descriptor_android->GetJavaObject().obj());
}

void BluetoothTestAndroid::OnFakeBluetoothDeviceConnectGattCalled(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  gatt_open_connections_++;
  gatt_connection_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattDisconnect(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  gatt_disconnection_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattClose(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  gatt_open_connections_--;

  // close implies disconnect
  gatt_disconnection_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattDiscoverServices(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  gatt_discovery_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattSetCharacteristicNotification(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  gatt_notify_characteristic_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattReadCharacteristic(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  gatt_read_characteristic_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattWriteCharacteristic(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jbyteArray>& value) {
  gatt_write_characteristic_attempts_++;
  base::android::JavaByteArrayToByteVector(env, value, &last_write_value_);
}

void BluetoothTestAndroid::OnFakeBluetoothGattReadDescriptor(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  gatt_read_descriptor_attempts_++;
}

void BluetoothTestAndroid::OnFakeBluetoothGattWriteDescriptor(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jbyteArray>& value) {
  gatt_write_descriptor_attempts_++;
  base::android::JavaByteArrayToByteVector(env, value, &last_write_value_);
}

void BluetoothTestAndroid::OnFakeAdapterStateChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const bool powered) {
  adapter_->NotifyAdapterStateChanged(powered);
}

}  // namespace device