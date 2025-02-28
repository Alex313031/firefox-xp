/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://webaudio.github.io/web-audio-api/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

dictionary AudioBufferSourceOptions {
             AudioBuffer? buffer;
             float        detune = 0;
             boolean      loop = false;
             double       loopEnd = 0;
             double       loopStart = 0;
             float        playbackRate = 1;
};

[Pref="dom.webaudio.enabled",
 Exposed=Window]
interface AudioBufferSourceNode : AudioScheduledSourceNode {
    constructor(BaseAudioContext context,
                optional AudioBufferSourceOptions options = {});

    attribute AudioBuffer? buffer;

    readonly attribute AudioParam playbackRate;
    readonly attribute AudioParam detune;

    attribute boolean loop;
    attribute double loopStart;
    attribute double loopEnd;

    [Throws]
    void start(optional double when = 0, optional double grainOffset = 0,
               optional double grainDuration);
};

// Mozilla extensions
AudioBufferSourceNode includes AudioNodePassThrough;
