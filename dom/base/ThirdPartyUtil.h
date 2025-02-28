/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThirdPartyUtil_h__
#define ThirdPartyUtil_h__

#include "mozIThirdPartyUtil.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Document.h"
#include "nsCOMPtr.h"
#include "nsEffectiveTLDService.h"
#include "nsString.h"
#include "nsPIDOMWindow.h"

class nsIURI;
class nsPIDOMWindowOuter;

class ThirdPartyUtil final : public mozIThirdPartyUtil {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZITHIRDPARTYUTIL

  nsresult Init();

  static void Startup();
  static ThirdPartyUtil* GetInstance();

 private:
  ~ThirdPartyUtil();

  bool IsThirdPartyInternal(const nsCString& aFirstDomain,
                            const nsCString& aSecondDomain) {
    // Check strict equality.
    return aFirstDomain != aSecondDomain;
  }
  nsresult IsThirdPartyInternal(const nsCString& aFirstDomain,
                                nsIURI* aSecondURI, bool* aResult);

  nsCString GetBaseDomainFromWindow(nsPIDOMWindowOuter* aWindow);

  RefPtr<nsEffectiveTLDService> mTLDService;
};

#endif
