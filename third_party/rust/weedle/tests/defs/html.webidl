[Exposed=Window,
 LegacyUnenumerableNamedProperties]
interface HTMLAllCollection {
  readonly attribute unsigned long length;
  getter Element (unsigned long index);
  getter (HTMLCollection or Element)? namedItem(DOMString name);
  (HTMLCollection or Element)? item(optional DOMString nameOrIndex);

  // Note: HTMLAllCollection objects have a custom [[Call]] internal method and an [[IsHTMLDDA]] internal slot.
};

[Exposed=Window]
interface HTMLFormControlsCollection : HTMLCollection {
  // inherits length and item()
  getter (RadioNodeList or Element)? namedItem(DOMString name); // shadows inherited namedItem()
};

[Exposed=Window]
interface RadioNodeList : NodeList {
  attribute DOMString value;
};

[Exposed=Window]
interface HTMLOptionsCollection : HTMLCollection {
  // inherits item(), namedItem()
  [CEReactions] attribute unsigned long length; // shadows inherited length
  [CEReactions] setter void (unsigned long index, HTMLOptionElement? option);
  [CEReactions] void add((HTMLOptionElement or HTMLOptGroupElement) element, optional (HTMLElement or long)? before = null);
  [CEReactions] void remove(long index);
  attribute long selectedIndex;
};

[Exposed=(Window,Worker)]
interface DOMStringList {
  readonly attribute unsigned long length;
  getter DOMString? item(unsigned long index);
  boolean contains(DOMString string);
};

enum DocumentReadyState { "loading", "interactive", "complete" };
typedef (HTMLScriptElement or SVGScriptElement) HTMLOrSVGScriptElement;

[OverrideBuiltins]
partial interface Document {
  // resource metadata management
  [PutForwards=href, Unforgeable] readonly attribute Location? location;
  attribute USVString domain;
  readonly attribute USVString referrer;
  attribute USVString cookie;
  readonly attribute DOMString lastModified;
  readonly attribute DocumentReadyState readyState;

  // DOM tree accessors
  getter object (DOMString name);
  [CEReactions] attribute DOMString title;
  [CEReactions] attribute DOMString dir;
  [CEReactions] attribute HTMLElement? body;
  readonly attribute HTMLHeadElement? head;
  [SameObject] readonly attribute HTMLCollection images;
  [SameObject] readonly attribute HTMLCollection embeds;
  [SameObject] readonly attribute HTMLCollection plugins;
  [SameObject] readonly attribute HTMLCollection links;
  [SameObject] readonly attribute HTMLCollection forms;
  [SameObject] readonly attribute HTMLCollection scripts;
  NodeList getElementsByName(DOMString elementName);
  readonly attribute HTMLOrSVGScriptElement? currentScript; // classic scripts in a document tree only

  // dynamic markup insertion
  [CEReactions] Document open(optional DOMString type, optional DOMString replace = ""); // type is ignored
  WindowProxy open(USVString url, DOMString name, DOMString features);
  [CEReactions] void close();
  [CEReactions] void write(DOMString... text);
  [CEReactions] void writeln(DOMString... text);

  // user interaction
  readonly attribute WindowProxy? defaultView;
  readonly attribute Element? activeElement;
  boolean hasFocus();
  [CEReactions] attribute DOMString designMode;
  [CEReactions] boolean execCommand(DOMString commandId, optional boolean showUI = false, optional DOMString value = "");
  boolean queryCommandEnabled(DOMString commandId);
  boolean queryCommandIndeterm(DOMString commandId);
  boolean queryCommandState(DOMString commandId);
  boolean queryCommandSupported(DOMString commandId);
  DOMString queryCommandValue(DOMString commandId);

  // special event handler IDL attributes that only apply to Document objects
  [LenientThis] attribute EventHandler onreadystatechange;
};
Document includes GlobalEventHandlers;
Document includes DocumentAndElementEventHandlers;

[Exposed=Window,
 HTMLConstructor]
interface HTMLElement : Element {
  // metadata attributes
  [CEReactions] attribute DOMString title;
  [CEReactions] attribute DOMString lang;
  [CEReactions] attribute boolean translate;
  [CEReactions] attribute DOMString dir;

  // user interaction
  [CEReactions] attribute boolean hidden;
  void click();
  [CEReactions] attribute DOMString accessKey;
  readonly attribute DOMString accessKeyLabel;
  [CEReactions] attribute boolean draggable;
  [CEReactions] attribute boolean spellcheck;
  [CEReactions] attribute DOMString autocapitalize;

  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString innerText;
};

HTMLElement includes GlobalEventHandlers;
HTMLElement includes DocumentAndElementEventHandlers;
HTMLElement includes ElementContentEditable;

// Note: intentionally not [HTMLConstructor]
[Exposed=Window]
interface HTMLUnknownElement : HTMLElement { };

interface mixin HTMLOrSVGElement {
  [SameObject] readonly attribute DOMStringMap dataset;
  attribute DOMString nonce;

  [CEReactions] attribute long tabIndex;
  void focus(optional FocusOptions options);
  void blur();
};
HTMLElement includes HTMLOrSVGElement;
SVGElement includes HTMLOrSVGElement;

[Exposed=Window,
 OverrideBuiltins]
interface DOMStringMap {
  getter DOMString (DOMString name);
  [CEReactions] setter void (DOMString name, DOMString value);
  [CEReactions] deleter void (DOMString name);
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLHtmlElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLHeadElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLTitleElement : HTMLElement {
  [CEReactions] attribute DOMString text;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLBaseElement : HTMLElement {
  [CEReactions] attribute USVString href;
  [CEReactions] attribute DOMString target;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLLinkElement : HTMLElement {
  [CEReactions] attribute USVString href;
  [CEReactions] attribute DOMString? crossOrigin;
  [CEReactions] attribute DOMString rel;
  [CEReactions] attribute DOMString as; // (default "")
  [SameObject, PutForwards=value] readonly attribute DOMTokenList relList;
  [CEReactions] attribute DOMString media;
  [CEReactions] attribute DOMString integrity;
  [CEReactions] attribute DOMString hreflang;
  [CEReactions] attribute DOMString type;
  [SameObject, PutForwards=value] readonly attribute DOMTokenList sizes;
  [CEReactions] attribute DOMString referrerPolicy;
};
HTMLLinkElement includes LinkStyle;

[Exposed=Window,
 HTMLConstructor]
interface HTMLMetaElement : HTMLElement {
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute DOMString httpEquiv;
  [CEReactions] attribute DOMString content;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLStyleElement : HTMLElement {
  [CEReactions] attribute DOMString media;
};
HTMLStyleElement includes LinkStyle;

[Exposed=Window,
 HTMLConstructor]
interface HTMLBodyElement : HTMLElement {};

HTMLBodyElement includes WindowEventHandlers;

[Exposed=Window,
 HTMLConstructor]
interface HTMLHeadingElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLParagraphElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLHRElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLPreElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLQuoteElement : HTMLElement {
  [CEReactions] attribute USVString cite;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLOListElement : HTMLElement {
  [CEReactions] attribute boolean reversed;
  [CEReactions] attribute long start;
  [CEReactions] attribute DOMString type;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLUListElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLMenuElement : HTMLElement {
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLLIElement : HTMLElement {
  [CEReactions] attribute long value;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLDListElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLDivElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLAnchorElement : HTMLElement {
  [CEReactions] attribute DOMString target;
  [CEReactions] attribute DOMString download;
  [CEReactions] attribute USVString ping;
  [CEReactions] attribute DOMString rel;
  [SameObject, PutForwards=value] readonly attribute DOMTokenList relList;
  [CEReactions] attribute DOMString hreflang;
  [CEReactions] attribute DOMString type;

  [CEReactions] attribute DOMString text;

  [CEReactions] attribute DOMString referrerPolicy;
};
HTMLAnchorElement includes HTMLHyperlinkElementUtils;

[Exposed=Window,
 HTMLConstructor]
interface HTMLDataElement : HTMLElement {
  [CEReactions] attribute DOMString value;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLTimeElement : HTMLElement {
  [CEReactions] attribute DOMString dateTime;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLSpanElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLBRElement : HTMLElement {};

interface mixin HTMLHyperlinkElementUtils {
  [CEReactions] stringifier attribute USVString href;
  readonly attribute USVString origin;
  [CEReactions] attribute USVString protocol;
  [CEReactions] attribute USVString username;
  [CEReactions] attribute USVString password;
  [CEReactions] attribute USVString host;
  [CEReactions] attribute USVString hostname;
  [CEReactions] attribute USVString port;
  [CEReactions] attribute USVString pathname;
  [CEReactions] attribute USVString search;
  [CEReactions] attribute USVString hash;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLModElement : HTMLElement {
  [CEReactions] attribute USVString cite;
  [CEReactions] attribute DOMString dateTime;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLPictureElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLSourceElement : HTMLElement {
  [CEReactions] attribute USVString src;
  [CEReactions] attribute DOMString type;
  [CEReactions] attribute USVString srcset;
  [CEReactions] attribute DOMString sizes;
  [CEReactions] attribute DOMString media;
};

[Exposed=Window,
 HTMLConstructor,
 NamedConstructor=Image(optional unsigned long width, optional unsigned long height)]
interface HTMLImageElement : HTMLElement {
  [CEReactions] attribute DOMString alt;
  [CEReactions] attribute USVString src;
  [CEReactions] attribute USVString srcset;
  [CEReactions] attribute DOMString sizes;
  [CEReactions] attribute DOMString? crossOrigin;
  [CEReactions] attribute DOMString useMap;
  [CEReactions] attribute boolean isMap;
  [CEReactions] attribute unsigned long width;
  [CEReactions] attribute unsigned long height;
  readonly attribute unsigned long naturalWidth;
  readonly attribute unsigned long naturalHeight;
  readonly attribute boolean complete;
  readonly attribute USVString currentSrc;
  [CEReactions] attribute DOMString referrerPolicy;
  [CEReactions] attribute DOMString decoding;

  Promise<void> decode();
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLIFrameElement : HTMLElement {
  [CEReactions] attribute USVString src;
  [CEReactions] attribute DOMString srcdoc;
  [CEReactions] attribute DOMString name;
  [SameObject, PutForwards=value] readonly attribute DOMTokenList sandbox;
  [CEReactions] attribute boolean allowFullscreen;
  [CEReactions] attribute boolean allowPaymentRequest;
  [CEReactions] attribute boolean allowUserMedia;
  [CEReactions] attribute DOMString width;
  [CEReactions] attribute DOMString height;
  [CEReactions] attribute DOMString referrerPolicy;
  readonly attribute Document? contentDocument;
  readonly attribute WindowProxy? contentWindow;
  Document? getSVGDocument();
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLEmbedElement : HTMLElement {
  [CEReactions] attribute USVString src;
  [CEReactions] attribute DOMString type;
  [CEReactions] attribute DOMString width;
  [CEReactions] attribute DOMString height;
  Document? getSVGDocument();
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLObjectElement : HTMLElement {
  [CEReactions] attribute USVString data;
  [CEReactions] attribute DOMString type;
  [CEReactions] attribute boolean typeMustMatch;
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute DOMString useMap;
  readonly attribute HTMLFormElement? form;
  [CEReactions] attribute DOMString width;
  [CEReactions] attribute DOMString height;
  readonly attribute Document? contentDocument;
  readonly attribute WindowProxy? contentWindow;
  Document? getSVGDocument();

  readonly attribute boolean willValidate;
  readonly attribute ValidityState validity;
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  void setCustomValidity(DOMString error);
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLParamElement : HTMLElement {
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute DOMString value;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLVideoElement : HTMLMediaElement {
  [CEReactions] attribute unsigned long width;
  [CEReactions] attribute unsigned long height;
  readonly attribute unsigned long videoWidth;
  readonly attribute unsigned long videoHeight;
  [CEReactions] attribute USVString poster;
  [CEReactions] attribute boolean playsInline;
};

[Exposed=Window,
 HTMLConstructor,
 NamedConstructor=Audio(optional DOMString src)]
interface HTMLAudioElement : HTMLMediaElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLTrackElement : HTMLElement {
  [CEReactions] attribute DOMString kind;
  [CEReactions] attribute USVString src;
  [CEReactions] attribute DOMString srclang;
  [CEReactions] attribute DOMString label;
  [CEReactions] attribute boolean default;

  const unsigned short NONE = 0;
  const unsigned short LOADING = 1;
  const unsigned short LOADED = 2;
  const unsigned short ERROR = 3;
  readonly attribute unsigned short readyState;

  readonly attribute TextTrack track;
};

enum CanPlayTypeResult { "" /* empty string */, "maybe", "probably" };
typedef (MediaStream or MediaSource or Blob) MediaProvider;

[Exposed=Window]
interface HTMLMediaElement : HTMLElement {

  // error state
  readonly attribute MediaError? error;

  // network state
  [CEReactions] attribute USVString src;
  attribute MediaProvider? srcObject;
  readonly attribute USVString currentSrc;
  [CEReactions] attribute DOMString? crossOrigin;
  const unsigned short NETWORK_EMPTY = 0;
  const unsigned short NETWORK_IDLE = 1;
  const unsigned short NETWORK_LOADING = 2;
  const unsigned short NETWORK_NO_SOURCE = 3;
  readonly attribute unsigned short networkState;
  [CEReactions] attribute DOMString preload;
  readonly attribute TimeRanges buffered;
  void load();
  CanPlayTypeResult canPlayType(DOMString type);

  // ready state
  const unsigned short HAVE_NOTHING = 0;
  const unsigned short HAVE_METADATA = 1;
  const unsigned short HAVE_CURRENT_DATA = 2;
  const unsigned short HAVE_FUTURE_DATA = 3;
  const unsigned short HAVE_ENOUGH_DATA = 4;
  readonly attribute unsigned short readyState;
  readonly attribute boolean seeking;

  // playback state
  attribute double currentTime;
  void fastSeek(double time);
  readonly attribute unrestricted double duration;
  object getStartDate();
  readonly attribute boolean paused;
  attribute double defaultPlaybackRate;
  attribute double playbackRate;
  readonly attribute TimeRanges played;
  readonly attribute TimeRanges seekable;
  readonly attribute boolean ended;
  [CEReactions] attribute boolean autoplay;
  [CEReactions] attribute boolean loop;
  Promise<void> play();
  void pause();

  // controls
  [CEReactions] attribute boolean controls;
  attribute double volume;
  attribute boolean muted;
  [CEReactions] attribute boolean defaultMuted;

  // tracks
  [SameObject] readonly attribute AudioTrackList audioTracks;
  [SameObject] readonly attribute VideoTrackList videoTracks;
  [SameObject] readonly attribute TextTrackList textTracks;
  TextTrack addTextTrack(TextTrackKind kind, optional DOMString label = "", optional DOMString language = "");
};

[Exposed=Window]
interface MediaError {
  const unsigned short MEDIA_ERR_ABORTED = 1;
  const unsigned short MEDIA_ERR_NETWORK = 2;
  const unsigned short MEDIA_ERR_DECODE = 3;
  const unsigned short MEDIA_ERR_SRC_NOT_SUPPORTED = 4;

  readonly attribute unsigned short code;
  readonly attribute DOMString message;
};

[Exposed=Window]
interface AudioTrackList : EventTarget {
  readonly attribute unsigned long length;
  getter AudioTrack (unsigned long index);
  AudioTrack? getTrackById(DOMString id);

  attribute EventHandler onchange;
  attribute EventHandler onaddtrack;
  attribute EventHandler onremovetrack;
};

[Exposed=Window]
interface AudioTrack {
  readonly attribute DOMString id;
  readonly attribute DOMString kind;
  readonly attribute DOMString label;
  readonly attribute DOMString language;
  attribute boolean enabled;
};

[Exposed=Window]
interface VideoTrackList : EventTarget {
  readonly attribute unsigned long length;
  getter VideoTrack (unsigned long index);
  VideoTrack? getTrackById(DOMString id);
  readonly attribute long selectedIndex;

  attribute EventHandler onchange;
  attribute EventHandler onaddtrack;
  attribute EventHandler onremovetrack;
};

[Exposed=Window]
interface VideoTrack {
  readonly attribute DOMString id;
  readonly attribute DOMString kind;
  readonly attribute DOMString label;
  readonly attribute DOMString language;
  attribute boolean selected;
};

[Exposed=Window]
interface TextTrackList : EventTarget {
  readonly attribute unsigned long length;
  getter TextTrack (unsigned long index);
  TextTrack? getTrackById(DOMString id);

  attribute EventHandler onchange;
  attribute EventHandler onaddtrack;
  attribute EventHandler onremovetrack;
};

enum TextTrackMode { "disabled",  "hidden",  "showing" };
enum TextTrackKind { "subtitles",  "captions",  "descriptions",  "chapters",  "metadata" };

[Exposed=Window]
interface TextTrack : EventTarget {
  readonly attribute TextTrackKind kind;
  readonly attribute DOMString label;
  readonly attribute DOMString language;

  readonly attribute DOMString id;
  readonly attribute DOMString inBandMetadataTrackDispatchType;

  attribute TextTrackMode mode;

  readonly attribute TextTrackCueList? cues;
  readonly attribute TextTrackCueList? activeCues;

  void addCue(TextTrackCue cue);
  void removeCue(TextTrackCue cue);

  attribute EventHandler oncuechange;
};

[Exposed=Window]
interface TextTrackCueList {
  readonly attribute unsigned long length;
  getter TextTrackCue (unsigned long index);
  TextTrackCue? getCueById(DOMString id);
};

[Exposed=Window]
interface TextTrackCue : EventTarget {
  readonly attribute TextTrack? track;

  attribute DOMString id;
  attribute double startTime;
  attribute double endTime;
  attribute boolean pauseOnExit;

  attribute EventHandler onenter;
  attribute EventHandler onexit;
};

[Exposed=Window]
interface TimeRanges {
  readonly attribute unsigned long length;
  double start(unsigned long index);
  double end(unsigned long index);
};

[Exposed=Window,
 Constructor(DOMString type, optional TrackEventInit eventInitDict)]
interface TrackEvent : Event {
  readonly attribute (VideoTrack or AudioTrack or TextTrack)? track;
};

dictionary TrackEventInit : EventInit {
  (VideoTrack or AudioTrack or TextTrack)? track = null;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLMapElement : HTMLElement {
  [CEReactions] attribute DOMString name;
  [SameObject] readonly attribute HTMLCollection areas;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLAreaElement : HTMLElement {
  [CEReactions] attribute DOMString alt;
  [CEReactions] attribute DOMString coords;
  [CEReactions] attribute DOMString shape;
  [CEReactions] attribute DOMString target;
  [CEReactions] attribute DOMString download;
  [CEReactions] attribute USVString ping;
  [CEReactions] attribute DOMString rel;
  [SameObject, PutForwards=value] readonly attribute DOMTokenList relList;
  [CEReactions] attribute DOMString referrerPolicy;
};
HTMLAreaElement includes HTMLHyperlinkElementUtils;

[Exposed=Window,
 HTMLConstructor]
interface HTMLTableElement : HTMLElement {
  [CEReactions] attribute HTMLTableCaptionElement? caption;
  HTMLTableCaptionElement createCaption();
  [CEReactions] void deleteCaption();

  [CEReactions] attribute HTMLTableSectionElement? tHead;
  HTMLTableSectionElement createTHead();
  [CEReactions] void deleteTHead();

  [CEReactions] attribute HTMLTableSectionElement? tFoot;
  HTMLTableSectionElement createTFoot();
  [CEReactions] void deleteTFoot();

  [SameObject] readonly attribute HTMLCollection tBodies;
  HTMLTableSectionElement createTBody();

  [SameObject] readonly attribute HTMLCollection rows;
  HTMLTableRowElement insertRow(optional long index = -1);
  [CEReactions] void deleteRow(long index);
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLTableCaptionElement : HTMLElement {};

[Exposed=Window,
 HTMLConstructor]
interface HTMLTableColElement : HTMLElement {
  [CEReactions] attribute unsigned long span;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLTableSectionElement : HTMLElement {
  [SameObject] readonly attribute HTMLCollection rows;
  HTMLTableRowElement insertRow(optional long index = -1);
  [CEReactions] void deleteRow(long index);
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLTableRowElement : HTMLElement {
  readonly attribute long rowIndex;
  readonly attribute long sectionRowIndex;
  [SameObject] readonly attribute HTMLCollection cells;
  HTMLTableCellElement insertCell(optional long index = -1);
  [CEReactions] void deleteCell(long index);
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLTableCellElement : HTMLElement {
  [CEReactions] attribute unsigned long colSpan;
  [CEReactions] attribute unsigned long rowSpan;
  [CEReactions] attribute DOMString headers;
  readonly attribute long cellIndex;

  [CEReactions] attribute DOMString scope; // only conforming for th elements
  [CEReactions] attribute DOMString abbr;  // only conforming for th elements
};

[Exposed=Window,
 OverrideBuiltins,
 LegacyUnenumerableNamedProperties,
 HTMLConstructor]
interface HTMLFormElement : HTMLElement {
  [CEReactions] attribute DOMString acceptCharset;
  [CEReactions] attribute USVString action;
  [CEReactions] attribute DOMString autocomplete;
  [CEReactions] attribute DOMString enctype;
  [CEReactions] attribute DOMString encoding;
  [CEReactions] attribute DOMString method;
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute boolean noValidate;
  [CEReactions] attribute DOMString target;

  [SameObject] readonly attribute HTMLFormControlsCollection elements;
  readonly attribute unsigned long length;
  getter Element (unsigned long index);
  getter (RadioNodeList or Element) (DOMString name);

  void submit();
  [CEReactions] void reset();
  boolean checkValidity();
  boolean reportValidity();
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLLabelElement : HTMLElement {
  readonly attribute HTMLFormElement? form;
  [CEReactions] attribute DOMString htmlFor;
  readonly attribute HTMLElement? control;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLInputElement : HTMLElement {
  [CEReactions] attribute DOMString accept;
  [CEReactions] attribute DOMString alt;
  [CEReactions] attribute DOMString autocomplete;
  [CEReactions] attribute boolean autofocus;
  [CEReactions] attribute boolean defaultChecked;
  attribute boolean checked;
  [CEReactions] attribute DOMString dirName;
  [CEReactions] attribute boolean disabled;
  readonly attribute HTMLFormElement? form;
  attribute FileList? files;
  [CEReactions] attribute USVString formAction;
  [CEReactions] attribute DOMString formEnctype;
  [CEReactions] attribute DOMString formMethod;
  [CEReactions] attribute boolean formNoValidate;
  [CEReactions] attribute DOMString formTarget;
  [CEReactions] attribute unsigned long height;
  attribute boolean indeterminate;
  readonly attribute HTMLElement? list;
  [CEReactions] attribute DOMString max;
  [CEReactions] attribute long maxLength;
  [CEReactions] attribute DOMString min;
  [CEReactions] attribute long minLength;
  [CEReactions] attribute boolean multiple;
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute DOMString pattern;
  [CEReactions] attribute DOMString placeholder;
  [CEReactions] attribute boolean readOnly;
  [CEReactions] attribute boolean required;
  [CEReactions] attribute unsigned long size;
  [CEReactions] attribute USVString src;
  [CEReactions] attribute DOMString step;
  [CEReactions] attribute DOMString type;
  [CEReactions] attribute DOMString defaultValue;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString value;
  attribute object? valueAsDate;
  attribute unrestricted double valueAsNumber;
  [CEReactions] attribute unsigned long width;

  void stepUp(optional long n = 1);
  void stepDown(optional long n = 1);

  readonly attribute boolean willValidate;
  readonly attribute ValidityState validity;
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  void setCustomValidity(DOMString error);

  readonly attribute NodeList? labels;

  void select();
  attribute unsigned long? selectionStart;
  attribute unsigned long? selectionEnd;
  attribute DOMString? selectionDirection;
  void setRangeText(DOMString replacement);
  void setRangeText(DOMString replacement, unsigned long start, unsigned long end, optional SelectionMode selectionMode = "preserve");
  void setSelectionRange(unsigned long start, unsigned long end, optional DOMString direction);
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLButtonElement : HTMLElement {
  [CEReactions] attribute boolean autofocus;
  [CEReactions] attribute boolean disabled;
  readonly attribute HTMLFormElement? form;
  [CEReactions] attribute USVString formAction;
  [CEReactions] attribute DOMString formEnctype;
  [CEReactions] attribute DOMString formMethod;
  [CEReactions] attribute boolean formNoValidate;
  [CEReactions] attribute DOMString formTarget;
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute DOMString type;
  [CEReactions] attribute DOMString value;

  readonly attribute boolean willValidate;
  readonly attribute ValidityState validity;
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  void setCustomValidity(DOMString error);

  readonly attribute NodeList labels;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLSelectElement : HTMLElement {
  [CEReactions] attribute DOMString autocomplete;
  [CEReactions] attribute boolean autofocus;
  [CEReactions] attribute boolean disabled;
  readonly attribute HTMLFormElement? form;
  [CEReactions] attribute boolean multiple;
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute boolean required;
  [CEReactions] attribute unsigned long size;

  readonly attribute DOMString type;

  [SameObject] readonly attribute HTMLOptionsCollection options;
  [CEReactions] attribute unsigned long length;
  getter Element? item(unsigned long index);
  HTMLOptionElement? namedItem(DOMString name);
  [CEReactions] void add((HTMLOptionElement or HTMLOptGroupElement) element, optional (HTMLElement or long)? before = null);
  [CEReactions] void remove(); // ChildNode overload
  [CEReactions] void remove(long index);
  [CEReactions] setter void (unsigned long index, HTMLOptionElement? option);

  [SameObject] readonly attribute HTMLCollection selectedOptions;
  attribute long selectedIndex;
  attribute DOMString value;

  readonly attribute boolean willValidate;
  readonly attribute ValidityState validity;
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  void setCustomValidity(DOMString error);

  readonly attribute NodeList labels;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLDataListElement : HTMLElement {
  [SameObject] readonly attribute HTMLCollection options;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLOptGroupElement : HTMLElement {
  [CEReactions] attribute boolean disabled;
  [CEReactions] attribute DOMString label;
};

[Exposed=Window,
 HTMLConstructor,
 NamedConstructor=Option(optional DOMString text = "", optional DOMString value, optional boolean defaultSelected = false, optional boolean selected = false)]
interface HTMLOptionElement : HTMLElement {
  [CEReactions] attribute boolean disabled;
  readonly attribute HTMLFormElement? form;
  [CEReactions] attribute DOMString label;
  [CEReactions] attribute boolean defaultSelected;
  attribute boolean selected;
  [CEReactions] attribute DOMString value;

  [CEReactions] attribute DOMString text;
  readonly attribute long index;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLTextAreaElement : HTMLElement {
  [CEReactions] attribute DOMString autocomplete;
  [CEReactions] attribute boolean autofocus;
  [CEReactions] attribute unsigned long cols;
  [CEReactions] attribute DOMString dirName;
  [CEReactions] attribute boolean disabled;
  readonly attribute HTMLFormElement? form;
  [CEReactions] attribute long maxLength;
  [CEReactions] attribute long minLength;
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute DOMString placeholder;
  [CEReactions] attribute boolean readOnly;
  [CEReactions] attribute boolean required;
  [CEReactions] attribute unsigned long rows;
  [CEReactions] attribute DOMString wrap;

  readonly attribute DOMString type;
  [CEReactions] attribute DOMString defaultValue;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString value;
  readonly attribute unsigned long textLength;

  readonly attribute boolean willValidate;
  readonly attribute ValidityState validity;
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  void setCustomValidity(DOMString error);

  readonly attribute NodeList labels;

  void select();
  attribute unsigned long selectionStart;
  attribute unsigned long selectionEnd;
  attribute DOMString selectionDirection;
  void setRangeText(DOMString replacement);
  void setRangeText(DOMString replacement, unsigned long start, unsigned long end, optional SelectionMode selectionMode = "preserve");
  void setSelectionRange(unsigned long start, unsigned long end, optional DOMString direction);
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLOutputElement : HTMLElement {
  [SameObject, PutForwards=value] readonly attribute DOMTokenList htmlFor;
  readonly attribute HTMLFormElement? form;
  [CEReactions] attribute DOMString name;

  readonly attribute DOMString type;
  [CEReactions] attribute DOMString defaultValue;
  [CEReactions] attribute DOMString value;

  readonly attribute boolean willValidate;
  readonly attribute ValidityState validity;
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  void setCustomValidity(DOMString error);

  readonly attribute NodeList labels;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLProgressElement : HTMLElement {
  [CEReactions] attribute double value;
  [CEReactions] attribute double max;
  readonly attribute double position;
  readonly attribute NodeList labels;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLMeterElement : HTMLElement {
  [CEReactions] attribute double value;
  [CEReactions] attribute double min;
  [CEReactions] attribute double max;
  [CEReactions] attribute double low;
  [CEReactions] attribute double high;
  [CEReactions] attribute double optimum;
  readonly attribute NodeList labels;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLFieldSetElement : HTMLElement {
  [CEReactions] attribute boolean disabled;
  readonly attribute HTMLFormElement? form;
  [CEReactions] attribute DOMString name;

  readonly attribute DOMString type;

  [SameObject] readonly attribute HTMLCollection elements;

  readonly attribute boolean willValidate;
  [SameObject] readonly attribute ValidityState validity;
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  void setCustomValidity(DOMString error);
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLLegendElement : HTMLElement {
  readonly attribute HTMLFormElement? form;
};

enum SelectionMode {
  "select",
  "start",
  "end",
  "preserve" // default
};

[Exposed=Window]
interface ValidityState {
  readonly attribute boolean valueMissing;
  readonly attribute boolean typeMismatch;
  readonly attribute boolean patternMismatch;
  readonly attribute boolean tooLong;
  readonly attribute boolean tooShort;
  readonly attribute boolean rangeUnderflow;
  readonly attribute boolean rangeOverflow;
  readonly attribute boolean stepMismatch;
  readonly attribute boolean badInput;
  readonly attribute boolean customError;
  readonly attribute boolean valid;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLDetailsElement : HTMLElement {
  [CEReactions] attribute boolean open;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLDialogElement : HTMLElement {
  [CEReactions] attribute boolean open;
  attribute DOMString returnValue;
  [CEReactions] void show();
  [CEReactions] void showModal();
  [CEReactions] void close(optional DOMString returnValue);
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLScriptElement : HTMLElement {
  [CEReactions] attribute USVString src;
  [CEReactions] attribute DOMString type;
  [CEReactions] attribute boolean noModule;
  [CEReactions] attribute boolean async;
  [CEReactions] attribute boolean defer;
  [CEReactions] attribute DOMString? crossOrigin;
  [CEReactions] attribute DOMString text;
  [CEReactions] attribute DOMString integrity;

};

[Exposed=Window,
 HTMLConstructor]
interface HTMLTemplateElement : HTMLElement {
  readonly attribute DocumentFragment content;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLSlotElement : HTMLElement {
  [CEReactions] attribute DOMString name;
  sequence<Node> assignedNodes(optional AssignedNodesOptions options);
  sequence<Element> assignedElements(optional AssignedNodesOptions options);
};

dictionary AssignedNodesOptions {
  boolean flatten = false;
};

typedef (CanvasRenderingContext2D or ImageBitmapRenderingContext or WebGLRenderingContext) RenderingContext;

[Exposed=Window,
 HTMLConstructor]
interface HTMLCanvasElement : HTMLElement {
  [CEReactions] attribute unsigned long width;
  [CEReactions] attribute unsigned long height;

  RenderingContext? getContext(DOMString contextId, optional any options = null);

  USVString toDataURL(optional DOMString type, optional any quality);
  void toBlob(BlobCallback _callback, optional DOMString type, optional any quality);
  OffscreenCanvas transferControlToOffscreen();
};

callback BlobCallback = void (Blob? blob);

typedef (HTMLImageElement or
         SVGImageElement) HTMLOrSVGImageElement;

typedef (HTMLOrSVGImageElement or
         HTMLVideoElement or
         HTMLCanvasElement or
         ImageBitmap or
         OffscreenCanvas) CanvasImageSource;

enum CanvasFillRule { "nonzero", "evenodd" };

dictionary CanvasRenderingContext2DSettings {
  boolean alpha = true;
};

enum ImageSmoothingQuality { "low", "medium", "high" };

[Exposed=Window]
interface CanvasRenderingContext2D {
  // back-reference to the canvas
  readonly attribute HTMLCanvasElement canvas;
};
CanvasRenderingContext2D includes CanvasState;
CanvasRenderingContext2D includes CanvasTransform;
CanvasRenderingContext2D includes CanvasCompositing;
CanvasRenderingContext2D includes CanvasImageSmoothing;
CanvasRenderingContext2D includes CanvasFillStrokeStyles;
CanvasRenderingContext2D includes CanvasShadowStyles;
CanvasRenderingContext2D includes CanvasFilters;
CanvasRenderingContext2D includes CanvasRect;
CanvasRenderingContext2D includes CanvasDrawPath;
CanvasRenderingContext2D includes CanvasUserInterface;
CanvasRenderingContext2D includes CanvasText;
CanvasRenderingContext2D includes CanvasDrawImage;
CanvasRenderingContext2D includes CanvasImageData;
CanvasRenderingContext2D includes CanvasPathDrawingStyles;
CanvasRenderingContext2D includes CanvasTextDrawingStyles;
CanvasRenderingContext2D includes CanvasPath;

interface mixin CanvasState {
  // state
  void save(); // push state on state stack
  void restore(); // pop state stack and restore state
};

interface mixin CanvasTransform {
  // transformations (default transform is the identity matrix)
  void scale(unrestricted double x, unrestricted double y);
  void rotate(unrestricted double angle);
  void translate(unrestricted double x, unrestricted double y);
  void transform(unrestricted double a, unrestricted double b, unrestricted double c, unrestricted double d, unrestricted double e, unrestricted double f);

  [NewObject] DOMMatrix getTransform();
  void setTransform(unrestricted double a, unrestricted double b, unrestricted double c, unrestricted double d, unrestricted double e, unrestricted double f);
  void setTransform(optional DOMMatrix2DInit transform);
  void resetTransform();

};

interface mixin CanvasCompositing {
  // compositing
  attribute unrestricted double globalAlpha; // (default 1.0)
  attribute DOMString globalCompositeOperation; // (default source-over)
};

interface mixin CanvasImageSmoothing {
  // image smoothing
  attribute boolean imageSmoothingEnabled; // (default true)
  attribute ImageSmoothingQuality imageSmoothingQuality; // (default low)

};

interface mixin CanvasFillStrokeStyles {
  // colors and styles (see also the CanvasPathDrawingStyles and CanvasTextDrawingStyles interfaces)
  attribute (DOMString or CanvasGradient or CanvasPattern) strokeStyle; // (default black)
  attribute (DOMString or CanvasGradient or CanvasPattern) fillStyle; // (default black)
  CanvasGradient createLinearGradient(double x0, double y0, double x1, double y1);
  CanvasGradient createRadialGradient(double x0, double y0, double r0, double x1, double y1, double r1);
  CanvasPattern? createPattern(CanvasImageSource image, [TreatNullAs=EmptyString] DOMString repetition);

};

interface mixin CanvasShadowStyles {
  // shadows
  attribute unrestricted double shadowOffsetX; // (default 0)
  attribute unrestricted double shadowOffsetY; // (default 0)
  attribute unrestricted double shadowBlur; // (default 0)
  attribute DOMString shadowColor; // (default transparent black)
};

interface mixin CanvasFilters {
  // filters
  attribute DOMString filter; // (default "none")
};

interface mixin CanvasRect {
  // rects
  void clearRect(unrestricted double x, unrestricted double y, unrestricted double w, unrestricted double h);
  void fillRect(unrestricted double x, unrestricted double y, unrestricted double w, unrestricted double h);
  void strokeRect(unrestricted double x, unrestricted double y, unrestricted double w, unrestricted double h);
};

interface mixin CanvasDrawPath {
  // path API (see also CanvasPath)
  void beginPath();
  void fill(optional CanvasFillRule fillRule = "nonzero");
  void fill(Path2D path, optional CanvasFillRule fillRule = "nonzero");
  void stroke();
  void stroke(Path2D path);
  void clip(optional CanvasFillRule fillRule = "nonzero");
  void clip(Path2D path, optional CanvasFillRule fillRule = "nonzero");
  void resetClip();
  boolean isPointInPath(unrestricted double x, unrestricted double y, optional CanvasFillRule fillRule = "nonzero");
  boolean isPointInPath(Path2D path, unrestricted double x, unrestricted double y, optional CanvasFillRule fillRule = "nonzero");
  boolean isPointInStroke(unrestricted double x, unrestricted double y);
  boolean isPointInStroke(Path2D path, unrestricted double x, unrestricted double y);
};

interface mixin CanvasUserInterface {
  void drawFocusIfNeeded(Element element);
  void drawFocusIfNeeded(Path2D path, Element element);
  void scrollPathIntoView();
  void scrollPathIntoView(Path2D path);
};

interface mixin CanvasText {
  // text (see also the CanvasPathDrawingStyles and CanvasTextDrawingStyles interfaces)
  void fillText(DOMString text, unrestricted double x, unrestricted double y, optional unrestricted double maxWidth);
  void strokeText(DOMString text, unrestricted double x, unrestricted double y, optional unrestricted double maxWidth);
  TextMetrics measureText(DOMString text);
};

interface mixin CanvasDrawImage {
  // drawing images
  void drawImage(CanvasImageSource image, unrestricted double dx, unrestricted double dy);
  void drawImage(CanvasImageSource image, unrestricted double dx, unrestricted double dy, unrestricted double dw, unrestricted double dh);
  void drawImage(CanvasImageSource image, unrestricted double sx, unrestricted double sy, unrestricted double sw, unrestricted double sh, unrestricted double dx, unrestricted double dy, unrestricted double dw, unrestricted double dh);
};

interface mixin CanvasImageData {
  // pixel manipulation
  ImageData createImageData(long sw, long sh);
  ImageData createImageData(ImageData imagedata);
  ImageData getImageData(long sx, long sy, long sw, long sh);
  void putImageData(ImageData imagedata, long dx, long dy);
  void putImageData(ImageData imagedata, long dx, long dy, long dirtyX, long dirtyY, long dirtyWidth, long dirtyHeight);
};

enum CanvasLineCap { "butt", "round", "square" };
enum CanvasLineJoin { "round", "bevel", "miter" };
enum CanvasTextAlign { "start", "end", "left", "right", "center" };
enum CanvasTextBaseline { "top", "hanging", "middle", "alphabetic", "ideographic", "bottom" };
enum CanvasDirection { "ltr", "rtl", "inherit" };

interface mixin CanvasPathDrawingStyles {
  // line caps/joins
  attribute unrestricted double lineWidth; // (default 1)
  attribute CanvasLineCap lineCap; // (default "butt")
  attribute CanvasLineJoin lineJoin; // (default "miter")
  attribute unrestricted double miterLimit; // (default 10)

  // dashed lines
  void setLineDash(sequence<unrestricted double> segments); // default empty
  sequence<unrestricted double> getLineDash();
  attribute unrestricted double lineDashOffset;
};

interface mixin CanvasTextDrawingStyles {
  // text
  attribute DOMString font; // (default 10px sans-serif)
  attribute CanvasTextAlign textAlign; // (default: "start")
  attribute CanvasTextBaseline textBaseline; // (default: "alphabetic")
  attribute CanvasDirection direction; // (default: "inherit")
};

interface mixin CanvasPath {
  // shared path API methods
  void closePath();
  void moveTo(unrestricted double x, unrestricted double y);
  void lineTo(unrestricted double x, unrestricted double y);
  void quadraticCurveTo(unrestricted double cpx, unrestricted double cpy, unrestricted double x, unrestricted double y);
  void bezierCurveTo(unrestricted double cp1x, unrestricted double cp1y, unrestricted double cp2x, unrestricted double cp2y, unrestricted double x, unrestricted double y);
  void arcTo(unrestricted double x1, unrestricted double y1, unrestricted double x2, unrestricted double y2, unrestricted double radius);
  void rect(unrestricted double x, unrestricted double y, unrestricted double w, unrestricted double h);
  void arc(unrestricted double x, unrestricted double y, unrestricted double radius, unrestricted double startAngle, unrestricted double endAngle, optional boolean anticlockwise = false);
  void ellipse(unrestricted double x, unrestricted double y, unrestricted double radiusX, unrestricted double radiusY, unrestricted double rotation, unrestricted double startAngle, unrestricted double endAngle, optional boolean anticlockwise = false);
};

[Exposed=(Window,Worker)]
interface CanvasGradient {
  // opaque object
  void addColorStop(double offset, DOMString color);
};

[Exposed=(Window,Worker)]
interface CanvasPattern {
  // opaque object
  void setTransform(optional DOMMatrix2DInit transform);
};

[Exposed=Window]
interface TextMetrics {
  // x-direction
  readonly attribute double width; // advance width
  readonly attribute double actualBoundingBoxLeft;
  readonly attribute double actualBoundingBoxRight;

  // y-direction
  readonly attribute double fontBoundingBoxAscent;
  readonly attribute double fontBoundingBoxDescent;
  readonly attribute double actualBoundingBoxAscent;
  readonly attribute double actualBoundingBoxDescent;
  readonly attribute double emHeightAscent;
  readonly attribute double emHeightDescent;
  readonly attribute double hangingBaseline;
  readonly attribute double alphabeticBaseline;
  readonly attribute double ideographicBaseline;
};

[Constructor(unsigned long sw, unsigned long sh),
 Constructor(Uint8ClampedArray data, unsigned long sw, optional unsigned long sh),
 Exposed=(Window,Worker),
 Serializable]
interface ImageData {
  readonly attribute unsigned long width;
  readonly attribute unsigned long height;
  readonly attribute Uint8ClampedArray data;
};

[Constructor(optional (Path2D or DOMString) path),
 Exposed=(Window,Worker)]
interface Path2D {
  void addPath(Path2D path, optional DOMMatrix2DInit transform);
};
Path2D includes CanvasPath;

[Exposed=Window]
interface ImageBitmapRenderingContext {
  readonly attribute HTMLCanvasElement canvas;
  void transferFromImageBitmap(ImageBitmap? bitmap);
};

dictionary ImageBitmapRenderingContextSettings {
  boolean alpha = true;
};

typedef (OffscreenCanvasRenderingContext2D or
        WebGLRenderingContext) OffscreenRenderingContext;

dictionary ImageEncodeOptions {
  DOMString type = "image/png";
  unrestricted double quality = 1.0;
};

enum OffscreenRenderingContextId { "2d", "webgl" };

[Constructor([EnforceRange] unsigned long long width, [EnforceRange] unsigned long long height), Exposed=(Window,Worker), Transferable]
interface OffscreenCanvas : EventTarget {
  attribute unsigned long long width;
  attribute unsigned long long height;

  OffscreenRenderingContext? getContext(OffscreenRenderingContextId contextId, optional any options = null);
  ImageBitmap transferToImageBitmap();
  Promise<Blob> convertToBlob(optional ImageEncodeOptions options);
};

[Exposed=(Window,Worker)]
interface OffscreenCanvasRenderingContext2D {
  void commit();
  readonly attribute OffscreenCanvas canvas;
};

OffscreenCanvasRenderingContext2D includes CanvasState;
OffscreenCanvasRenderingContext2D includes CanvasTransform;
OffscreenCanvasRenderingContext2D includes CanvasCompositing;
OffscreenCanvasRenderingContext2D includes CanvasImageSmoothing;
OffscreenCanvasRenderingContext2D includes CanvasFillStrokeStyles;
OffscreenCanvasRenderingContext2D includes CanvasShadowStyles;
OffscreenCanvasRenderingContext2D includes CanvasFilters;
OffscreenCanvasRenderingContext2D includes CanvasRect;
OffscreenCanvasRenderingContext2D includes CanvasDrawPath;
OffscreenCanvasRenderingContext2D includes CanvasDrawImage;
OffscreenCanvasRenderingContext2D includes CanvasImageData;
OffscreenCanvasRenderingContext2D includes CanvasPathDrawingStyles;
OffscreenCanvasRenderingContext2D includes CanvasPath;

[Exposed=Window]
interface CustomElementRegistry {
  [CEReactions] void define(DOMString name, Function constructor, optional ElementDefinitionOptions options);
  any get(DOMString name);
  Promise<void> whenDefined(DOMString name);
  [CEReactions] void upgrade(Node root);
};

dictionary ElementDefinitionOptions {
  DOMString extends;
};

dictionary FocusOptions {
  boolean preventScroll = false;
};

interface mixin ElementContentEditable {
  [CEReactions] attribute DOMString contentEditable;
  readonly attribute boolean isContentEditable;
  [CEReactions] attribute DOMString inputMode;
};

[Exposed=Window,
 Constructor]
interface DataTransfer {
  attribute DOMString dropEffect;
  attribute DOMString effectAllowed;

  [SameObject] readonly attribute DataTransferItemList items;

  void setDragImage(Element image, long x, long y);

  /* old interface */
  readonly attribute FrozenArray<DOMString> types;
  DOMString getData(DOMString format);
  void setData(DOMString format, DOMString data);
  void clearData(optional DOMString format);
  [SameObject] readonly attribute FileList files;
};

[Exposed=Window]
interface DataTransferItemList {
  readonly attribute unsigned long length;
  getter DataTransferItem (unsigned long index);
  DataTransferItem? add(DOMString data, DOMString type);
  DataTransferItem? add(File data);
  void remove(unsigned long index);
  void clear();
};

[Exposed=Window]
interface DataTransferItem {
  readonly attribute DOMString kind;
  readonly attribute DOMString type;
  void getAsString(FunctionStringCallback? _callback);
  File? getAsFile();
};

callback FunctionStringCallback = void (DOMString data);

[Exposed=Window,
 Constructor(DOMString type, optional DragEventInit eventInitDict)]
interface DragEvent : MouseEvent {
  readonly attribute DataTransfer? dataTransfer;
};

dictionary DragEventInit : MouseEventInit {
  DataTransfer? dataTransfer = null;
};

[Global=Window,
 Exposed=Window,
 LegacyUnenumerableNamedProperties]
interface Window : EventTarget {
  // the current browsing context
  [Unforgeable] readonly attribute WindowProxy window;
  [Replaceable] readonly attribute WindowProxy self;
  [Unforgeable] readonly attribute Document document;
  attribute DOMString name;
  [PutForwards=href, Unforgeable] readonly attribute Location location;
  readonly attribute History history;
  readonly attribute CustomElementRegistry customElements;
  [Replaceable] readonly attribute BarProp locationbar;
  [Replaceable] readonly attribute BarProp menubar;
  [Replaceable] readonly attribute BarProp personalbar;
  [Replaceable] readonly attribute BarProp scrollbars;
  [Replaceable] readonly attribute BarProp statusbar;
  [Replaceable] readonly attribute BarProp toolbar;
  attribute DOMString status;
  void close();
  readonly attribute boolean closed;
  void stop();
  void focus();
  void blur();

  // other browsing contexts
  [Replaceable] readonly attribute WindowProxy frames;
  [Replaceable] readonly attribute unsigned long length;
  [Unforgeable] readonly attribute WindowProxy? top;
  attribute any opener;
  [Replaceable] readonly attribute WindowProxy? parent;
  readonly attribute Element? frameElement;
  WindowProxy? open(optional USVString url = "about:blank", optional DOMString target = "_blank", optional [TreatNullAs=EmptyString] DOMString features = "");
  getter object (DOMString name);
  // Since this is the global object, the IDL named getter adds a NamedPropertiesObject exotic
  // object on the prototype chain. Indeed, this does not make the global object an exotic object.
  // Indexed access is taken care of by the WindowProxy exotic object.

  // the user agent
  readonly attribute Navigator navigator;
  readonly attribute ApplicationCache applicationCache;

  // user prompts
  void alert();
  void alert(DOMString message);
  boolean confirm(optional DOMString message = "");
  DOMString? prompt(optional DOMString message = "", optional DOMString default = "");
  void print();

  unsigned long requestAnimationFrame(FrameRequestCallback callback);
  void cancelAnimationFrame(unsigned long handle);

  void postMessage(any message, USVString targetOrigin, optional sequence<object> transfer = []);
};
Window includes GlobalEventHandlers;
Window includes WindowEventHandlers;

callback FrameRequestCallback = void (DOMHighResTimeStamp time);

[Exposed=Window]
interface BarProp {
  readonly attribute boolean visible;
};

enum ScrollRestoration { "auto", "manual" };

[Exposed=Window]
interface History {
  readonly attribute unsigned long length;
  attribute ScrollRestoration scrollRestoration;
  readonly attribute any state;
  void go(optional long delta = 0);
  void back();
  void forward();
  void pushState(any data, DOMString title, optional USVString? url = null);
  void replaceState(any data, DOMString title, optional USVString? url = null);
};

[Exposed=Window]
interface Location { // but see also additional creation steps and overridden internal methods
  [Unforgeable] stringifier attribute USVString href;
  [Unforgeable] readonly attribute USVString origin;
  [Unforgeable] attribute USVString protocol;
  [Unforgeable] attribute USVString host;
  [Unforgeable] attribute USVString hostname;
  [Unforgeable] attribute USVString port;
  [Unforgeable] attribute USVString pathname;
  [Unforgeable] attribute USVString search;
  [Unforgeable] attribute USVString hash;

  [Unforgeable] void assign(USVString url);
  [Unforgeable] void replace(USVString url);
  [Unforgeable] void reload();

  [Unforgeable, SameObject] readonly attribute DOMStringList ancestorOrigins;
};

[Exposed=Window,
 Constructor(DOMString type, optional PopStateEventInit eventInitDict)]
interface PopStateEvent : Event {
  readonly attribute any state;
};

dictionary PopStateEventInit : EventInit {
  any state = null;
};

[Exposed=Window,
 Constructor(DOMString type, optional HashChangeEventInit eventInitDict)]
interface HashChangeEvent : Event {
  readonly attribute USVString oldURL;
  readonly attribute USVString newURL;
};

dictionary HashChangeEventInit : EventInit {
  USVString oldURL = "";
  USVString newURL = "";
};

[Exposed=Window,
 Constructor(DOMString type, optional PageTransitionEventInit eventInitDict)]
interface PageTransitionEvent : Event {
  readonly attribute boolean persisted;
};

dictionary PageTransitionEventInit : EventInit {
  boolean persisted = false;
};

[Exposed=Window]
interface BeforeUnloadEvent : Event {
  attribute DOMString returnValue;
};

[Exposed=Window]
interface ApplicationCache : EventTarget {

  // update status
  const unsigned short UNCACHED = 0;
  const unsigned short IDLE = 1;
  const unsigned short CHECKING = 2;
  const unsigned short DOWNLOADING = 3;
  const unsigned short UPDATEREADY = 4;
  const unsigned short OBSOLETE = 5;
  readonly attribute unsigned short status;

  // updates
  void update();
  void abort();
  void swapCache();

  // events
  attribute EventHandler onchecking;
  attribute EventHandler onerror;
  attribute EventHandler onnoupdate;
  attribute EventHandler ondownloading;
  attribute EventHandler onprogress;
  attribute EventHandler onupdateready;
  attribute EventHandler oncached;
  attribute EventHandler onobsolete;
};

interface mixin NavigatorOnLine {
  readonly attribute boolean onLine;
};

[Constructor(DOMString type, optional ErrorEventInit eventInitDict), Exposed=(Window,Worker)]
interface ErrorEvent : Event {
  readonly attribute DOMString message;
  readonly attribute USVString filename;
  readonly attribute unsigned long lineno;
  readonly attribute unsigned long colno;
  readonly attribute any error;
};

dictionary ErrorEventInit : EventInit {
  DOMString message = "";
  USVString filename = "";
  unsigned long lineno = 0;
  unsigned long colno = 0;
  any error = null;
};

[Constructor(DOMString type, PromiseRejectionEventInit eventInitDict), Exposed=(Window,Worker)]
interface PromiseRejectionEvent : Event {
  readonly attribute Promise<any> promise;
  readonly attribute any reason;
};

dictionary PromiseRejectionEventInit : EventInit {
  required Promise<any> promise;
  any reason;
};

[TreatNonObjectAsNull]
callback EventHandlerNonNull = any (Event event);
typedef EventHandlerNonNull? EventHandler;

[TreatNonObjectAsNull]
callback OnErrorEventHandlerNonNull = any ((Event or DOMString) event, optional DOMString source, optional unsigned long lineno, optional unsigned long colno, optional any error);
typedef OnErrorEventHandlerNonNull? OnErrorEventHandler;

[TreatNonObjectAsNull]
callback OnBeforeUnloadEventHandlerNonNull = DOMString? (Event event);
typedef OnBeforeUnloadEventHandlerNonNull? OnBeforeUnloadEventHandler;

interface mixin GlobalEventHandlers {
  attribute EventHandler onabort;
  attribute EventHandler onauxclick;
  attribute EventHandler onblur;
  attribute EventHandler oncancel;
  attribute EventHandler oncanplay;
  attribute EventHandler oncanplaythrough;
  attribute EventHandler onchange;
  attribute EventHandler onclick;
  attribute EventHandler onclose;
  attribute EventHandler oncontextmenu;
  attribute EventHandler oncuechange;
  attribute EventHandler ondblclick;
  attribute EventHandler ondrag;
  attribute EventHandler ondragend;
  attribute EventHandler ondragenter;
  attribute EventHandler ondragexit;
  attribute EventHandler ondragleave;
  attribute EventHandler ondragover;
  attribute EventHandler ondragstart;
  attribute EventHandler ondrop;
  attribute EventHandler ondurationchange;
  attribute EventHandler onemptied;
  attribute EventHandler onended;
  attribute OnErrorEventHandler onerror;
  attribute EventHandler onfocus;
  attribute EventHandler oninput;
  attribute EventHandler oninvalid;
  attribute EventHandler onkeydown;
  attribute EventHandler onkeypress;
  attribute EventHandler onkeyup;
  attribute EventHandler onload;
  attribute EventHandler onloadeddata;
  attribute EventHandler onloadedmetadata;
  attribute EventHandler onloadend;
  attribute EventHandler onloadstart;
  attribute EventHandler onmousedown;
  [LenientThis] attribute EventHandler onmouseenter;
  [LenientThis] attribute EventHandler onmouseleave;
  attribute EventHandler onmousemove;
  attribute EventHandler onmouseout;
  attribute EventHandler onmouseover;
  attribute EventHandler onmouseup;
  attribute EventHandler onwheel;
  attribute EventHandler onpause;
  attribute EventHandler onplay;
  attribute EventHandler onplaying;
  attribute EventHandler onprogress;
  attribute EventHandler onratechange;
  attribute EventHandler onreset;
  attribute EventHandler onresize;
  attribute EventHandler onscroll;
  attribute EventHandler onsecuritypolicyviolation;
  attribute EventHandler onseeked;
  attribute EventHandler onseeking;
  attribute EventHandler onselect;
  attribute EventHandler onstalled;
  attribute EventHandler onsubmit;
  attribute EventHandler onsuspend;
  attribute EventHandler ontimeupdate;
  attribute EventHandler ontoggle;
  attribute EventHandler onvolumechange;
  attribute EventHandler onwaiting;
};

interface mixin WindowEventHandlers {
  attribute EventHandler onafterprint;
  attribute EventHandler onbeforeprint;
  attribute OnBeforeUnloadEventHandler onbeforeunload;
  attribute EventHandler onhashchange;
  attribute EventHandler onlanguagechange;
  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
  attribute EventHandler onoffline;
  attribute EventHandler ononline;
  attribute EventHandler onpagehide;
  attribute EventHandler onpageshow;
  attribute EventHandler onpopstate;
  attribute EventHandler onrejectionhandled;
  attribute EventHandler onstorage;
  attribute EventHandler onunhandledrejection;
  attribute EventHandler onunload;
};

interface mixin DocumentAndElementEventHandlers {
  attribute EventHandler oncopy;
  attribute EventHandler oncut;
  attribute EventHandler onpaste;
};

typedef (DOMString or Function) TimerHandler;

interface mixin WindowOrWorkerGlobalScope {
  [Replaceable] readonly attribute USVString origin;

  // base64 utility methods
  DOMString btoa(DOMString data);
  ByteString atob(DOMString data);

  // timers
  long setTimeout(TimerHandler handler, optional long timeout = 0, any... arguments);
  void clearTimeout(optional long handle = 0);
  long setInterval(TimerHandler handler, optional long timeout = 0, any... arguments);
  void clearInterval(optional long handle = 0);

  // ImageBitmap
  Promise<ImageBitmap> createImageBitmap(ImageBitmapSource image, optional ImageBitmapOptions options);
  Promise<ImageBitmap> createImageBitmap(ImageBitmapSource image, long sx, long sy, long sw, long sh, optional ImageBitmapOptions options);
};
Window includes WindowOrWorkerGlobalScope;
WorkerGlobalScope includes WindowOrWorkerGlobalScope;

[Exposed=Window]
interface Navigator {
  // objects implementing this interface also implement the interfaces given below
};
Navigator includes NavigatorID;
Navigator includes NavigatorLanguage;
Navigator includes NavigatorOnLine;
Navigator includes NavigatorContentUtils;
Navigator includes NavigatorCookies;
Navigator includes NavigatorPlugins;
Navigator includes NavigatorConcurrentHardware;

interface mixin NavigatorID {
  readonly attribute DOMString appCodeName; // constant "Mozilla"
  readonly attribute DOMString appName; // constant "Netscape"
  readonly attribute DOMString appVersion;
  readonly attribute DOMString platform;
  readonly attribute DOMString product; // constant "Gecko"
  [Exposed=Window] readonly attribute DOMString productSub;
  readonly attribute DOMString userAgent;
  [Exposed=Window] readonly attribute DOMString vendor;
  [Exposed=Window] readonly attribute DOMString vendorSub; // constant ""
};

partial interface NavigatorID {
  [Exposed=Window] boolean taintEnabled(); // constant false
  [Exposed=Window] readonly attribute DOMString oscpu;
};

interface mixin NavigatorLanguage {
  readonly attribute DOMString language;
  readonly attribute FrozenArray<DOMString> languages;
};

interface mixin NavigatorContentUtils {
  void registerProtocolHandler(DOMString scheme, USVString url, DOMString title);
  void unregisterProtocolHandler(DOMString scheme, USVString url);
};

interface mixin NavigatorCookies {
  readonly attribute boolean cookieEnabled;
};

interface mixin NavigatorPlugins {
  [SameObject] readonly attribute PluginArray plugins;
  [SameObject] readonly attribute MimeTypeArray mimeTypes;
  boolean javaEnabled();
};

[Exposed=Window,
 LegacyUnenumerableNamedProperties]
interface PluginArray {
  void refresh(optional boolean reload = false);
  readonly attribute unsigned long length;
  getter Plugin? item(unsigned long index);
  getter Plugin? namedItem(DOMString name);
};

[Exposed=Window,
 LegacyUnenumerableNamedProperties]
interface MimeTypeArray {
  readonly attribute unsigned long length;
  getter MimeType? item(unsigned long index);
  getter MimeType? namedItem(DOMString name);
};

[Exposed=Window,
 LegacyUnenumerableNamedProperties]
interface Plugin {
  readonly attribute DOMString name;
  readonly attribute DOMString description;
  readonly attribute DOMString filename;
  readonly attribute unsigned long length;
  getter MimeType? item(unsigned long index);
  getter MimeType? namedItem(DOMString name);
};

[Exposed=Window]
interface MimeType {
  readonly attribute DOMString type;
  readonly attribute DOMString description;
  readonly attribute DOMString suffixes; // comma-separated
  readonly attribute Plugin enabledPlugin;
};

[Exposed=(Window,Worker), Serializable, Transferable]
interface ImageBitmap {
  readonly attribute unsigned long width;
  readonly attribute unsigned long height;
  void close();
};

typedef (CanvasImageSource or
         Blob or
         ImageData) ImageBitmapSource;

enum ImageOrientation { "none", "flipY" };
enum PremultiplyAlpha { "none", "premultiply", "default" };
enum ColorSpaceConversion { "none", "default" };
enum ResizeQuality { "pixelated", "low", "medium", "high" };

dictionary ImageBitmapOptions {
  ImageOrientation imageOrientation = "none";
  PremultiplyAlpha premultiplyAlpha = "default";
  ColorSpaceConversion colorSpaceConversion = "default";
  [EnforceRange] unsigned long resizeWidth;
  [EnforceRange] unsigned long resizeHeight;
  ResizeQuality resizeQuality = "low";
};

[Constructor(DOMString type, optional MessageEventInit eventInitDict), Exposed=(Window,Worker,AudioWorklet)]
interface MessageEvent : Event {
  readonly attribute any data;
  readonly attribute USVString origin;
  readonly attribute DOMString lastEventId;
  readonly attribute MessageEventSource? source;
  readonly attribute FrozenArray<MessagePort> ports;

  void initMessageEvent(DOMString type, optional boolean bubbles = false, optional boolean cancelable = false, optional any data = null, optional USVString origin = "", optional DOMString lastEventId = "", optional MessageEventSource? source = null, optional sequence<MessagePort> ports = []);
};

dictionary MessageEventInit : EventInit {
  any data = null;
  USVString origin = "";
  DOMString lastEventId = "";
  MessageEventSource? source = null;
  sequence<MessagePort> ports = [];
};

typedef (WindowProxy or MessagePort or ServiceWorker) MessageEventSource;

[Constructor(USVString url, optional EventSourceInit eventSourceInitDict), Exposed=(Window,Worker)]
interface EventSource : EventTarget {
  readonly attribute USVString url;
  readonly attribute boolean withCredentials;

  // ready state
  const unsigned short CONNECTING = 0;
  const unsigned short OPEN = 1;
  const unsigned short CLOSED = 2;
  readonly attribute unsigned short readyState;

  // networking
  attribute EventHandler onopen;
  attribute EventHandler onmessage;
  attribute EventHandler onerror;
  void close();
};

dictionary EventSourceInit {
  boolean withCredentials = false;
};

enum BinaryType { "blob", "arraybuffer" };
[Constructor(USVString url, optional (DOMString or sequence<DOMString>) protocols = []), Exposed=(Window,Worker)]
interface WebSocket : EventTarget {
  readonly attribute USVString url;

  // ready state
  const unsigned short CONNECTING = 0;
  const unsigned short OPEN = 1;
  const unsigned short CLOSING = 2;
  const unsigned short CLOSED = 3;
  readonly attribute unsigned short readyState;
  readonly attribute unsigned long long bufferedAmount;

  // networking
  attribute EventHandler onopen;
  attribute EventHandler onerror;
  attribute EventHandler onclose;
  readonly attribute DOMString extensions;
  readonly attribute DOMString protocol;
  void close(optional [Clamp] unsigned short code, optional USVString reason);

  // messaging
  attribute EventHandler onmessage;
  attribute BinaryType binaryType;
  void send(USVString data);
  void send(Blob data);
  void send(ArrayBuffer data);
  void send(ArrayBufferView data);
};

[Constructor(DOMString type, optional CloseEventInit eventInitDict), Exposed=(Window,Worker)]
interface CloseEvent : Event {
  readonly attribute boolean wasClean;
  readonly attribute unsigned short code;
  readonly attribute USVString reason;
};

dictionary CloseEventInit : EventInit {
  boolean wasClean = false;
  unsigned short code = 0;
  USVString reason = "";
};

[Constructor, Exposed=(Window,Worker)]
interface MessageChannel {
  readonly attribute MessagePort port1;
  readonly attribute MessagePort port2;
};

[Exposed=(Window,Worker,AudioWorklet), Transferable]
interface MessagePort : EventTarget {
  void postMessage(any message, optional sequence<object> transfer = []);
  void start();
  void close();

  // event handlers
  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
};

[Constructor(DOMString name), Exposed=(Window,Worker)]
interface BroadcastChannel : EventTarget {
  readonly attribute DOMString name;
  void postMessage(any message);
  void close();
  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
};

[Exposed=Worker]
interface WorkerGlobalScope : EventTarget {
  readonly attribute WorkerGlobalScope self;
  readonly attribute WorkerLocation location;
  readonly attribute WorkerNavigator navigator;
  void importScripts(USVString... urls);

  attribute OnErrorEventHandler onerror;
  attribute EventHandler onlanguagechange;
  attribute EventHandler onoffline;
  attribute EventHandler ononline;
  attribute EventHandler onrejectionhandled;
  attribute EventHandler onunhandledrejection;
};

[Global=(Worker,DedicatedWorker),Exposed=DedicatedWorker]
interface DedicatedWorkerGlobalScope : WorkerGlobalScope {
  [Replaceable] readonly attribute DOMString name;

  void postMessage(any message, optional sequence<object> transfer = []);

  void close();

  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
};

[Global=(Worker,SharedWorker),Exposed=SharedWorker]
interface SharedWorkerGlobalScope : WorkerGlobalScope {
  [Replaceable] readonly attribute DOMString name;

  void close();

  attribute EventHandler onconnect;
};

interface mixin AbstractWorker {
  attribute EventHandler onerror;
};

[Constructor(USVString scriptURL, optional WorkerOptions options), Exposed=(Window,Worker)]
interface Worker : EventTarget {
  void terminate();

  void postMessage(any message, optional sequence<object> transfer = []);
  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
};

dictionary WorkerOptions {
  WorkerType type = "classic";
  RequestCredentials credentials = "omit"; // credentials is only used if type is "module"
  DOMString name = "";
};

enum WorkerType { "classic", "module" };

Worker includes AbstractWorker;

[Constructor(USVString scriptURL, optional (DOMString or WorkerOptions) options),
 Exposed=(Window,Worker)]
interface SharedWorker : EventTarget {
  readonly attribute MessagePort port;
};
SharedWorker includes AbstractWorker;

interface mixin NavigatorConcurrentHardware {
  readonly attribute unsigned long long hardwareConcurrency;
};

[Exposed=Worker]
interface WorkerNavigator {};
WorkerNavigator includes NavigatorID;
WorkerNavigator includes NavigatorLanguage;
WorkerNavigator includes NavigatorOnLine;
WorkerNavigator includes NavigatorConcurrentHardware;

[Exposed=Worker]
interface WorkerLocation {
  stringifier readonly attribute USVString href;
  readonly attribute USVString origin;
  readonly attribute USVString protocol;
  readonly attribute USVString host;
  readonly attribute USVString hostname;
  readonly attribute USVString port;
  readonly attribute USVString pathname;
  readonly attribute USVString search;
  readonly attribute USVString hash;
};

[Exposed=Window]
interface Storage {
  readonly attribute unsigned long length;
  DOMString? key(unsigned long index);
  getter DOMString? getItem(DOMString key);
  setter void setItem(DOMString key, DOMString value);
  deleter void removeItem(DOMString key);
  void clear();
};

interface mixin WindowSessionStorage {
  readonly attribute Storage sessionStorage;
};
Window includes WindowSessionStorage;

interface mixin WindowLocalStorage {
  readonly attribute Storage localStorage;
};
Window includes WindowLocalStorage;

[Exposed=Window,
 Constructor(DOMString type, optional StorageEventInit eventInitDict)]
interface StorageEvent : Event {
  readonly attribute DOMString? key;
  readonly attribute DOMString? oldValue;
  readonly attribute DOMString? newValue;
  readonly attribute USVString url;
  readonly attribute Storage? storageArea;
};

dictionary StorageEventInit : EventInit {
  DOMString? key = null;
  DOMString? oldValue = null;
  DOMString? newValue = null;
  USVString url = "";
  Storage? storageArea = null;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLMarqueeElement : HTMLElement {
  [CEReactions] attribute DOMString behavior;
  [CEReactions] attribute DOMString bgColor;
  [CEReactions] attribute DOMString direction;
  [CEReactions] attribute DOMString height;
  [CEReactions] attribute unsigned long hspace;
  [CEReactions] attribute long loop;
  [CEReactions] attribute unsigned long scrollAmount;
  [CEReactions] attribute unsigned long scrollDelay;
  [CEReactions] attribute boolean trueSpeed;
  [CEReactions] attribute unsigned long vspace;
  [CEReactions] attribute DOMString width;

  attribute EventHandler onbounce;
  attribute EventHandler onfinish;
  attribute EventHandler onstart;

  void start();
  void stop();
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLFrameSetElement : HTMLElement {
  [CEReactions] attribute DOMString cols;
  [CEReactions] attribute DOMString rows;
};
HTMLFrameSetElement includes WindowEventHandlers;

[Exposed=Window,
 HTMLConstructor]
interface HTMLFrameElement : HTMLElement {
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute DOMString scrolling;
  [CEReactions] attribute USVString src;
  [CEReactions] attribute DOMString frameBorder;
  [CEReactions] attribute USVString longDesc;
  [CEReactions] attribute boolean noResize;
  readonly attribute Document? contentDocument;
  readonly attribute WindowProxy? contentWindow;

  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString marginHeight;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString marginWidth;
};

partial interface HTMLAnchorElement {
  [CEReactions] attribute DOMString coords;
  [CEReactions] attribute DOMString charset;
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute DOMString rev;
  [CEReactions] attribute DOMString shape;
};

partial interface HTMLAreaElement {
  [CEReactions] attribute boolean noHref;
};

partial interface HTMLBodyElement {
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString text;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString link;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString vLink;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString aLink;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString bgColor;
  [CEReactions] attribute DOMString background;
};

partial interface HTMLBRElement {
  [CEReactions] attribute DOMString clear;
};

partial interface HTMLTableCaptionElement {
  [CEReactions] attribute DOMString align;
};

partial interface HTMLTableColElement {
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute DOMString ch;
  [CEReactions] attribute DOMString chOff;
  [CEReactions] attribute DOMString vAlign;
  [CEReactions] attribute DOMString width;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLDirectoryElement : HTMLElement {
  [CEReactions] attribute boolean compact;
};

partial interface HTMLDivElement {
  [CEReactions] attribute DOMString align;
};

partial interface HTMLDListElement {
  [CEReactions] attribute boolean compact;
};

partial interface HTMLEmbedElement {
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute DOMString name;
};

[Exposed=Window,
 HTMLConstructor]
interface HTMLFontElement : HTMLElement {
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString color;
  [CEReactions] attribute DOMString face;
  [CEReactions] attribute DOMString size;
};

partial interface HTMLHeadingElement {
  [CEReactions] attribute DOMString align;
};

partial interface HTMLHRElement {
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute DOMString color;
  [CEReactions] attribute boolean noShade;
  [CEReactions] attribute DOMString size;
  [CEReactions] attribute DOMString width;
};

partial interface HTMLHtmlElement {
  [CEReactions] attribute DOMString version;
};

partial interface HTMLIFrameElement {
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute DOMString scrolling;
  [CEReactions] attribute DOMString frameBorder;
  [CEReactions] attribute USVString longDesc;

  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString marginHeight;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString marginWidth;
};

partial interface HTMLImageElement {
  [CEReactions] attribute DOMString name;
  [CEReactions] attribute USVString lowsrc;
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute unsigned long hspace;
  [CEReactions] attribute unsigned long vspace;
  [CEReactions] attribute USVString longDesc;

  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString border;
};

partial interface HTMLInputElement {
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute DOMString useMap;
};

partial interface HTMLLegendElement {
  [CEReactions] attribute DOMString align;
};

partial interface HTMLLIElement {
  [CEReactions] attribute DOMString type;
};

partial interface HTMLLinkElement {
  [CEReactions] attribute DOMString charset;
  [CEReactions] attribute DOMString rev;
  [CEReactions] attribute DOMString target;
};

partial interface HTMLMenuElement {
  [CEReactions] attribute boolean compact;
};

partial interface HTMLMetaElement {
  [CEReactions] attribute DOMString scheme;
};

partial interface HTMLObjectElement {
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute DOMString archive;
  [CEReactions] attribute DOMString code;
  [CEReactions] attribute boolean declare;
  [CEReactions] attribute unsigned long hspace;
  [CEReactions] attribute DOMString standby;
  [CEReactions] attribute unsigned long vspace;
  [CEReactions] attribute DOMString codeBase;
  [CEReactions] attribute DOMString codeType;

  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString border;
};

partial interface HTMLOListElement {
  [CEReactions] attribute boolean compact;
};

partial interface HTMLParagraphElement {
  [CEReactions] attribute DOMString align;
};

partial interface HTMLParamElement {
  [CEReactions] attribute DOMString type;
  [CEReactions] attribute DOMString valueType;
};

partial interface HTMLPreElement {
  [CEReactions] attribute long width;
};

partial interface HTMLStyleElement {
  [CEReactions] attribute DOMString type;
};

partial interface HTMLScriptElement {
  [CEReactions] attribute DOMString charset;
  [CEReactions] attribute DOMString event;
  [CEReactions] attribute DOMString htmlFor;
};

partial interface HTMLTableElement {
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute DOMString border;
  [CEReactions] attribute DOMString frame;
  [CEReactions] attribute DOMString rules;
  [CEReactions] attribute DOMString summary;
  [CEReactions] attribute DOMString width;

  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString bgColor;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString cellPadding;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString cellSpacing;
};

partial interface HTMLTableSectionElement {
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute DOMString ch;
  [CEReactions] attribute DOMString chOff;
  [CEReactions] attribute DOMString vAlign;
};

partial interface HTMLTableCellElement {
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute DOMString axis;
  [CEReactions] attribute DOMString height;
  [CEReactions] attribute DOMString width;

  [CEReactions] attribute DOMString ch;
  [CEReactions] attribute DOMString chOff;
  [CEReactions] attribute boolean noWrap;
  [CEReactions] attribute DOMString vAlign;

  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString bgColor;
};

partial interface HTMLTableRowElement {
  [CEReactions] attribute DOMString align;
  [CEReactions] attribute DOMString ch;
  [CEReactions] attribute DOMString chOff;
  [CEReactions] attribute DOMString vAlign;

  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString bgColor;
};

partial interface HTMLUListElement {
  [CEReactions] attribute boolean compact;
  [CEReactions] attribute DOMString type;
};

partial interface Document {
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString fgColor;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString linkColor;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString vlinkColor;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString alinkColor;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString bgColor;

  [SameObject] readonly attribute HTMLCollection anchors;
  [SameObject] readonly attribute HTMLCollection applets;

  void clear();
  void captureEvents();
  void releaseEvents();

  [SameObject] readonly attribute HTMLAllCollection all;
};

partial interface Window {
  void captureEvents();
  void releaseEvents();

  [Replaceable, SameObject] readonly attribute External external;
};

[Exposed=Window,
 NoInterfaceObject]
interface External {
  void AddSearchProvider();
  void IsSearchProviderInstalled();
};
