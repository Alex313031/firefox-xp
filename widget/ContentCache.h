/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentCache_h
#define mozilla_ContentCache_h

#include <stdint.h>

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EventForwards.h"
#include "mozilla/WritingModes.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "nsTArray.h"
#include "Units.h"

namespace mozilla {

class ContentCacheInParent;

namespace dom {
class BrowserParent;
}  // namespace dom

/**
 * ContentCache stores various information of the child content.
 * This class has members which are necessary both in parent process and
 * content process.
 */

class ContentCache {
 public:
  typedef nsTArray<LayoutDeviceIntRect> RectArray;
  typedef widget::IMENotification IMENotification;

  ContentCache();

 protected:
  // Whole text in the target
  nsString mText;

  // Start offset of the composition string.
  uint32_t mCompositionStart;

  enum { ePrevCharRect = 1, eNextCharRect = 0 };

  struct Selection final {
    // Following values are offset in "flat text".
    uint32_t mAnchor;
    uint32_t mFocus;

    WritingMode mWritingMode;

    // Character rects at previous and next character of mAnchor and mFocus.
    // The reason why ContentCache needs to store each previous character of
    // them is IME may query character rect of the last character of a line
    // when caret is at the end of the line.
    // Note that use ePrevCharRect and eNextCharRect for accessing each item.
    LayoutDeviceIntRect mAnchorCharRects[2];
    LayoutDeviceIntRect mFocusCharRects[2];

    // Whole rect of selected text. This is empty if the selection is collapsed.
    LayoutDeviceIntRect mRect;

    Selection() : mAnchor(UINT32_MAX), mFocus(UINT32_MAX) {}

    void Clear() {
      mAnchor = mFocus = UINT32_MAX;
      mWritingMode = WritingMode();
      ClearAnchorCharRects();
      ClearFocusCharRects();
      mRect.SetEmpty();
    }

    void ClearAnchorCharRects() {
      for (size_t i = 0; i < ArrayLength(mAnchorCharRects); i++) {
        mAnchorCharRects[i].SetEmpty();
      }
    }
    void ClearFocusCharRects() {
      for (size_t i = 0; i < ArrayLength(mFocusCharRects); i++) {
        mFocusCharRects[i].SetEmpty();
      }
    }

    bool IsValid() const {
      return mAnchor != UINT32_MAX && mFocus != UINT32_MAX;
    }
    bool Collapsed() const {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return mFocus == mAnchor;
    }
    bool Reversed() const {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return mFocus < mAnchor;
    }
    uint32_t StartOffset() const {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return Reversed() ? mFocus : mAnchor;
    }
    uint32_t EndOffset() const {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return Reversed() ? mAnchor : mFocus;
    }
    uint32_t Length() const {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return Reversed() ? mAnchor - mFocus : mFocus - mAnchor;
    }
    LayoutDeviceIntRect StartCharRect() const {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return Reversed() ? mFocusCharRects[eNextCharRect]
                        : mAnchorCharRects[eNextCharRect];
    }
    LayoutDeviceIntRect EndCharRect() const {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return Reversed() ? mAnchorCharRects[eNextCharRect]
                        : mFocusCharRects[eNextCharRect];
    }
  } mSelection;

  bool IsSelectionValid() const {
    return mSelection.IsValid() && mSelection.EndOffset() <= mText.Length();
  }

  // Stores first char rect because Yosemite's Japanese IME sometimes tries
  // to query it.  If there is no text, this is caret rect.
  LayoutDeviceIntRect mFirstCharRect;

  struct Caret final {
    uint32_t mOffset;
    LayoutDeviceIntRect mRect;

    Caret() : mOffset(UINT32_MAX) {}

    void Clear() {
      mOffset = UINT32_MAX;
      mRect.SetEmpty();
    }

    bool IsValid() const { return mOffset != UINT32_MAX; }

    uint32_t Offset() const {
      NS_ASSERTION(IsValid(), "The caller should check if the caret is valid");
      return mOffset;
    }
  } mCaret;

  struct TextRectArray final {
    uint32_t mStart;
    RectArray mRects;

    TextRectArray() : mStart(UINT32_MAX) {}

    void Clear() {
      mStart = UINT32_MAX;
      mRects.Clear();
    }

    bool IsValid() const {
      if (mStart == UINT32_MAX) {
        return false;
      }
      CheckedInt<uint32_t> endOffset =
          CheckedInt<uint32_t>(mStart) + mRects.Length();
      return endOffset.isValid();
    }
    bool HasRects() const { return IsValid() && !mRects.IsEmpty(); }
    uint32_t StartOffset() const {
      NS_ASSERTION(IsValid(), "The caller should check if the caret is valid");
      return mStart;
    }
    uint32_t EndOffset() const {
      NS_ASSERTION(IsValid(), "The caller should check if the caret is valid");
      if (!IsValid()) {
        return UINT32_MAX;
      }
      return mStart + mRects.Length();
    }
    bool InRange(uint32_t aOffset) const {
      return IsValid() && StartOffset() <= aOffset && aOffset < EndOffset();
    }
    bool InRange(uint32_t aOffset, uint32_t aLength) const {
      CheckedInt<uint32_t> endOffset = CheckedInt<uint32_t>(aOffset) + aLength;
      if (NS_WARN_IF(!endOffset.isValid())) {
        return false;
      }
      return InRange(aOffset) && aOffset + aLength <= EndOffset();
    }
    bool IsOverlappingWith(uint32_t aOffset, uint32_t aLength) const {
      if (!HasRects() || aOffset == UINT32_MAX || !aLength) {
        return false;
      }
      CheckedInt<uint32_t> endOffset = CheckedInt<uint32_t>(aOffset) + aLength;
      if (NS_WARN_IF(!endOffset.isValid())) {
        return false;
      }
      return aOffset < EndOffset() && endOffset.value() > mStart;
    }
    LayoutDeviceIntRect GetRect(uint32_t aOffset) const;
    LayoutDeviceIntRect GetUnionRect(uint32_t aOffset, uint32_t aLength) const;
    LayoutDeviceIntRect GetUnionRectAsFarAsPossible(
        uint32_t aOffset, uint32_t aLength, bool aRoundToExistingOffset) const;
  } mTextRectArray;

  LayoutDeviceIntRect mEditorRect;

  friend class ContentCacheInParent;
  friend struct IPC::ParamTraits<ContentCache>;
};

class ContentCacheInChild final : public ContentCache {
 public:
  ContentCacheInChild();

  /**
   * When IME loses focus, this should be called and making this forget the
   * content for reducing footprint.
   */
  void Clear();

  /**
   * Cache*() retrieves the latest content information and store them.
   * Be aware, CacheSelection() calls CacheTextRects(), and also CacheText()
   * calls CacheSelection().  So, related data is also retrieved automatically.
   */
  bool CacheEditorRect(nsIWidget* aWidget,
                       const IMENotification* aNotification = nullptr);
  bool CacheSelection(nsIWidget* aWidget,
                      const IMENotification* aNotification = nullptr);
  bool CacheText(nsIWidget* aWidget,
                 const IMENotification* aNotification = nullptr);

  bool CacheAll(nsIWidget* aWidget,
                const IMENotification* aNotification = nullptr);

  /**
   * SetSelection() modifies selection with specified raw data. And also this
   * tries to retrieve text rects too.
   */
  void SetSelection(nsIWidget* aWidget, uint32_t aStartOffset, uint32_t aLength,
                    bool aReversed, const WritingMode& aWritingMode);

 private:
  bool QueryCharRect(nsIWidget* aWidget, uint32_t aOffset,
                     LayoutDeviceIntRect& aCharRect) const;
  bool QueryCharRectArray(nsIWidget* aWidget, uint32_t aOffset,
                          uint32_t aLength, RectArray& aCharRectArray) const;
  bool CacheCaret(nsIWidget* aWidget,
                  const IMENotification* aNotification = nullptr);
  bool CacheTextRects(nsIWidget* aWidget,
                      const IMENotification* aNotification = nullptr);
};

class ContentCacheInParent final : public ContentCache {
 public:
  explicit ContentCacheInParent(dom::BrowserParent& aBrowserParent);

  /**
   * AssignContent() is called when BrowserParent receives ContentCache from
   * the content process.  This doesn't copy composition information because
   * it's managed by BrowserParent itself.
   */
  void AssignContent(const ContentCache& aOther, nsIWidget* aWidget,
                     const IMENotification* aNotification = nullptr);

  /**
   * HandleQueryContentEvent() sets content data to aEvent.mReply.
   *
   * For eQuerySelectedText, fail if the cache doesn't contain the whole
   *  selected range. (This shouldn't happen because PuppetWidget should have
   *  already sent the whole selection.)
   *
   * For eQueryTextContent, fail only if the cache doesn't overlap with
   *  the queried range. Note the difference from above. We use
   *  this behavior because a normal eQueryTextContent event is allowed to
   *  have out-of-bounds offsets, so that widget can request content without
   *  knowing the exact length of text. It's up to widget to handle cases when
   *  the returned offset/length are different from the queried offset/length.
   *
   * For eQueryTextRect, fail if cached offset/length aren't equals to input.
   *   Cocoa widget always queries selected offset, so it works on it.
   *
   * For eQueryCaretRect, fail if cached offset isn't equals to input
   *
   * For eQueryEditorRect, always success
   */
  bool HandleQueryContentEvent(WidgetQueryContentEvent& aEvent,
                               nsIWidget* aWidget) const;

  /**
   * OnCompositionEvent() should be called before sending composition string.
   * This returns true if the event should be sent.  Otherwise, false.
   */
  bool OnCompositionEvent(const WidgetCompositionEvent& aCompositionEvent);

  /**
   * OnSelectionEvent() should be called before sending selection event.
   */
  void OnSelectionEvent(const WidgetSelectionEvent& aSelectionEvent);

  /**
   * OnEventNeedingAckHandled() should be called after the child process
   * handles a sent event which needs acknowledging.
   *
   * WARNING: This may send notifications to IME.  That might cause destroying
   *          BrowserParent or aWidget.  Therefore, the caller must not destroy
   *          this instance during a call of this method.
   */
  void OnEventNeedingAckHandled(nsIWidget* aWidget, EventMessage aMessage);

  /**
   * RequestIMEToCommitComposition() requests aWidget to commit or cancel
   * composition.  If it's handled synchronously, this returns true.
   *
   * @param aWidget     The widget to be requested to commit or cancel
   *                    the composition.
   * @param aCancel     When the caller tries to cancel the composition, true.
   *                    Otherwise, i.e., tries to commit the composition, false.
   * @param aCommittedString    The committed string (i.e., the last data of
   *                            dispatched composition events during requesting
   *                            IME to commit composition.
   * @return            Whether the composition is actually committed
   *                    synchronously.
   */
  bool RequestIMEToCommitComposition(nsIWidget* aWidget, bool aCancel,
                                     nsAString& aCommittedString);

  /**
   * MaybeNotifyIME() may notify IME of the notification.  If child process
   * hasn't been handled all sending events yet, this stores the notification
   * and flush it later.
   */
  void MaybeNotifyIME(nsIWidget* aWidget, const IMENotification& aNotification);

 private:
  IMENotification mPendingSelectionChange;
  IMENotification mPendingTextChange;
  IMENotification mPendingLayoutChange;
  IMENotification mPendingCompositionUpdate;

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  // Log of event messages to be output to crash report.
  nsTArray<EventMessage> mDispatchedEventMessages;
  nsTArray<EventMessage> mReceivedEventMessages;
  // Log of RequestIMEToCommitComposition() in the last 2 compositions.
  enum class RequestIMEToCommitCompositionResult : uint8_t {
    eToOldCompositionReceived,
    eToCommittedCompositionReceived,
    eReceivedAfterBrowserParentBlur,
    eReceivedButNoTextComposition,
    eHandledAsynchronously,
    eHandledSynchronously,
  };
  const char* ToReadableText(
      RequestIMEToCommitCompositionResult aResult) const {
    switch (aResult) {
      case RequestIMEToCommitCompositionResult::eToOldCompositionReceived:
        return "Commit request is not handled because it's for "
               "older composition";
      case RequestIMEToCommitCompositionResult::eToCommittedCompositionReceived:
        return "Commit request is not handled because BrowserParent has "
               "already "
               "sent commit event for the composition";
      case RequestIMEToCommitCompositionResult::eReceivedAfterBrowserParentBlur:
        return "Commit request is handled with stored composition string "
               "because BrowserParent has already lost focus";
      case RequestIMEToCommitCompositionResult::eReceivedButNoTextComposition:
        return "Commit request is not handled because there is no "
               "TextCompsition instance";
      case RequestIMEToCommitCompositionResult::eHandledAsynchronously:
        return "Commit request is handled but IME doesn't commit current "
               "composition synchronously";
      case RequestIMEToCommitCompositionResult::eHandledSynchronously:
        return "Commit reqeust is handled synchronously";
      default:
        return "Unknown reason";
    }
  }
  nsTArray<RequestIMEToCommitCompositionResult>
      mRequestIMEToCommitCompositionResults;
#endif  // MOZ_DIAGNOSTIC_ASSERT_ENABLED

  // mBrowserParent is owner of the instance.
  dom::BrowserParent& MOZ_NON_OWNING_REF mBrowserParent;
  // mCompositionString is composition string which were sent to the remote
  // process but not yet committed in the remote process.
  nsString mCompositionString;
  // This is not nullptr only while the instance is requesting IME to
  // composition.  Then, data value of dispatched composition events should
  // be stored into the instance.
  nsAString* mCommitStringByRequest;
  // mPendingEventsNeedingAck is increased before sending a composition event or
  // a selection event and decreased after they are received in the child
  // process.
  uint32_t mPendingEventsNeedingAck;
  // mCompositionStartInChild stores current composition start offset in the
  // remote process.
  uint32_t mCompositionStartInChild;
  // mPendingCommitLength is commit string length of the first pending
  // composition.  This is used by relative offset query events when querying
  // new composition start offset.
  // Note that when mPendingCompositionCount is not 0, i.e., there are 2 or
  // more pending compositions, this cache won't be used because in such case,
  // anyway ContentCacheInParent cannot return proper character rect.
  uint32_t mPendingCommitLength;
  // mPendingCompositionCount is number of compositions which started in widget
  // but not yet handled in the child process.
  uint8_t mPendingCompositionCount;
  // mPendingCommitCount is number of eCompositionCommit(AsIs) events which
  // were sent to the child process but not yet handled in it.
  uint8_t mPendingCommitCount;
  // mWidgetHasComposition is true when the widget in this process thinks that
  // IME has composition.  So, this is set to true when eCompositionStart is
  // dispatched and set to false when eCompositionCommit(AsIs) is dispatched.
  bool mWidgetHasComposition;
  // mIsChildIgnoringCompositionEvents is set to true if the child process
  // requests commit composition whose commit has already been sent to it.
  // Then, set to false when the child process ignores the commit event.
  bool mIsChildIgnoringCompositionEvents;

  ContentCacheInParent() = delete;

  /**
   * When following methods' aRoundToExistingOffset is true, even if specified
   * offset or range is out of bounds, the result is computed with the existing
   * cache forcibly.
   */
  bool GetCaretRect(uint32_t aOffset, bool aRoundToExistingOffset,
                    LayoutDeviceIntRect& aCaretRect) const;
  bool GetTextRect(uint32_t aOffset, bool aRoundToExistingOffset,
                   LayoutDeviceIntRect& aTextRect) const;
  bool GetUnionTextRects(uint32_t aOffset, uint32_t aLength,
                         bool aRoundToExistingOffset,
                         LayoutDeviceIntRect& aUnionTextRect) const;

  void FlushPendingNotifications(nsIWidget* aWidget);

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  /**
   * Remove unnecessary messages from mDispatchedEventMessages and
   * mReceivedEventMessages.
   */
  void RemoveUnnecessaryEventMessageLog();

  /**
   * Append event message log to aLog.
   */
  void AppendEventMessageLog(nsACString& aLog) const;
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
};

}  // namespace mozilla

#endif  // mozilla_ContentCache_h
