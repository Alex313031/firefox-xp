/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTrack.h"
#include "AudioTrack.h"
#include "MediaTrackList.h"
#include "VideoTrack.h"

namespace mozilla {
namespace dom {

MediaTrack::MediaTrack(nsIGlobalObject* aOwnerGlobal, const nsAString& aId,
                       const nsAString& aKind, const nsAString& aLabel,
                       const nsAString& aLanguage)
    : DOMEventTargetHelper(aOwnerGlobal),
      mId(aId),
      mKind(aKind),
      mLabel(aLabel),
      mLanguage(aLanguage) {}

MediaTrack::~MediaTrack() {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(dom::MediaTrack, DOMEventTargetHelper, mList)

NS_IMPL_ADDREF_INHERITED(dom::MediaTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(dom::MediaTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(dom::MediaTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

void MediaTrack::SetTrackList(MediaTrackList* aList) { mList = aList; }

}  // namespace dom
}  // namespace mozilla
