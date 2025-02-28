/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm");
const {Dedupe} = ChromeUtils.import("resource://activity-stream/common/Dedupe.jsm");

const TOP_SITES_DEFAULT_ROWS = 1;
const TOP_SITES_MAX_SITES_PER_ROW = 8;

const dedupe = new Dedupe(site => site && site.url);

const INITIAL_STATE = {
  App: {
    // Have we received real data from the app yet?
    initialized: false,
  },
  TopSites: {
    // Have we received real data from history yet?
    initialized: false,
    // The history (and possibly default) links
    rows: [],
    // Used in content only to dispatch action to TopSiteForm.
    editForm: null,
    // Used in content only to open the SearchShortcutsForm modal.
    showSearchShortcutsForm: false,
    // The list of available search shortcuts.
    searchShortcuts: [],
  },
  Prefs: {
    initialized: false,
    values: {},
  },
  Dialog: {
    visible: false,
    data: {},
  },
  Sections: [],
  Search: {
    // When search hand-off is enabled, we render a big button that is styled to
    // look like a search textbox. If the button is clicked, we style
    // the button as if it was a focused search box and show a fake cursor but
    // really focus the awesomebar without the focus styles ("hidden focus").
    fakeFocus: false,
    // Hide the search box after handing off to AwesomeBar and user starts typing.
    hide: false,
  },
};

function App(prevState = INITIAL_STATE.App, action) {
  switch (action.type) {
    case at.INIT:
      return Object.assign({}, prevState, action.data || {}, {initialized: true});
    default:
      return prevState;
  }
}

function ASRouter(prevState = INITIAL_STATE.ASRouter, action) {
  switch (action.type) {
    case at.AS_ROUTER_INITIALIZED:
      return {...action.data, initialized: true};
    default:
      return prevState;
  }
}

/**
 * insertPinned - Inserts pinned links in their specified slots
 *
 * @param {array} a list of links
 * @param {array} a list of pinned links
 * @return {array} resulting list of links with pinned links inserted
 */
function insertPinned(links, pinned) {
  // Remove any pinned links
  const pinnedUrls = pinned.map(link => link && link.url);
  let newLinks = links.filter(link => (link ? !pinnedUrls.includes(link.url) : false));
  newLinks = newLinks.map(link => {
    if (link && link.isPinned) {
      delete link.isPinned;
      delete link.pinIndex;
    }
    return link;
  });

  // Then insert them in their specified location
  pinned.forEach((val, index) => {
    if (!val) { return; }
    let link = Object.assign({}, val, {isPinned: true, pinIndex: index});
    if (index > newLinks.length) {
      newLinks[index] = link;
    } else {
      newLinks.splice(index, 0, link);
    }
  });

  return newLinks;
}

function TopSites(prevState = INITIAL_STATE.TopSites, action) {
  let hasMatch;
  let newRows;
  switch (action.type) {
    case at.TOP_SITES_UPDATED:
      if (!action.data || !action.data.links) {
        return prevState;
      }
      return Object.assign({}, prevState, {initialized: true, rows: action.data.links}, action.data.pref ? {pref: action.data.pref} : {});
    case at.TOP_SITES_PREFS_UPDATED:
      return Object.assign({}, prevState, {pref: action.data.pref});
    case at.TOP_SITES_EDIT:
      return Object.assign({}, prevState, {
        editForm: {
          index: action.data.index,
          previewResponse: null,
        },
      });
    case at.TOP_SITES_CANCEL_EDIT:
      return Object.assign({}, prevState, {editForm: null});
    case at.TOP_SITES_OPEN_SEARCH_SHORTCUTS_MODAL:
      return Object.assign({}, prevState, {showSearchShortcutsForm: true});
    case at.TOP_SITES_CLOSE_SEARCH_SHORTCUTS_MODAL:
      return Object.assign({}, prevState, {showSearchShortcutsForm: false});
    case at.PREVIEW_RESPONSE:
      if (!prevState.editForm || action.data.url !== prevState.editForm.previewUrl) {
        return prevState;
      }
      return Object.assign({}, prevState, {
        editForm: {
          index: prevState.editForm.index,
          previewResponse: action.data.preview,
          previewUrl: action.data.url,
        },
      });
    case at.PREVIEW_REQUEST:
      if (!prevState.editForm) {
        return prevState;
      }
      return Object.assign({}, prevState, {
        editForm: {
          index: prevState.editForm.index,
          previewResponse: null,
          previewUrl: action.data.url,
        },
      });
    case at.PREVIEW_REQUEST_CANCEL:
      if (!prevState.editForm) {
        return prevState;
      }
      return Object.assign({}, prevState, {
        editForm: {
          index: prevState.editForm.index,
          previewResponse: null,
        },
      });
    case at.SCREENSHOT_UPDATED:
      newRows = prevState.rows.map(row => {
        if (row && row.url === action.data.url) {
          hasMatch = true;
          return Object.assign({}, row, {screenshot: action.data.screenshot});
        }
        return row;
      });
      return hasMatch ? Object.assign({}, prevState, {rows: newRows}) : prevState;
    case at.PLACES_BOOKMARK_ADDED:
      if (!action.data) {
        return prevState;
      }
      newRows = prevState.rows.map(site => {
        if (site && site.url === action.data.url) {
          const {bookmarkGuid, bookmarkTitle, dateAdded} = action.data;
          return Object.assign({}, site, {bookmarkGuid, bookmarkTitle, bookmarkDateCreated: dateAdded});
        }
        return site;
      });
      return Object.assign({}, prevState, {rows: newRows});
    case at.PLACES_BOOKMARK_REMOVED:
      if (!action.data) {
        return prevState;
      }
      newRows = prevState.rows.map(site => {
        if (site && site.url === action.data.url) {
          const newSite = Object.assign({}, site);
          delete newSite.bookmarkGuid;
          delete newSite.bookmarkTitle;
          delete newSite.bookmarkDateCreated;
          return newSite;
        }
        return site;
      });
      return Object.assign({}, prevState, {rows: newRows});
    case at.PLACES_LINK_DELETED:
      if (!action.data) {
        return prevState;
      }
      newRows = prevState.rows.filter(site => action.data.url !== site.url);
      return Object.assign({}, prevState, {rows: newRows});
    case at.UPDATE_SEARCH_SHORTCUTS:
      return {...prevState, searchShortcuts: action.data.searchShortcuts};
    default:
      return prevState;
  }
}

function Dialog(prevState = INITIAL_STATE.Dialog, action) {
  switch (action.type) {
    case at.DIALOG_OPEN:
      return Object.assign({}, prevState, {visible: true, data: action.data});
    case at.DIALOG_CANCEL:
      return Object.assign({}, prevState, {visible: false});
    case at.DELETE_HISTORY_URL:
      return Object.assign({}, INITIAL_STATE.Dialog);
    default:
      return prevState;
  }
}

function Prefs(prevState = INITIAL_STATE.Prefs, action) {
  let newValues;
  switch (action.type) {
    case at.PREFS_INITIAL_VALUES:
      return Object.assign({}, prevState, {initialized: true, values: action.data});
    case at.PREF_CHANGED:
      newValues = Object.assign({}, prevState.values);
      newValues[action.data.name] = action.data.value;
      return Object.assign({}, prevState, {values: newValues});
    default:
      return prevState;
  }
}

function Sections(prevState = INITIAL_STATE.Sections, action) {
  let hasMatch;
  let newState;
  switch (action.type) {
    case at.SECTION_DEREGISTER:
      return prevState.filter(section => section.id !== action.data);
    case at.SECTION_REGISTER:
      // If section exists in prevState, update it
      newState = prevState.map(section => {
        if (section && section.id === action.data.id) {
          hasMatch = true;
          return Object.assign({}, section, action.data);
        }
        return section;
      });
      // Otherwise, append it
      if (!hasMatch) {
        const initialized = !!(action.data.rows && action.data.rows.length > 0);
        const section = Object.assign({title: "", rows: [], enabled: false}, action.data, {initialized});
        newState.push(section);
      }
      return newState;
    case at.SECTION_UPDATE:
      newState = prevState.map(section => {
        if (section && section.id === action.data.id) {
          // If the action is updating rows, we should consider initialized to be true.
          // This can be overridden if initialized is defined in the action.data
          const initialized = action.data.rows ? {initialized: true} : {};

          // Make sure pinned cards stay at their current position when rows are updated.
          // Disabling a section (SECTION_UPDATE with empty rows) does not retain pinned cards.
          if (action.data.rows && action.data.rows.length > 0 && section.rows.find(card => card.pinned)) {
            const rows = Array.from(action.data.rows);
            section.rows.forEach((card, index) => {
              if (card.pinned) {
                // Only add it if it's not already there.
                if (rows[index].guid !== card.guid) {
                  rows.splice(index, 0, card);
                }
              }
            });
            return Object.assign({}, section, initialized, Object.assign({}, action.data, {rows}));
          }

          return Object.assign({}, section, initialized, action.data);
        }
        return section;
      });

      if (!action.data.dedupeConfigurations) {
        return newState;
      }

      action.data.dedupeConfigurations.forEach(dedupeConf => {
        newState = newState.map(section => {
          if (section.id === dedupeConf.id) {
            const dedupedRows = dedupeConf.dedupeFrom.reduce((rows, dedupeSectionId) => {
              const dedupeSection = newState.find(s => s.id === dedupeSectionId);
              const [, newRows] = dedupe.group(dedupeSection.rows, rows);
              return newRows;
            }, section.rows);

            return Object.assign({}, section, {rows: dedupedRows});
          }

          return section;
        });
      });

      return newState;
    case at.SECTION_UPDATE_CARD:
      return prevState.map(section => {
        if (section && section.id === action.data.id && section.rows) {
          const newRows = section.rows.map(card => {
            if (card.url === action.data.url) {
              return Object.assign({}, card, action.data.options);
            }
            return card;
          });
          return Object.assign({}, section, {rows: newRows});
        }
        return section;
      });
    case at.PLACES_BOOKMARK_ADDED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.map(item => {
          // find the item within the rows that is attempted to be bookmarked
          if (item.url === action.data.url) {
            const {bookmarkGuid, bookmarkTitle, dateAdded} = action.data;
            return Object.assign({}, item, {
              bookmarkGuid,
              bookmarkTitle,
              bookmarkDateCreated: dateAdded,
              type: "bookmark",
            });
          }
          return item;
        }),
      }));
    case at.PLACES_BOOKMARK_REMOVED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section => Object.assign({}, section, {
        rows: section.rows.map(item => {
          // find the bookmark within the rows that is attempted to be removed
          if (item.url === action.data.url) {
            const newSite = Object.assign({}, item);
            delete newSite.bookmarkGuid;
            delete newSite.bookmarkTitle;
            delete newSite.bookmarkDateCreated;
            if (!newSite.type || newSite.type === "bookmark") {
              newSite.type = "history";
            }
            return newSite;
          }
          return item;
        }),
      }));
    case at.PLACES_LINK_DELETED:
    case at.PLACES_LINK_BLOCKED:
      if (!action.data) {
        return prevState;
      }
      return prevState.map(section =>
        Object.assign({}, section, {rows: section.rows.filter(site => site.url !== action.data.url)}));
    default:
      return prevState;
  }
}

function Search(prevState = INITIAL_STATE.Search, action) {
  switch (action.type) {
    case at.HIDE_SEARCH:
      return Object.assign({...prevState, hide: true});
    case at.FAKE_FOCUS_SEARCH:
      return Object.assign({...prevState, fakeFocus: true});
    case at.SHOW_SEARCH:
      return Object.assign({...prevState, hide: false, fakeFocus: false});
    default:
      return prevState;
  }
}

this.INITIAL_STATE = INITIAL_STATE;
this.TOP_SITES_DEFAULT_ROWS = TOP_SITES_DEFAULT_ROWS;
this.TOP_SITES_MAX_SITES_PER_ROW = TOP_SITES_MAX_SITES_PER_ROW;

this.reducers = {
  TopSites,
  App,
  Prefs,
  Dialog,
  Sections,
  Search,
};

const EXPORTED_SYMBOLS = [
  "reducers",
  "INITIAL_STATE",
  "insertPinned",
  "TOP_SITES_DEFAULT_ROWS",
  "TOP_SITES_MAX_SITES_PER_ROW",
];
