/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/2011/webrtc/editor/webrtc.html#mediastreamevent
 */

dictionary MediaStreamTrackEventInit : EventInit {
    required MediaStreamTrack track;
};

[Exposed=Window]
interface MediaStreamTrackEvent : Event {
    constructor(DOMString type, MediaStreamTrackEventInit eventInitDict);

    [SameObject]
    readonly        attribute MediaStreamTrack track;
};
