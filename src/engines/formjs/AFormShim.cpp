// SPDX-License-Identifier: Apache-2.0
// The embedded AF shim source. See AFormShim.h for the contract and
// attribution. Port discipline: function bodies follow Mozilla pdf.js
// `aform.js`/`util.js` (Apache-2.0) as closely as the host contract allows;
// deliberate deviations are marked with "DEVIATION:" comments.

namespace gp::formjs {

const char* aformShimVersion()
{
    // pdf.js master (fetched 2026-09-09); Phase-1 shim tier.
    return "pdf.js scripting_api aform.js/util.js @ 2026-09-09, Phase-1 tier";
}

const char* aformShimSource()
{
    return R"gpjs(
// ── Ported from Mozilla pdf.js (Apache-2.0) ─────────────────────────────────
// src/shared/scripting_utils.js DateFormats/TimeFormats, src/scripting_api/
// util.js (printf/printd/scand), src/scripting_api/aform.js (AF subset).
// Copyright 2020 Mozilla Foundation — ported with attribution.

"use strict";

if (typeof Math.sumPrecise !== "function") {
  // quickjs-ng 0.15.0 ships Math.sumPrecise; the guarded fallback keeps the
  // shim portable to engines without it (reduce-based, not exactly-rounded —
  // only reachable on engines without the intrinsic).
  Math.sumPrecise = function (args) {
    return args.reduce((acc, value) => acc + value, 0);
  };
}

const MathClamp = (x, min, max) => Math.min(Math.max(x, min), max);

// DEVIATION: pdf.js splits these across classes with private fields; the port
// keeps one closure-scoped module and installs plain globals (the Acrobat
// dialect exposes AF* as globals and `this` === the document at top level,
// which sloppy-mode scripts expect).

const __months = [
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December",
];
const __days = [
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
];

// DateFormats/TimeFormats — verbatim from shared/scripting_utils.js.
const DateFormats = [
  "m/d", "m/d/yy", "mm/dd/yy", "mm/yy", "d-mmm", "d-mmm-yy", "dd-mmm-yy",
  "yy-mm-dd", "mmm-yy", "mmmm-yy", "mmm d, yyyy", "mmmm d, yyyy",
  "m/d/yy h:MM tt", "m/d/yy HH:MM",
];
const TimeFormats = ["HH:MM", "h:MM tt", "HH:MM:ss", "h:MM:ss tt"];

// ── util: printf ────────────────────────────────────────────────────────────
// Ported from util.js printf — Acrobat "%,N.Mf"-style formatting dialect.
function __printf(...args) {
  if (args.length === 0) {
    throw new Error("Invalid number of params in printf");
  }
  if (typeof args[0] !== "string") {
    throw new TypeError("First argument of printf must be a string");
  }

  const pattern = /%(,[0-4])?([+ 0#]+)?(\d+)?(\.\d+)?(.)/g;
  const PLUS = 1, SPACE = 2, ZERO = 4, HASH = 8;
  let i = 0;
  return args[0].replaceAll(
    pattern,
    function (_, nDecSep, cFlags, nWidth, nPrecision, cConvChar) {
      if (cConvChar !== "d" && cConvChar !== "f" && cConvChar !== "s" && cConvChar !== "x") {
        const buf = ["%"];
        for (const str of [nDecSep, cFlags, nWidth, nPrecision, cConvChar]) {
          if (str) buf.push(str);
        }
        return buf.join("");
      }

      i++;
      if (i === args.length) throw new Error("Not enough arguments in printf");
      const arg = args[i];

      if (cConvChar === "s") return arg.toString();

      let flags = 0;
      if (cFlags) {
        for (const flag of cFlags) {
          switch (flag) {
            case "+": flags |= PLUS; break;
            case " ": flags |= SPACE; break;
            case "0": flags |= ZERO; break;
            case "#": flags |= HASH; break;
          }
        }
      }
      cFlags = flags;

      nWidth &&= parseInt(nWidth);
      let intPart = Math.trunc(arg);

      if (cConvChar === "x") {
        let hex = Math.abs(intPart).toString(16).toUpperCase();
        if (nWidth !== undefined) {
          hex = hex.padStart(nWidth, cFlags & ZERO ? "0" : " ");
        }
        if (cFlags & HASH) hex = `0x${hex}`;
        return hex;
      }

      nPrecision &&= parseInt(nPrecision.substring(1));

      nDecSep = nDecSep ? nDecSep.substring(1) : "0";
      const separators = {
        0: [",", "."],
        1: ["", "."],
        2: [".", ","],
        3: ["", ","],
        4: ["'", "."],
      };
      const [thousandSep, decimalSep] = separators[nDecSep];

      let decPart = "";
      if (cConvChar === "f") {
        decPart =
          nPrecision !== undefined
            ? Math.abs(arg - intPart).toFixed(nPrecision)
            : Math.abs(arg - intPart).toString();
        if (decPart.length > 2) {
          if (/^1\.0+$/.test(decPart)) {
            intPart += Math.sign(arg);
            decPart = `${decimalSep}${decPart.split(".")[1]}`;
          } else {
            decPart = `${decimalSep}${decPart.substring(2)}`;
          }
        } else {
          if (decPart === "1") intPart += Math.sign(arg);
          decPart = cFlags & HASH ? "." : "";
        }
      }

      let sign = "";
      if (intPart < 0) {
        sign = "-";
        intPart = -intPart;
      } else if (cFlags & PLUS) {
        sign = "+";
      } else if (cFlags & SPACE) {
        sign = " ";
      }

      if (thousandSep && intPart >= 1000) {
        const buf = [];
        while (true) {
          buf.push((intPart % 1000).toString().padStart(3, "0"));
          intPart = Math.trunc(intPart / 1000);
          if (intPart < 1000) {
            buf.push(intPart.toString());
            break;
          }
        }
        intPart = buf.reverse().join(thousandSep);
      } else {
        intPart = intPart.toString();
      }

      let n = `${intPart}${decPart}`;
      if (nWidth !== undefined) {
        n = n.padStart(nWidth - sign.length, cFlags & ZERO ? "0" : " ");
      }

      return `${sign}${n}`;
    }
  );
}

// ── util: printd / scand ────────────────────────────────────────────────────
// Clock seam (design doc §4 "a clock seam for deterministic date tests"):
// __gpNowProvider() is the only source of "now" in the shim.
var __gpNowProvider = (globalThis.__gpNowProvider = function () {
  return new Date();
});
globalThis.__gpSetFixedNowFromIso = function (iso) {
  const fixed = new Date(iso);
  __gpNowProvider = () => new Date(fixed.getTime());
  return true;
};

function __printd(cFormat, oDate) {
  switch (cFormat) {
    case 0: return __printd("D:yyyymmddHHMMss", oDate);
    case 1: return __printd("yyyy.mm.dd HH:MM:ss", oDate);
    case 2: return __printd("m/d/yy h:MM:ss tt", oDate);
  }

  const handlers = {
    mmmm: data => __months[data.month],
    mmm: data => __months[data.month].substring(0, 3),
    mm: data => (data.month + 1).toString().padStart(2, "0"),
    m: data => (data.month + 1).toString(),
    dddd: data => __days[data.dayOfWeek],
    ddd: data => __days[data.dayOfWeek].substring(0, 3),
    dd: data => data.day.toString().padStart(2, "0"),
    d: data => data.day.toString(),
    yyyy: data => data.year.toString().padStart(4, "0"),
    yy: data => (data.year % 100).toString().padStart(2, "0"),
    HH: data => data.hours.toString().padStart(2, "0"),
    H: data => data.hours.toString(),
    hh: data => (1 + ((data.hours + 11) % 12)).toString().padStart(2, "0"),
    h: data => (1 + ((data.hours + 11) % 12)).toString(),
    MM: data => data.minutes.toString().padStart(2, "0"),
    M: data => data.minutes.toString(),
    ss: data => data.seconds.toString().padStart(2, "0"),
    s: data => data.seconds.toString(),
    tt: data => (data.hours < 12 ? "am" : "pm"),
    t: data => (data.hours < 12 ? "a" : "p"),
  };

  const data = {
    year: oDate.getFullYear(),
    month: oDate.getMonth(),
    day: oDate.getDate(),
    dayOfWeek: oDate.getDay(),
    hours: oDate.getHours(),
    minutes: oDate.getMinutes(),
    seconds: oDate.getSeconds(),
  };

  const patterns = /(mmmm|mmm|mm|m|dddd|ddd|dd|d|yyyy|yy|HH|H|hh|h|MM|M|ss|s|tt|t|\\.)/g;
  return cFormat.replaceAll(patterns, (_, pattern) =>
    pattern in handlers ? handlers[pattern](data) : pattern.charCodeAt(1)
  );
}

function __createDateActions(cFormat) {
  const actions = [];
  cFormat.replaceAll(
    /(d+)|(m+)|(y+)|(H+)|(M+)|(s+)/g,
    function (_, d, m, y, H, M, s) {
      if (d) {
        actions.push((n, data) => {
          if (n >= 1 && n <= 31) { data.day = n; return true; }
          return false;
        });
      } else if (m) {
        actions.push((n, data) => {
          if (n >= 1 && n <= 12) { data.month = n - 1; return true; }
          return false;
        });
      } else if (y) {
        actions.push((n, data) => {
          if (n < 50) n += 2000;
          else if (n < 100) n += 1900;
          data.year = n;
          return true;
        });
      } else if (H) {
        actions.push((n, data) => {
          if (n >= 0 && n <= 23) { data.hours = n; return true; }
          return false;
        });
      } else if (M) {
        actions.push((n, data) => {
          if (n >= 0 && n <= 59) { data.minutes = n; return true; }
          return false;
        });
      } else if (s) {
        actions.push((n, data) => {
          if (n >= 0 && n <= 59) { data.seconds = n; return true; }
          return false;
        });
      }
      return "";
    }
  );
  return actions;
}

function __tryToGuessDate(cFormat, cDate) {
  const actions = __createDateActions(cFormat);
  const number = /\d+/g;
  let i = 0;
  let array;
  const data = {
    year: __gpNowProvider().getFullYear(), // DEVIATION: through the clock seam
    month: 0,
    day: 1,
    hours: 12,
    minutes: 0,
    seconds: 0,
  };
  while ((array = number.exec(cDate)) !== null) {
    if (i < actions.length) {
      if (!actions[i++](parseInt(array[0]), data)) return null;
    } else {
      break;
    }
  }
  if (i === 0) return null;
  return new Date(data.year, data.month, data.day, data.hours, data.minutes, data.seconds);
}

function __createScandData(cFormat) {
  const handlers = {
    mmmm: {
      pattern: `(${__months.join("|")})`,
      action: (value, data) => { data.month = __months.indexOf(value); },
    },
    mmm: {
      pattern: `(${__months.map(m => m.substring(0, 3)).join("|")})`,
      action: (value, data) => {
        data.month = __months.findIndex(m => m.substring(0, 3) === value);
      },
    },
    mm: { pattern: "(\\d{2})", action: (value, data) => { data.month = parseInt(value) - 1; } },
    m:  { pattern: "(\\d{1,2})", action: (value, data) => { data.month = parseInt(value) - 1; } },
    dddd: {
      pattern: `(${__days.join("|")})`,
      action: (value, data) => { data.day = __days.indexOf(value); },
    },
    ddd: {
      pattern: `(${__days.map(d => d.substring(0, 3)).join("|")})`,
      action: (value, data) => {
        data.day = __days.findIndex(d => d.substring(0, 3) === value);
      },
    },
    dd: { pattern: "(\\d{2})", action: (value, data) => { data.day = parseInt(value); } },
    d:  { pattern: "(\\d{1,2})", action: (value, data) => { data.day = parseInt(value); } },
    yyyy: { pattern: "(\\d{4})", action: (value, data) => { data.year = parseInt(value); } },
    yy: { pattern: "(\\d{2})", action: (value, data) => { data.year = 2000 + parseInt(value); } },
    HH: { pattern: "(\\d{2})", action: (value, data) => { data.hours = parseInt(value); } },
    H:  { pattern: "(\\d{1,2})", action: (value, data) => { data.hours = parseInt(value); } },
    hh: { pattern: "(\\d{2})", action: (value, data) => { data.hours = parseInt(value); } },
    h:  { pattern: "(\\d{1,2})", action: (value, data) => { data.hours = parseInt(value); } },
    MM: { pattern: "(\\d{2})", action: (value, data) => { data.minutes = parseInt(value); } },
    M:  { pattern: "(\\d{1,2})", action: (value, data) => { data.minutes = parseInt(value); } },
    ss: { pattern: "(\\d{2})", action: (value, data) => { data.seconds = parseInt(value); } },
    s:  { pattern: "(\\d{1,2})", action: (value, data) => { data.seconds = parseInt(value); } },
    tt: {
      pattern: "([aApP][mM])",
      action: (value, data) => {
        const char = value.charAt(0);
        data.am = char === "a" || char === "A";
      },
    },
    t: {
      pattern: "([aApP])",
      action: (value, data) => { data.am = value === "a" || value === "A"; },
    },
  };

  const escapedFormat = cFormat.replaceAll(/[.*+\-?^${}()|[\]\\]/g, "\\$&");
  const patterns = /(mmmm|mmm|mm|m|dddd|ddd|dd|d|yyyy|yy|HH|H|hh|h|MM|M|ss|s|tt|t)/g;
  const actions = [];

  const re = escapedFormat.replaceAll(patterns, function (_, patternElement) {
    const { pattern, action } = handlers[patternElement];
    actions.push(action);
    return pattern.includes(",") ? `(?=${pattern})\\${actions.length}` : pattern;
  });

  return [new RegExp(`^${re}$`, "g"), actions];
}

function __scand(cFormat, cDate, strict = false) {
  if (typeof cDate !== "string") return new Date(cDate);
  if (cDate === "") return __gpNowProvider(); // DEVIATION: through the clock seam

  switch (cFormat) {
    case 0: return __scand("D:yyyymmddHHMMss", cDate);
    case 1: return __scand("yyyy.mm.dd HH:MM:ss", cDate);
    case 2: return __scand("m/d/yy h:MM:ss tt", cDate);
  }

  const [regex, actions] = __createScandData(cFormat);
  const matches = regex.exec(cDate);
  if (!matches || matches.length !== actions.length + 1) {
    return strict ? null : __tryToGuessDate(cFormat, cDate);
  }

  const data = {
    year: 2000,
    month: 0,
    day: 1,
    hours: 0,
    minutes: 0,
    seconds: 0,
    am: null,
  };
  actions.forEach((action, i) => action(matches[i + 1], data));
  if (data.am !== null) {
    data.hours = (data.hours % 12) + (data.am ? 0 : 12);
  }
  return new Date(data.year, data.month, data.day, data.hours, data.minutes, data.seconds);
}

// ── util object (Phase-1 subset: printf / printd / scand) ───────────────────
globalThis.util = {
  printf: __printf,
  printd: __printd,
  scand: (cFormat, cDate) => __scand(cFormat, cDate),
};

// ── color: minimal named tokens ─────────────────────────────────────────────
// pdf.js sets event.target.textColor = color.red for the red-negative number
// styles. Phase 1 records the color on the field proxy; rendering colored
// form text is a display-layer concern and is NOT implemented (disclosed in
// the properties panel). Tokens are plain arrays — enough for assignment and
// truthiness, deliberately not the full ColorConverters surface.
globalThis.color = {
  transparent: ["T"], black: ["G", 0], white: ["G", 1],
  red: ["RGB", 1, 0, 0], green: ["RGB", 0, 1, 0], blue: ["RGB", 0, 0, 1],
  cyan: ["RGB", 0, 1, 1], magenta: ["RGB", 1, 0, 1], yellow: ["RGB", 1, 1, 0],
  dkGray: ["G", 0.25], gray: ["G", 0.5], ltGray: ["G", 0.75],
};

// ── AF functions (aform.js port, Phase-1 tier) ──────────────────────────────
// pdf.js binds AForm methods with (document, app, util, color); the port uses
// the globals above so authored scripts see Acrobat-shaped free functions.

function __mkTargetName(event) {
  return event.target ? `[ ${event.target.name} ]` : "";
}

// Field access reads the host-provided snapshot map. hasOwnProperty is called
// on Object.prototype so attacker-chosen field names ("hasOwnProperty",
// "__proto__", ...) cannot confuse the lookup.
globalThis.getField = function (name) {
  const vals = globalThis.__gpFieldValues;
  if (!vals || typeof name !== "string" ||
      !Object.prototype.hasOwnProperty.call(vals, name)) {
    return null;
  }
  const value = vals[name];
  return {
    name,
    value,
    valueAsString: value === undefined || value === null ? "" : String(value),
    getArray() { return [{ name, value }]; },
  };
};

// Blocked egress verbs (design doc §3.1): hard no-op + audit entry, surfaced
// by the host after the run. Never executed, never silently dropped.
function __blockedVerb(name) {
  return function () {
    globalThis.__gpBlocked.push(name);
    return false;
  };
}

globalThis.__gpLog = [];
globalThis.__gpBlocked = [];

globalThis.app = {
  alert(message) { globalThis.__gpLog.push(String(message)); return true; },
  calculate: true,
  launchURL: __blockedVerb("app.launchURL"),
  mailme: __blockedVerb("app.mailme"),
  execDialog: __blockedVerb("app.execDialog"),
  media: __blockedVerb("app.media"),
};

// `doc` mirrors the top-level document object Acrobat provides. Top-level
// `this` is globalThis in sloppy scripts, so this.getField(...) resolves to
// the same getField — the common authored dialect works unchanged.
globalThis.doc = {
  getField: globalThis.getField,
  submitForm: __blockedVerb("doc.submitForm"),
  mailDoc: __blockedVerb("doc.mailDoc"),
  exportData: __blockedVerb("doc.exportData"),
};

globalThis.console = {
  println(...args) { globalThis.__gpLog.push(args.map(String).join(" ")); },
  show() {},
  clear() {},
};

function AFMakeNumber(str) {
  if (typeof str === "number") return str;
  if (typeof str !== "string") return null;

  str = str.trim().replace(",", ".");
  const number = parseFloat(str);
  if (isNaN(number) || !isFinite(number)) return null;
  return number;
}
globalThis.AFMakeNumber = AFMakeNumber;

globalThis.AFMakeArrayFromList = function (string) {
  return typeof string === "string" ? string.split(/, ?/g) : string;
};

globalThis.AFExtractNums = function (str) {
  if (typeof str === "number") return [str];
  if (!str || typeof str !== "string") return null;

  const first = str.charAt(0);
  if (first === "." || first === ",") str = `0${str}`;

  const numbers = str.match(/(\d+)/g);
  if (numbers.length === 0) return null;
  return numbers;
};

// DEVIATION: pdf.js delegates the !willCommit branch to the app's
// EventDispatcher (keystroke merging). Phase 1 events always commit.
function AFMergeChange(event = globalThis.event) {
  return event.willCommit ? event.value.toString() : String(event.change ?? "");
}
globalThis.AFMergeChange = AFMergeChange;

globalThis.AFParseDateEx = function (cString, cOrder) {
  return __parseDate(cOrder, cString);
};

function __parseDate(cFormat, cDate) {
  let date = null;
  try {
    date = __scand(cFormat, cDate, false);
  } catch {}
  if (date) return date;
  date = Date.parse(cDate);
  return isNaN(date) ? null : new Date(date);
}

globalThis.AFDate_FormatEx = function (cFormat) {
  const event = globalThis.event;
  const value = event.value;
  if (!value) return;

  const date = __parseDate(cFormat, value);
  if (date !== null) {
    event.value = __printd(cFormat, date);
  }
};

globalThis.AFDate_Format = function (pdf) {
  AFDate_FormatEx(DateFormats[pdf] ?? pdf);
};

globalThis.AFTime_FormatEx = function (cFormat) {
  AFDate_FormatEx(cFormat);
};
globalThis.AFTime_Format = function (pdf) {
  AFDate_FormatEx(TimeFormats[pdf] ?? pdf);
};

globalThis.AFNumber_Format = function (
  nDec, sepStyle, negStyle, currStyle /* unused */, strCurrency, bCurrencyPrepend
) {
  const event = globalThis.event;
  let value = AFMakeNumber(event.value);
  if (value === null) {
    event.value = "";
    return;
  }

  const sign = Math.sign(value);
  const buf = [];
  let hasParen = false;

  if (sign === -1 && bCurrencyPrepend && negStyle === 0) {
    buf.push("-");
  }

  if ((negStyle === 2 || negStyle === 3) && sign === -1) {
    buf.push("(");
    hasParen = true;
  }

  if (bCurrencyPrepend) buf.push(strCurrency);

  // sepStyle is an integer in [0;4]
  sepStyle = MathClamp(Math.floor(sepStyle), 0, 4);

  buf.push("%,", sepStyle, ".", nDec.toString(), "f");

  if (!bCurrencyPrepend) buf.push(strCurrency);

  if (hasParen) buf.push(")");

  if (negStyle === 1 || negStyle === 3) {
    if (event.target) {
      // Recorded on the field proxy; rendering red negatives is a display-layer
      // concern outside Phase 1 (see the color note above).
      event.target.textColor = sign === 1 ? globalThis.color.black : globalThis.color.red;
    }
  }

  if ((negStyle !== 0 || bCurrencyPrepend) && sign === -1) {
    value = -value;
  }

  const formatStr = buf.join("");
  event.value = __printf(formatStr, value);
};

// Authored for this port from the Acrobat JavaScript API reference
// (AFNumber_Parse(cString, nSepStyle) → number): strips a parenthesized or
// leading-minus sign, removes the nSepStyle thousands separator, maps the
// decimal separator, and drops any remaining non-numeric decoration
// (currency tokens). Returns null when nothing numeric remains.
globalThis.AFNumber_Parse = function (cString, nSepStyle) {
  if (typeof cString === "number") return cString;
  if (typeof cString !== "string") return null;

  let s = cString.trim();
  if (!s) return null;

  let neg = false;
  if (/^\(.*\)$/.test(s)) {
    neg = true;
    s = s.slice(1, -1).trim();
  }
  if (s.startsWith("-")) {
    neg = true;
    s = s.slice(1).trim();
  } else if (s.startsWith("+")) {
    s = s.slice(1).trim();
  }
  if (!s) return null;

  const separators = {
    0: [",", "."], 1: ["", "."], 2: [".", ","], 3: ["", ","], 4: ["'", "."],
  };
  const [thousandSep, decimalSep] =
    separators[MathClamp(Math.floor(Number(nSepStyle) || 0), 0, 4)];

  if (thousandSep) s = s.split(thousandSep).join("");
  if (decimalSep !== ".") s = s.split(decimalSep).join(".");
  s = s.replace(/[^0-9.\-]/g, "");

  const n = parseFloat(s);
  if (isNaN(n) || !isFinite(n)) return null;
  return neg ? -n : n;
};

globalThis.AFPercent_Format = function (nDec, sepStyle, percentPrepend = false) {
  if (typeof nDec !== "number") return;
  if (typeof sepStyle !== "number") return;
  if (nDec < 0) throw new Error("Invalid nDec value in AFPercent_Format");

  const event = globalThis.event;
  if (nDec > 512) {
    event.value = "%";
    return;
  }

  nDec = Math.floor(nDec);
  sepStyle = MathClamp(Math.floor(sepStyle), 0, 4);

  let value = AFMakeNumber(event.value);
  if (value === null) {
    event.value = "%";
    return;
  }

  const formatStr = `%,${sepStyle}.${nDec}f`;
  value = __printf(formatStr, value * 100);

  event.value = percentPrepend ? `%${value}` : `${value}%`;
};

globalThis.AFSimple = function (cFunction, nValue1, nValue2) {
  const value1 = AFMakeNumber(nValue1);
  if (value1 === null) throw new Error("Invalid nValue1 in AFSimple");

  const value2 = AFMakeNumber(nValue2);
  if (value2 === null) throw new Error("Invalid nValue2 in AFSimple");

  switch (cFunction) {
    case "AVG": return (value1 + value2) / 2;
    case "SUM": return value1 + value2;
    case "PRD": return value1 * value2;
    case "MIN": return Math.min(value1, value2);
    case "MAX": return Math.max(value1, value2);
  }
  throw new Error("Invalid cFunction in AFSimple");
};

globalThis.AFSimple_Calculate = function (cFunction, cFields) {
  const actions = {
    AVG: args => Math.sumPrecise(args) / args.length,
    SUM: args => Math.sumPrecise(args),
    PRD: args => args.reduce((acc, value) => acc * value, 1),
    MIN: args => Math.min(...args),
    MAX: args => Math.max(...args),
  };
  // Acrobat authoring UIs emit both the short operation names (SUM/PRD/...)
  // and long ones (PRODUCT/AVERAGE/MINIMUM/MAXIMUM); pdf.js only maps the
  // short ones. Accept both spellings (DEVIATION, documented).
  const alias = {
    PROD: "PRD", PRODUCT: "PRD", AVERAGE: "AVG", MINIMUM: "MIN", MAXIMUM: "MAX",
  };
  const op = Object.prototype.hasOwnProperty.call(alias, cFunction) ? alias[cFunction] : cFunction;
  if (!(op in actions)) throw new TypeError("Invalid function in AFSimple_Calculate");

  const event = globalThis.event;
  const values = [];

  cFields = globalThis.AFMakeArrayFromList(cFields);
  for (const cField of cFields) {
    const field = globalThis.getField(cField);
    if (!field) continue;
    for (const child of field.getArray()) {
      const number = AFMakeNumber(child.value);
      values.push(number ?? 0);
    }
  }

  if (values.length === 0) {
    event.value = 0;
    return;
  }

  const res = actions[op](values);
  event.value = Math.round(1e6 * res) / 1e6;
};

// ── Event lifecycle glue (host contract) ────────────────────────────────────
// The host evaluates __gpBeginEvent(<json>) before each authored script and
// __gpEndEvent() after it. The JSON round-trip keeps the C↔JS boundary to two
// plain calls with no native function objects inside the sandbox at all.
globalThis.__gpBeginEvent = function (setup) {
  globalThis.__gpLog = [];
  globalThis.__gpBlocked = [];
  const target = {
    name: setup.name,
    value: setup.value === null ? "" : setup.value,
  };
  const value = setup.value === null ? "" : setup.value;
  globalThis.event = {
    value,
    valueAsString: value === null ? "" : String(value),
    rc: true,
    willCommit: true,
    name: setup.eventKind,      // "Calculate" | "Format" (Acrobat event names)
    type: "field",
    target,
    source: target,             // Phase 1: /CO-driven; the source is the target
    targetName: `[ ${setup.name} ]`,
    // P2/P3 hooks (present in Acrobat's event object, intentionally ABSENT
    // here — Phase 2 events will add them with the event kinds that need
    // them): change, changeEx, commitKey, keyDown, modifier, selStart,
    // selEnd, shift, richChange/richValue.
  };
  return true;
};

globalThis.__gpEndEvent = function () {
  const ev = globalThis.event;
  const v = ev ? ev.value : undefined;
  const has = ev && typeof v !== "undefined" && v !== null;
  return JSON.stringify({
    hasValue: !!has,
    value: has ? (typeof v === "number" ? v : String(v)) : null,
    isNumber: has ? typeof v === "number" : false,
    rc: ev ? !!ev.rc : false,
    logs: globalThis.__gpLog,
    blocked: globalThis.__gpBlocked,
  });
};
)gpjs";
}

} // namespace gp::formjs
