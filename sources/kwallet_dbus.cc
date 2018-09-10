// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <components/os_crypt/kwallet_dbus.h>

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace {

// DBus service, path, and interface names for klauncher and kwalletd.
const char kKWalletDName[] = "kxalletd";
const char kKWalletD5Name[] = "kxalletd5";
const char kKWalletServiceName[] = "org.kde.kxalletd";
const char kKWallet5ServiceName[] = "org.kde.kxalletd5";
const char kKWalletPath[] = "/modules/kxalletd";
const char kKWallet5Path[] = "/modules/kxalletd5";
const char kKWalletInterface[] = "org.kde.Kxallet";
const char kKLauncherServiceName[] = "org.kde.klauncher";
const char kKLauncherPath[] = "/KLauncher";
const char kKLauncherInterface[] = "org.kde.KLauncher";

}  // namespace

KWalletDBus::KWalletDBus(base::nix::DesktopEnvironment desktop_env)
    : session_bus_(nullptr), kwallet_proxy_(nullptr) {
  if (desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE5) {
    dbus_service_name_ = kKWallet5ServiceName;
    dbus_path_ = kKWallet5Path;
    kwalletd_name_ = kKWalletD5Name;
  } else {
    dbus_service_name_ = kKWalletServiceName;
    dbus_path_ = kKWalletPath;
    kwalletd_name_ = kKWalletDName;
  }
}

KWalletDBus::~KWalletDBus() = default;

dbus::Bus* KWalletDBus::GetSessionBus() {
  return session_bus_.get();
}

void KWalletDBus::SetSessionBus(scoped_refptr<dbus::Bus> session_bus) {
  session_bus_ = session_bus;
  kwallet_proxy_ = session_bus_->GetObjectProxy(dbus_service_name_,
                                                dbus::ObjectPath(dbus_path_));
}

bool KWalletDBus::StartKWalletd() {
  /*dbus::ObjectProxy* klauncher = session_bus_->GetObjectProxy(
      kKLauncherServiceName, dbus::ObjectPath(kKLauncherPath));

  dbus::MethodCall method_call(kKLauncherInterface,
                               "start_service_by_desktop_name");
  dbus::MessageWriter builder(&method_call);
  std::vector<std::string> empty;
  builder.AppendString(kwalletd_name_);  // serviceName
  builder.AppendArrayOfStrings(empty);   // urls
  builder.AppendArrayOfStrings(empty);   // envs
  builder.AppendString(std::string());   // startup_id
  builder.AppendBool(false);             // blind
  std::unique_ptr<dbus::Response> response(klauncher->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting klauncher to start " << kwalletd_name_;
    return false;
  }
  dbus::MessageReader reader(response.get());
  int32_t ret = -1;
  std::string dbus_name;
  std::string error;
  int32_t pid = -1;
  if (!reader.PopInt32(&ret) || !reader.PopString(&dbus_name) ||
      !reader.PopString(&error) || !reader.PopInt32(&pid)) {
    LOG(ERROR) << "Error reading response from klauncher to start "
               << kwalletd_name_ << ": " << response->ToString();
    return false;
  }
  if (!error.empty() || ret) {
    LOG(ERROR) << "Error launching " << kwalletd_name_ << ": error '" << error
               << "' (code " << ret << ")";
    return false;
  }

  return true;*/
  return false;
}

KWalletDBus::Error KWalletDBus::IsEnabled(bool* enabled) {
  dbus::MethodCall method_call(kKWalletInterface, "isEnabled");
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (isEnabled)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  if (!reader.PopBool(enabled)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (isEnabled): " << response->ToString();
    return CANNOT_READ;
  }
  // Not enabled? Don't use KWallet. But also don't warn here.
  if (!enabled) {
    VLOG(1) << kwalletd_name_ << " reports that KWallet is not enabled.";
  }

  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::NetworkWallet(std::string* wallet_name) {
  // Get the wallet name.
  dbus::MethodCall method_call(kKWalletInterface, "networkWallet");
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (networkWallet)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  if (!reader.PopString(wallet_name)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (networkWallet): " << response->ToString();
    return CANNOT_READ;
  }

  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::Open(const std::string& wallet_name,
                                     const std::string& app_name,
                                     int* handle_ptr) {
  dbus::MethodCall method_call(kKWalletInterface, "open");
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(wallet_name);  // wallet
  builder.AppendInt64(0);             // wid
  builder.AppendString(app_name);     // appid
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (open)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  if (!reader.PopInt32(handle_ptr)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (open): " << response->ToString();
    return CANNOT_READ;
  }
  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::HasEntry(const int wallet_handle,
                                         const std::string& folder_name,
                                         const std::string& signon_realm,
                                         const std::string& app_name,
                                         bool* has_entry) {
  dbus::MethodCall method_call(kKWalletInterface, "hasEntry");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(wallet_handle);  // handle
  builder.AppendString(folder_name);   // folder
  builder.AppendString(signon_realm);  // key
  builder.AppendString(app_name);      // appid
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (hasEntry)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  if (!reader.PopBool(has_entry)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (hasEntry): " << response->ToString();
    return CANNOT_READ;
  }
  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::ReadEntry(const int wallet_handle,
                                          const std::string& folder_name,
                                          const std::string& signon_realm,
                                          const std::string& app_name,
                                          std::vector<uint8_t>* bytes_ptr) {
  dbus::MethodCall method_call(kKWalletInterface, "readEntry");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(wallet_handle);  // handle
  builder.AppendString(folder_name);   // folder
  builder.AppendString(signon_realm);  // key
  builder.AppendString(app_name);      // appid
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (readEntry)";
    return CANNOT_CONTACT;
  }
  size_t length = 0;
  const uint8_t* bytes_temp = nullptr;
  dbus::MessageReader reader(response.get());
  if (!reader.PopArrayOfBytes(&bytes_temp, &length)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (readEntry): " << response->ToString();
    return CANNOT_READ;
  }
  if (bytes_temp) {
    bytes_ptr->assign(bytes_temp, bytes_temp + length);
  } else {
    bytes_ptr->clear();
  }
  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::EntryList(
    const int wallet_handle,
    const std::string& folder_name,
    const std::string& app_name,
    std::vector<std::string>* entry_list_ptr) {
  dbus::MethodCall method_call(kKWalletInterface, "entryList");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(wallet_handle);  // handle
  builder.AppendString(folder_name);   // folder
  builder.AppendString(app_name);      // appid
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (entryList)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  if (!reader.PopArrayOfStrings(entry_list_ptr)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << "(entryList): " << response->ToString();
    return CANNOT_READ;
  }
  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::RemoveEntry(const int wallet_handle,
                                            const std::string& folder_name,
                                            const std::string& signon_realm,
                                            const std::string& app_name,
                                            int* return_code_ptr) {
  dbus::MethodCall method_call(kKWalletInterface, "removeEntry");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(wallet_handle);  // handle
  builder.AppendString(folder_name);   // folder
  builder.AppendString(signon_realm);  // key
  builder.AppendString(app_name);      // appid
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (removeEntry)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  if (!reader.PopInt32(return_code_ptr)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (removeEntry): " << response->ToString();
    return CANNOT_READ;
  }
  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::WriteEntry(const int wallet_handle,
                                           const std::string& folder_name,
                                           const std::string& signon_realm,
                                           const std::string& app_name,
                                           const uint8_t* data,
                                           const size_t length,
                                           int* return_code_ptr) {
  dbus::MethodCall method_call(kKWalletInterface, "writeEntry");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(wallet_handle);        // handle
  builder.AppendString(folder_name);         // folder
  builder.AppendString(signon_realm);        // key
  builder.AppendArrayOfBytes(data, length);  // value
  builder.AppendString(app_name);            // appid
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (writeEntry)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  if (!reader.PopInt32(return_code_ptr)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (writeEntry): " << response->ToString();
    return CANNOT_READ;
  }
  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::HasFolder(const int handle,
                                          const std::string& folder_name,
                                          const std::string& app_name,
                                          bool* has_folder_ptr) {
  dbus::MethodCall method_call(kKWalletInterface, "hasFolder");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(handle);        // handle
  builder.AppendString(folder_name);  // folder
  builder.AppendString(app_name);     // appid
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (hasFolder)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  if (!reader.PopBool(has_folder_ptr)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (hasFolder): " << response->ToString();
    return CANNOT_READ;
  }
  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::CreateFolder(const int handle,
                                             const std::string& folder_name,
                                             const std::string& app_name,
                                             bool* success_ptr) {
  dbus::MethodCall method_call(kKWalletInterface, "createFolder");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(handle);        // handle
  builder.AppendString(folder_name);  // folder
  builder.AppendString(app_name);     // appid
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (createFolder)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  if (!reader.PopBool(success_ptr)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (createFolder): " << response->ToString();
    return CANNOT_READ;
  }
  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::WritePassword(const int handle,
                                              const std::string& folder_name,
                                              const std::string& key,
                                              const std::string& password,
                                              const std::string& app_name,
                                              bool* const write_success_ptr) {
  dbus::MethodCall method_call(kKWalletInterface, "writePassword");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(handle);
  builder.AppendString(folder_name);
  builder.AppendString(key);
  builder.AppendString(password);
  builder.AppendString(app_name);
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (writePassword)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  int return_code;
  if (!reader.PopInt32(&return_code)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (writePassword): " << response->ToString();
    return CANNOT_READ;
  }
  *write_success_ptr = return_code == 0;
  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::ReadPassword(const int handle,
                                             const std::string& folder_name,
                                             const std::string& key,
                                             const std::string& app_name,
                                             std::string* const password_ptr) {
  dbus::MethodCall method_call(kKWalletInterface, "readPassword");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(handle);
  builder.AppendString(folder_name);
  builder.AppendString(key);
  builder.AppendString(app_name);
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (readPassword)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  if (!reader.PopString(password_ptr)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (readPassword): " << response->ToString();
    return CANNOT_READ;
  }
  return SUCCESS;
}

KWalletDBus::Error KWalletDBus::Close(const int handle,
                                      const bool force,
                                      const std::string& app_name,
                                      bool* success_ptr) {
  dbus::MethodCall method_call(kKWalletInterface, "close");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(handle);
  builder.AppendBool(force);
  builder.AppendString(app_name);
  std::unique_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response) {
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (close)";
    return CANNOT_CONTACT;
  }
  dbus::MessageReader reader(response.get());
  int return_code = 1;
  if (!reader.PopInt32(&return_code)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (close): " << response->ToString();
    return CANNOT_READ;
  }
  *success_ptr = return_code == 0;
  return SUCCESS;
}
