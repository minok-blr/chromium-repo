// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_ACCESS_TOKEN_H_
#define BASE_WIN_ACCESS_TOKEN_H_

#include <memory>
#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/win/scoped_handle.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace win {

// This class is used to access the information for a Windows access token.
class BASE_EXPORT AccessToken {
 public:
  // This class represents an access token group.
  class BASE_EXPORT Group {
   public:
    // Get the group SID.
    const Sid& GetSid() const { return sid_; }
    // Get the group attribute flags.
    DWORD GetAttributes() const { return attributes_; }
    // Returns true if the group is an integrity level.
    bool IsIntegrity() const;
    // Returns true if the group is enabled.
    bool IsEnabled() const;
    // Returns true if the group is deny only.
    bool IsDenyOnly() const;
    // Returns true if the group is the logon ID.
    bool IsLogonId() const;

    Group(Sid&& sid, DWORD attributes);
    Group(Group&&);
    ~Group();

   private:
    Sid sid_;
    DWORD attributes_;
  };

  // This class represents an access token privilege.
  class BASE_EXPORT Privilege {
   public:
    // Get the privilege LUID.
    CHROME_LUID GetLuid() const { return luid_; }
    // Get the privilege attribute flags.
    DWORD GetAttributes() const { return attributes_; }
    // Get the name of the privilege.
    std::wstring GetName() const;
    // Returns true if the privilege is enabled.
    bool IsEnabled() const;

    Privilege(CHROME_LUID luid, DWORD attributes);

   private:
    CHROME_LUID luid_;
    DWORD attributes_;
  };

  // This class represents an access token default DACL.
  class BASE_EXPORT Dacl {
   public:
    // Constructor, makes a copy of |dacl|.
    explicit Dacl(const ACL* dacl);
    Dacl(const Dacl&) = delete;
    Dacl& operator=(const Dacl&) = delete;
    Dacl(Dacl&&);
    ~Dacl();

    // Get a typed pointer to the ACL.
    ACL* GetAcl() const;

   private:
    std::vector<char> acl_buffer_;
  };

  // Creates an AccessToken object from a token handle.
  // |token| the token handle. This handle will be duplicated for TOKEN_QUERY
  // access, therefore the caller must be granted that access to the token
  // object. The AccessToken object owns its own copy of the token handle so
  // the original can be closed.
  static absl::optional<AccessToken> FromToken(HANDLE token);

  // Creates an AccessToken object from a process handle.
  // |process| the process handle. The handle needs to have
  // PROCESS_QUERY_LIMITED_INFORMATION access to the handle and TOKEN_QUERY
  // access to the token object.
  // |impersonation| if true then the process token will be duplicated to an
  // impersonation token. This allows you to call the IsMember API which
  // requires an impersonation token. To duplicate TOKEN_DUPLICATE access is
  // required.
  static absl::optional<AccessToken> FromProcess(HANDLE process,
                                                 bool impersonation = false);

  // Creates an AccessToken object for the current process.
  // |impersonation| if true then the process token will be duplicated to an
  // impersonation token. This allows you to call the IsMember API which
  // requires an impersonation token. To duplicate TOKEN_DUPLICATE access is
  // required.
  static absl::optional<AccessToken> FromCurrentProcess(
      bool impersonation = false);

  // Creates an AccessToken object from a thread handle. The thread must be
  // impersonating a token for this to succeed.
  // |thread| the thread handle. The handle needs to have
  // THREAD_QUERY_LIMITED_INFORMATION access and TOKEN_QUERY access to the
  // token object.
  // |open_as_self| open the token using the process token rather than the
  // current thread's impersonated token.
  // If the thread isn't impersonating it will return an empty value and the
  // Win32 last error code will be ERROR_NO_TOKEN.
  static absl::optional<AccessToken> FromThread(HANDLE thread,
                                                bool open_as_self = true);

  // Creates an AccessToken object from the current thread. The thread must be
  // impersonating a token for this to succeed.
  // |open_as_self| open the thread handle using the process token rather
  // than the current thread's impersonated token.
  // If the thread isn't impersonating it will return an empty value and the
  // Win32 last error code will be ERROR_NO_TOKEN.
  static absl::optional<AccessToken> FromCurrentThread(
      bool open_as_self = true);

  // Creates an AccessToken object for the current thread's effective token.
  // If the thread is impersonating then it'll try and open the thread token,
  // otherwise it'll open the process token.
  static absl::optional<AccessToken> FromEffective();

  AccessToken(const AccessToken&) = delete;
  AccessToken& operator=(const AccessToken&) = delete;
  AccessToken(AccessToken&&);
  AccessToken& operator=(AccessToken&&);
  ~AccessToken();

  // Get the token's user SID.
  Sid User() const;

  // Get the token's user group.
  Group UserGroup() const;

  // Get the token's owner SID. This can be different to the user SID, it's
  // used as the default owner for new secured objects.
  Sid Owner() const;

  // Get the token's primary group SID.
  Sid PrimaryGroup() const;

  // Get the token logon SID. Returns an empty value if the token doesn't have
  // a logon SID. If the logon SID doesn't exist then the Win32 last error code
  // will be ERROR_NOT_FOUND.
  absl::optional<Sid> LogonId() const;

  // Get the token's integrity level. Returns MAXDWORD if the token doesn't
  // have an integrity level.
  DWORD IntegrityLevel() const;

  // Get the token's session ID. Returns MAXDWORD if the token if the session
  // ID can't be queried.
  DWORD SessionId() const;

  // The token's group list.
  std::vector<Group> Groups() const;

  // Get whether the token is a restricted.
  bool IsRestricted() const;

  // The token's restricted SIDs list. If not a restricted token this will
  // return an empty vector.
  std::vector<Group> RestrictedSids() const;

  // Get whether the token is an appcontainer.
  bool IsAppContainer() const;

  // Get the token's appcontainer SID. If not an appcontainer token this will
  // return an empty value.
  absl::optional<Sid> AppContainerSid() const;

  // The token's capabilities. If not an appcontainer token this will return an
  // empty vector.
  std::vector<Group> Capabilities() const;

  // Get the UAC linked token.
  absl::optional<AccessToken> LinkedToken() const;

  // Get the default DACL for the token. Returns an empty value if the token
  // doesn't have a default DACL.
  absl::optional<Dacl> DefaultDacl() const;

  // Get the token's ID.
  CHROME_LUID Id() const;

  // Get the token's authentication ID.
  CHROME_LUID AuthenticationId() const;

  // Get the token's privileges.
  std::vector<Privilege> Privileges() const;

  // Get whether the token is elevated.
  bool IsElevated() const;

  // Checks if the sid is a member of the token's groups. The token must be
  // an impersonation token rather than a primary token. If the token is not an
  // impersonation token then it returns false and the Win32 last error will be
  // set to ERROR_NO_IMPERSONATION_TOKEN.
  bool IsMember(const Sid& sid) const;

  // Checks if the well known sid is a member of the token's groups. The token
  // must be an impersonation token rather than a primary token. If the token
  // is not an impersonation token then it returns false and the Win32 last
  // error will be set to ERROR_NO_IMPERSONATION_TOKEN.
  bool IsMember(WellKnownSid known_sid) const;

  // Checks if the token is an impersonation token. If false then it's a primary
  // token.
  bool IsImpersonation() const;

  // Checks if the token can only be used for identification. This is based on
  // the security impersonation level of the token. If the level is less than
  // or equal to SecurityIdentification this function returns true. Always
  // returns false for a primary token.
  bool IsIdentification() const;

 private:
  explicit AccessToken(HANDLE token);
  ScopedHandle token_;
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_ACCESS_TOKEN_H_